#include "adguard_manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "redis_manager.hpp"
#include "license_manager.hpp"
#include "entertainment_manager.hpp"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <set>
#include <algorithm>
#include <unistd.h>
#include <fstream>

using json = nlohmann::json;

AdGuardManager& AdGuardManager::getInstance() {
    static AdGuardManager instance;
    return instance;
}

void AdGuardManager::load_config_credentials() {
    AppConfig config = Utils::Config::load_config();
    base_url = config.adguard_url;
    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }
    auth_name = config.adguard_user;
    auth_pass = config.adguard_password;
    
    if (base_url.empty()) {
        Utils::Logger::error("AdGuard URL is empty in config");
    } else {
        Utils::Logger::info("Loaded AdGuard URL: " + base_url + " (User: " + auth_name + ")");
    }
}

std::map<std::string, std::string> AdGuardManager::get_auth_headers() {
    return {
        {"X-Auth-User", auth_name},
        {"X-Auth-Pass", auth_pass}
    }; 
}

nlohmann::json AdGuardManager::get_agh_clients() {
    auto headers = get_auth_headers();
    std::string url = base_url + "/control/clients";
    
    auto res = Utils::http_get(url, headers, 10);
    if (res.status_code == 200) {
        try {
            json j = json::parse(res.body);
            if (j.contains("clients")) {
                return j["clients"];
            }
        } catch (...) {
            Utils::Logger::error("Failed to parse AGH clients JSON");
        }
    } else {
        Utils::Logger::error("Failed to fetching AGH clients. Status: " + std::to_string(res.status_code));
    }
    return json::array();
}

void AdGuardManager::sync_adguard_config() {
    load_config_credentials();
    if (base_url.empty()) return;
    
    // --- License Enforcement ---
    // If license expired, we force all clients to bypass filtering
    bool is_valid = LicenseManager::getInstance().is_license_valid();
    if (!is_valid) {
        Utils::Logger::warn("License expired. Forcing all AdGuard clients to bypass filtering.");
    }

    Utils::Logger::info("Starting AdGuard Config Sync (Redis-driven)...");
    
    // 1. Read full config from file
    json full_config = Utils::Config::read_json_file(CONFIG_FILE);
    if (!full_config.contains("ADGUARD")) {
        Utils::Logger::error("No ADGUARD section in config.");
        return;
    }
    
    // Original templates act as data source for policies (filtering, safe search etc)
    json original_templates = full_config["ADGUARD"].value("TEMPLATE", json::object());
    // New templates will only contain active groups from DB
    json updated_templates = json::object();
    
    DB& db = DB::getInstance();
    RedisManager& redis = RedisManager::getInstance();

    // --- Data Source Switch: Read from Redis instead of DB for device grouping ---
    std::string devices_json_str = redis.get("pc:devices");
    json all_devices = json::array();
    if (!devices_json_str.empty()) {
        try {
            all_devices = json::parse(devices_json_str);
            Utils::Logger::info("Loaded " + std::to_string(all_devices.size()) + " devices from Redis cache.");
        } catch (...) {
            Utils::Logger::error("Failed to parse pc:devices from Redis for AdGuard sync. Data: " + devices_json_str);
        }
    }

    // Fallback to DB if Redis is empty or failed
    if (all_devices.empty()) {
        Utils::Logger::warn("pc:devices in Redis is empty. Falling back to DB for device list...");
        auto db_devices = db.query("SELECT id, name, mac, groups_id FROM devices");
        for (const auto& row : db_devices) {
            json d;
            d["id"] = std::stoi(row.at("id"));
            d["name"] = row.at("name");
            d["mac"] = row.at("mac");
            d["groups_id"] = std::stoi(row.at("groups_id"));
            all_devices.push_back(d);
        }
        Utils::Logger::info("Loaded " + std::to_string(all_devices.size()) + " devices from DB fallback.");
    }

    // Headers with Auth
    auto headers = get_auth_headers();
    headers["Content-Type"] = "application/json";

    // 2. Fetch existing clients from AGH to decide Add vs Update
    json existing_clients_list = get_agh_clients();
    std::set<std::string> existing_names;
    for (const auto& c : existing_clients_list) {
        if (c.contains("name")) {
            existing_names.insert(c["name"].get<std::string>());
        }
    }

    // 3. --- Data Source Switch: Read groups from Redis instead of DB ---
    std::string groups_json_str = redis.get("pc:groups");
    json all_groups = json::array();
    if (!groups_json_str.empty()) {
        try {
            all_groups = json::parse(groups_json_str);
        } catch (...) {
            Utils::Logger::error("Failed to parse pc:groups from Redis for AdGuard sync. Data: " + groups_json_str);
        }
    }

    // Fallback to DB if Redis is empty (robustness)
    if (all_groups.empty()) {
        auto db_groups = db.query("SELECT id, name, adguard_client_key FROM groups");
        for (const auto& row : db_groups) {
            json g;
            g["id"] = std::stoi(row.at("id"));
            g["name"] = row.at("name");
            g["adguard_client_key"] = row.count("adguard_client_key") ? row.at("adguard_client_key") : "";
            all_groups.push_back(g);
        }
    }

    Utils::Logger::info("Found " + std::to_string(all_groups.size()) + " groups for AdGuard sync.");
    
    std::set<std::string> valid_client_keys;
    int success_count = 0;

    for (const auto& g : all_groups) {
        try {
            std::string client_key = g.value("adguard_client_key", "");
            if (client_key.empty()) continue;

            // Trim spaces just in case
            client_key.erase(0, client_key.find_first_not_of(" \t"));
            client_key.erase(client_key.find_last_not_of(" \t") + 1);
            if (client_key.empty()) continue;

            valid_client_keys.insert(client_key);

            int gid = 0;
            if (g["id"].is_number()) gid = g["id"].get<int>();
            else if (g["id"].is_string()) gid = std::stoi(g["id"].get<std::string>());
            
            std::string gname = g.value("name", "");
            Utils::Logger::info("Processing AdGuard Group: " + gname + " (Key: " + client_key + ", ID: " + std::to_string(gid) + ")");

            // A. Get policy template from base.conf
            json group_config;
            if (original_templates.contains(client_key)) {
                group_config = original_templates[client_key];
            } else {
                Utils::Logger::warn("No template found for key: " + client_key + ". Using default settings.");
                group_config = {
                    {"name", client_key},
                    {"filtering_enabled", true},
                    {"parental_enabled", false},
                    {"safebrowsing_enabled", false},
                    {"use_global_settings", true}
                };
            }

            // B. Rebuild IDs from Redis-sourced JSON list
            std::vector<std::string> new_mac_list;
            for (const auto& dev : all_devices) {
                if (!dev.contains("groups_id") || !dev.contains("mac")) continue;
                
                int dev_gid = 0;
                if (dev["groups_id"].is_number()) dev_gid = dev["groups_id"].get<int>();
                else if (dev["groups_id"].is_string()) dev_gid = std::stoi(dev["groups_id"].get<std::string>());

                if (dev_gid == gid) {
                    std::string m = dev["mac"].get<std::string>();
                    if (!m.empty()) {
                         std::transform(m.begin(), m.end(), m.begin(), ::toupper);
                         if (std::find(new_mac_list.begin(), new_mac_list.end(), m) == new_mac_list.end()) {
                             new_mac_list.push_back(m);
                         }
                    }
                }
            }
            
            group_config["ids"] = new_mac_list;
            group_config["name"] = client_key; // Ensure name matches key
            
            // --- License Enforcement: Override Filtering if Expired ---
            if (!is_valid) {
                group_config["filtering_enabled"] = false;
                group_config["parental_enabled"] = false;
                group_config["safebrowsing_enabled"] = false;
                group_config["safesearch_enabled"] = false;
            }

            // C. Sync to AdGuard Home
            std::string url;
            bool is_update = existing_names.count(client_key) > 0;
            json payload;

            if (is_update) {
                url = base_url + "/control/clients/update";
                payload["name"] = client_key;
                payload["data"] = group_config;
            } else {
                url = base_url + "/control/clients/add";
                payload = group_config;
            }
            
            auto res = Utils::http_post(url, payload.dump(), headers, 10);
            if (res.status_code == 200) {
                Utils::Logger::info("Synced AdGuard client (" + std::string(is_update ? "update" : "add") + "): " + client_key + " (" + std::to_string(new_mac_list.size()) + " devices)");
                success_count++;
            } else {
                Utils::Logger::error("Failed to sync AGH client: " + client_key + " | Status: " + std::to_string(res.status_code) + " | Msg: " + res.body);
            }

            // D. Keep for base.conf update
            updated_templates[client_key] = group_config;
        } catch (const std::exception& e) {
            Utils::Logger::error("Exception syncing AdGuard group: " + std::string(e.what()));
        } catch (...) {
            Utils::Logger::error("Unknown exception syncing AdGuard group");
        }
    }

    // 4. Cleanup: Delete clients from AGH that are no longer in DB groups
    for (const auto& existing_name : existing_names) {
        if (valid_client_keys.find(existing_name) == valid_client_keys.end()) {
            // This client exists in AGH but is not managed by our current groups
            Utils::Logger::info("Cleaning up stale AdGuard client: " + existing_name);
            std::string del_url = base_url + "/control/clients/delete";
            json del_payload;
            del_payload["name"] = existing_name;
            
            auto res = Utils::http_post(del_url, del_payload.dump(), headers, 10);
            if (res.status_code == 200) {
                Utils::Logger::info("Successfully deleted stale client: " + existing_name);
            } else {
                Utils::Logger::warn("Failed to delete stale client: " + existing_name + " | Status: " + std::to_string(res.status_code));
            }
        }
    }
    
    // 5. Update and save base.conf (Only keep active groups)
    full_config["ADGUARD"]["TEMPLATE"] = updated_templates;
    
    std::ofstream config_file(CONFIG_FILE);
    if (config_file.is_open()) {
        config_file << full_config.dump(4);
        config_file.close();
        Utils::Logger::info("Updated base.conf TEMPLATE section.");
    }
    
    Utils::Logger::info("AdGuard Sync Completed. Success: " + std::to_string(success_count));
}

void AdGuardManager::analyze_logs() {
    if (!LicenseManager::getInstance().is_license_valid()) {
        // We don't log warning here because it runs every minute
        return;
    }
    load_config_credentials();
    if (base_url.empty()) return;
    
    auto headers = get_auth_headers();
    std::string query_url = base_url + "/control/querylog?limit=5000";
    
    auto res = Utils::http_get(query_url, headers, 5);
    if (res.status_code != 200) {
        Utils::Logger::error("Failed to fetch AdGuard query log: " + std::to_string(res.status_code));
        return;
    }
    
    try {
        json log_data = json::parse(res.body);
        if (!log_data.contains("data")) return;

        DB& db = DB::getInstance();
        RedisManager& redis = RedisManager::getInstance();
        
        // --- Optimization: Pre-cache devices and groups to avoid SQL in loop ---
        struct DevMeta {
            std::string id;
            bool enable_video = false;
            bool enable_gaming = false;
        };
        std::map<std::string, DevMeta> device_cache; // Key: MAC (lowercase) or IP

        // 1. Get groups flags (now from Redis)
        std::string groups_json_str = redis.get("pc:groups");
        struct GroupMeta { bool video; bool gaming; };
        std::map<int, GroupMeta> group_map;
        
        if (!groups_json_str.empty()) {
            try {
                json all_groups = json::parse(groups_json_str);
                for (const auto& gr : all_groups) {
                    if (!gr.contains("id")) continue;
                    
                    bool v_det = false;
                    if (gr.contains("enable_video_detection")) {
                        auto& v = gr["enable_video_detection"];
                        if (v.is_boolean()) v_det = v.get<bool>();
                        else if (v.is_number()) v_det = (v.get<int>() == 1);
                    }
                    
                    bool g_det = false;
                    if (gr.contains("enable_gaming_detection")) {
                        auto& v = gr["enable_gaming_detection"];
                        if (v.is_boolean()) g_det = v.get<bool>();
                        else if (v.is_number()) g_det = (v.get<int>() == 1);
                    }

                    group_map[gr["id"].get<int>()] = { v_det, g_det };
                }
            } catch (...) {
                Utils::Logger::error("Failed to parse pc:groups in analyze_logs. Data: " + groups_json_str);
            }
        }

        // Fallback to DB if Redis groups is empty
        if (group_map.empty()) {
            auto groups_rows = db.query("SELECT id, enable_video_detection, enable_gaming_detection FROM groups");
            for (const auto& gr : groups_rows) {
                group_map[std::stoi(gr.at("id"))] = {
                    gr.at("enable_video_detection") == "1",
                    gr.at("enable_gaming_detection") == "1"
                };
            }
        }

        // 2. Get devices from Redis
        std::string devices_json_str = redis.get("pc:devices");
        if (!devices_json_str.empty()) {
            try {
                json all_devices = json::parse(devices_json_str);
                for (const auto& d : all_devices) {
                    if (!d.contains("id") || !d.contains("groups_id")) continue;
                    
                    DevMeta meta;
                    meta.id = std::to_string(d["id"].get<int>());
                    int gid = d["groups_id"].get<int>();
                    if (group_map.count(gid)) {
                        meta.enable_video = group_map[gid].video;
                        meta.enable_gaming = group_map[gid].gaming;
                    }

                    if (d.contains("mac")) {
                        std::string m = d["mac"].get<std::string>();
                        std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                        device_cache[m] = meta;
                    }
                    if (d.contains("ip") && !d["ip"].is_null()) {
                        device_cache[d["ip"].get<std::string>()] = meta;
                    }
                }
            } catch (...) {
                Utils::Logger::error("Failed to parse pc:devices in analyze_logs. Data: " + devices_json_str);
            }
        }

        // --- 增量同步逻辑 ---
        std::string last_sync_time = redis.get("pc:stats:last_log_time");
        std::string latest_time_in_batch = "";
        int new_logs_count = 0;
        
        for (const auto& entry : log_data["data"]) {
            std::string time_str = entry.value("time", "");
            if (time_str.empty()) continue;
            
            // 如果日志时间不晚于上次同步时间，则停止处理（因为 AGH 返回的是倒序，最新的在前）
            if (!last_sync_time.empty() && time_str <= last_sync_time) {
                break; 
            }
            
            // 记录本批次中最晚的时间
            if (latest_time_in_batch.empty() || time_str > latest_time_in_batch) {
                latest_time_in_batch = time_str;
            }
            
            std::string client = entry.value("client", "");
            std::string domain = entry.contains("question") ? entry["question"].value("name", "") : "";
            
            if (client.empty() || domain.empty()) continue;
            
            // Normalize client (if it's a MAC)
            std::string client_key = client;
            std::transform(client_key.begin(), client_key.end(), client_key.begin(), ::tolower);

            // Fast lookup in cache
            if (device_cache.find(client_key) == device_cache.end()) continue;
            
            new_logs_count++;
            const auto& dev_meta = device_cache[client_key];
            std::string device_id = dev_meta.id;
            bool enable_video = dev_meta.enable_video;
            bool enable_gaming = dev_meta.enable_gaming;
            
            // AGH specific fields
            int is_blocked = 0;
            std::string reason = "";
            if (entry.contains("reason")) {
                reason = entry.value("reason", "");
                if (reason.find("Filtered") == 0) {
                    is_blocked = 1;
                }
            }
            std::string qtype = entry.contains("question") ? entry["question"].value("type", "A") : "A";

            // --- Redis Real-time Sync & Aggregation ---
            std::string date_part = time_str.substr(0, 10);
            std::string detail_key = "pc:stats:detail:" + date_part + ":device:" + device_id;
            // 按 域名:类型:拦截状态 进行三轴聚合
            std::string field_base = domain + ":" + qtype + ":" + std::to_string(is_blocked);

            // 1. 详细维度聚合 (明细)
            redis.hincrby(detail_key, field_base + ":count", 1);
            redis.hsetnx(detail_key, field_base + ":begin", time_str);
            redis.hset(detail_key, field_base + ":end", time_str);
            redis.expire(detail_key, 172800); // 48 hours cleanup safety

            // 2. 基础数据 (仪表盘汇总)
            std::string stats_date_key = "pc:stats:date:" + date_part;
            redis.hincrby(stats_date_key, "total", 1);
            redis.hincrby(stats_date_key, "device:" + device_id + ":total", 1);
            if (is_blocked) {
                redis.hincrby(stats_date_key, "blocked", 1);
                redis.hincrby(stats_date_key, "device:" + device_id + ":blocked", 1);
            }
            redis.expire(stats_date_key, 172800); 

            // 2. Entertainment Detection
            std::string root = EntertainmentManager::getInstance().get_root_domain(domain);
            auto cat_res = EntertainmentManager::getInstance().auto_categorize(root);
            
            if (!cat_res.first.empty()) {
                bool is_video = (cat_res.first == "video");
                bool is_gaming = (cat_res.first == "gaming");
                
                // Skip if detection is disabled for this category in this group
                if (is_video && !enable_video) continue;
                if (is_gaming && !enable_gaming) continue;

                // We have an entertainment hit!
                // Increment counts in Redis for instant dashboard access
                std::string key = "pc:entertainment:date:" + date_part + ":device:" + device_id;
                redis.hincrby(key, root + ":count", 1);
                redis.hsetnx(key, root + ":begin", time_str);
                redis.hset(key, root + ":end", time_str);
                
                // Store category info if not present (optional, for fast lookup)
                redis.hset("pc:entertainment:categories", root, cat_res.first);
                redis.expire(key, 172800); // 48 hours cleanup safety (Consistent with stats)
            }
        }
        
        // 更新最后同步时间
        if (!latest_time_in_batch.empty()) {
            redis.set("pc:stats:last_log_time", latest_time_in_batch);
            Utils::Logger::info("AdGuard log sync completed: processed " + std::to_string(new_logs_count) + " new logs.");
        }
    } catch (...) {
        Utils::Logger::error("Error parsing AdGuard query logs");
    }
}

void AdGuardManager::collect_hourly_stats() {
    // Legacy hourly stats removed in favor of Redis aggregation
}

bool AdGuardManager::add_custom_filter_rule(const std::string& domain) {
    if (domain.empty()) return false;
    load_config_credentials();
    
    std::string rule = "||" + domain + "^";
    auto headers = get_auth_headers();
    
    Utils::Logger::info("Attempting to block domain: " + domain + " via " + base_url);

    // 1. Get existing rules
    auto res = Utils::http_get(base_url + "/control/filtering/status", headers, 5);
    if (res.status_code != 200) {
        Utils::Logger::error("Failed to get AdGuard filtering status. Status: " + std::to_string(res.status_code) + " | Error: " + res.error_msg);
        return false;
    }
    
    try {
        json j = json::parse(res.body);
        std::vector<std::string> rules;
        if (j.contains("user_rules")) {
            rules = j["user_rules"].get<std::vector<std::string>>();
        }
        
        // check exists
        for (const auto& r : rules) {
            if (r == rule) return true;
        }
        
        // add
        rules.push_back(rule);
        
        json payload;
        payload["rules"] = rules;
        
        headers["Content-Type"] = "application/json";
        auto post_res = Utils::http_post(base_url + "/control/filtering/set_rules", payload.dump(), headers, 5);
        if (post_res.status_code == 200) {
            Utils::Logger::info("Blocked domain: " + domain);
            return true;
        } else {
            Utils::Logger::error("Failed to set AdGuard rules. Status: " + std::to_string(post_res.status_code) + " | Body: " + post_res.body + " | Error: " + post_res.error_msg);
        }
    } catch (const std::exception& e) {
        Utils::Logger::error("Exception in add_custom_filter_rule: " + std::string(e.what()));
        return false;
    } catch (...) {
        Utils::Logger::error("Unknown exception in add_custom_filter_rule");
        return false;
    }
    return false;
}

bool AdGuardManager::remove_custom_filter_rule(const std::string& domain) {
    if (domain.empty()) return false;
    load_config_credentials();
    
    std::string rule = "||" + domain + "^";
    auto headers = get_auth_headers();
    
    auto res = Utils::http_get(base_url + "/control/filtering/status", headers, 5);
    if (res.status_code != 200) {
        Utils::Logger::error("Failed to get AdGuard filtering status for removal.");
        return false;
    }
    
    try {
        json j = json::parse(res.body);
        std::vector<std::string> rules;
        if (j.contains("user_rules")) {
            rules = j["user_rules"].get<std::vector<std::string>>();
        }
        
        auto it = std::find(rules.begin(), rules.end(), rule);
        if (it == rules.end()) return true; // Already removed
        
        rules.erase(it);
        
        json payload;
        payload["rules"] = rules;
        
        headers["Content-Type"] = "application/json";
        auto post_res = Utils::http_post(base_url + "/control/filtering/set_rules", payload.dump(), headers, 5);
        if (post_res.status_code == 200) {
            Utils::Logger::info("Successfully unblocked domain: " + domain);
            return true;
        }
    } catch (...) {
        Utils::Logger::error("Exception in remove_custom_filter_rule");
    }
    return false;
}

std::string AdGuardManager::get_all_custom_rules() {
    load_config_credentials();
    auto headers = get_auth_headers();
    
    auto res = Utils::http_get(base_url + "/control/filtering/status", headers, 5);
    if (res.status_code == 200) {
        try {
            json j = json::parse(res.body);
            if (j.contains("user_rules")) {
                return j["user_rules"].dump();
            }
        } catch (...) {}
    }
    return "[]";
}

void AdGuardManager::sync_aggregates_to_db(const std::string& date_str) {
    RedisManager& redis = RedisManager::getInstance();
    DB& db = DB::getInstance();
    
    Utils::Logger::info("Starting stats migration from Redis to SQLite for " + date_str);
    
    auto devices = db.query("SELECT id FROM devices");
    for (const auto& dev : devices) {
        std::string device_id = dev.at("id");
        std::string detail_key = "pc:stats:detail:" + date_str + ":device:" + device_id;
        
        auto stats = redis.hgetall(detail_key);
        if (stats.empty()) continue;
        
        struct AggData {
            long count = 0;
            std::string begin;
            std::string end;
        };
        std::map<std::string, AggData> agg_map;
        
        for (const auto& pair : stats) {
            std::string field = pair.first;
            std::string val = pair.second;
            
            size_t last_colon = field.find_last_of(':');
            if (last_colon == std::string::npos) continue;
            
            std::string base = field.substr(0, last_colon);
            std::string suffix = field.substr(last_colon + 1);
            
            if (suffix == "count") agg_map[base].count = std::stol(val);
            else if (suffix == "begin") agg_map[base].begin = val;
            else if (suffix == "end") agg_map[base].end = val;
        }

        if (agg_map.empty()) continue;

        // 1. 批量入库 adguard_device_logs
        db.exec("BEGIN TRANSACTION;");
        for (const auto& pair : agg_map) {
            std::string base = pair.first; // domain:qtype:is_blocked
            const auto& data = pair.second;
            
            std::vector<std::string> parts = Utils::split_string(base, ':');
            if (parts.size() < 3) continue;
            
            // Robust parsing: last is is_blocked, second to last is qtype, prefix is domain
            std::string is_blocked = parts.back();
            std::string qtype = parts[parts.size() - 2];
            
            std::string domain;
            for (size_t i = 0; i < parts.size() - 2; ++i) {
                if (i > 0) domain += ":";
                domain += parts[i];
            }
            
            std::string sql = "INSERT OR REPLACE INTO adguard_device_logs (device_id, domain, query_type, is_blocked, count, access_date, begin_at, end_at) VALUES (" + 
                              device_id + ", '" + Utils::escape_sql(domain) + "', '" + Utils::escape_sql(qtype) + "', " + is_blocked + ", " + std::to_string(data.count) + ", '" + date_str + "', '" + 
                              data.begin + "', '" + data.end + "')";
            db.exec(sql);
        }
        db.exec("COMMIT;");
        
        // 2. 聚合入库 adguard (汇总表)
        struct SummaryData {
            long total = 0;
            long blocked = 0;
        };
        std::map<std::string, SummaryData> summary;
        
        for (const auto& pair : agg_map) {
            std::vector<std::string> parts = Utils::split_string(pair.first, ':');
            if (parts.size() < 3) continue;
            
            std::string domain = parts[0];
            int is_blocked = std::stoi(parts[2]);
            std::string root = EntertainmentManager::getInstance().get_root_domain(domain);
            
            // 基础汇总数据
            summary[root].total += pair.second.count;
            if (is_blocked) {
                summary[root].blocked += pair.second.count;
            }
        }
        
        // 汇总落库
        db.exec("BEGIN TRANSACTION;");
        for (const auto& pair : summary) {
            std::string sql_sum = "INSERT OR REPLACE INTO adguard (stat_dt, device_id, domain, query_count, block_count) VALUES ('" + 
                                  date_str + "', " + device_id + ", '" + pair.first + "', " + std::to_string(pair.second.total) + ", " + std::to_string(pair.second.blocked) + ")";
            db.exec(sql_sum);
        }
        db.exec("COMMIT;");
        
        // 4. 清理 Redis
        redis.del(detail_key);
        Utils::Logger::info("Migrated device " + device_id + " basic stats to DB.");
    }
}

void AdGuardManager::run_daily_analysis() {
    // 1. 先进行一次实时日志抓取，确保 Redis 数据最新
    analyze_logs();
    
    // 2. 确定昨天的日期
    std::time_t t = std::time(nullptr);
    t -= 86400; // yesterday
    std::tm* tm_info = std::localtime(&t);
    char buf[12];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
    std::string yesterday(buf);
    
    // 3. 执行移库聚合逻辑
    sync_aggregates_to_db(yesterday);
}

int AdGuardManager::sync_logs_for_date(const std::string& date_str) {
    analyze_logs();
    sync_aggregates_to_db(date_str);
    return 0;
}

bool AdGuardManager::is_service_running() {
    std::string res = Utils::exec_command("pidof AdGuardHome");
    return !res.empty();
}

bool AdGuardManager::start_service() {
    Utils::Logger::info("Manually starting AdGuard Home service...");
    
    // Commands to try in order
    std::vector<std::string> cmds = {
        "systemctl start adguardhome",
        "systemctl start AdGuardHome",
        "/etc/init.d/adguardhome start",
        "/etc/init.d/AdGuardHome start"
    };

    for (const auto& cmd : cmds) {
        Utils::Logger::info("Trying to start with: " + cmd);
        Utils::exec_command(cmd.c_str());
        
        // Wait a bit and check if running
        usleep(500000); // 500ms
        if (is_service_running()) {
            Utils::Logger::info("Service started successfully via: " + cmd);
            return true;
        }
    }

    Utils::Logger::error("Failed to start AdGuard Home service after multiple attempts.");
    return false;
}

nlohmann::json AdGuardManager::get_protection_status() {
    load_config_credentials();
    auto headers = get_auth_headers();
    auto res = Utils::http_get(base_url + "/control/status", headers, 5);
    if (res.status_code == 200) {
        try {
            return json::parse(res.body);
        } catch (...) {
            Utils::Logger::error("Failed to parse AGH status JSON");
        }
    }
    return json::object();
}

bool AdGuardManager::set_protection(bool enabled, int duration_ms) {
    load_config_credentials();
    auto headers = get_auth_headers();
    headers["Content-Type"] = "application/json";
    
    json payload;
    payload["enabled"] = enabled;
    if (!enabled && duration_ms > 0) {
        payload["duration"] = (long long)duration_ms;
    }
    
    auto res = Utils::http_post(base_url + "/control/protection", payload.dump(), headers, 5);
    return (res.status_code == 200 || res.status_code == 204);
}
void AdGuardManager::refresh_home_stats_cache() {
    load_config_credentials();
    if (base_url.empty()) return;

    auto headers = get_auth_headers();
    std::string url = base_url + "/control/stats";
    
    auto res = Utils::http_get(url, headers, 5);
    if (res.status_code != 200) {
        Utils::Logger::error("Failed to fetch AdGuard stats for cache: " + std::to_string(res.status_code));
        return;
    }

    try {
        json raw_stats = json::parse(res.body);
        long total_queries = raw_stats.value("num_dns_queries", 0L);
        long blocked_queries = raw_stats.value("num_blocked_filtering", 0L);

        // Merge with Redis real-time data for "Today"
        RedisManager& redis = RedisManager::getInstance();
        std::string date_str = Utils::get_current_time_str().substr(0, 10);
        std::string stats_date_key = "pc:stats:date:" + date_str;
        auto redis_stats = redis.hgetall(stats_date_key);

        if (!redis_stats.empty()) {
            long r_total = std::stol(redis_stats.count("total") ? redis_stats.at("total") : "0");
            long r_blocked = std::stol(redis_stats.count("blocked") ? redis_stats.at("blocked") : "0");
            
            // If Redis data is more up-to-date (larger total), use it
            if (r_total > total_queries) {
                total_queries = r_total;
                blocked_queries = r_blocked;
            }
        }

        // Cache the unified summary to Redis for the Python API
        json summary;
        summary["total"] = total_queries;
        summary["blocked"] = blocked_queries;
        summary["updated_at"] = Utils::get_current_time_str();

        redis.set("pc:stats:home_summary", summary.dump(), 3600); // 1 hour TTL safety
        // Utils::Logger::info("Home stats cache refreshed: " + summary.dump());
    } catch (const std::exception& e) {
        Utils::Logger::error("Exception in refresh_home_stats_cache: " + std::string(e.what()));
    }
}

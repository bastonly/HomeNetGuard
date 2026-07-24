#include "nft_manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "redis_manager.hpp"
#include "license_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

// Static Member Definitions
const std::string NftManager::TABLE = "parent_control";
const std::string NftManager::LOCAL_IP = "192.168.0.1";

bool NftManager::is_table_exists() {
    std::string res = Utils::exec_command(("nft list table inet " + TABLE + " 2>/dev/null").c_str());
    return !res.empty() && res.find("Error") == std::string::npos;
}

static std::string get_safe_name(std::string name) {
    std::string safe = name;
    std::replace_if(safe.begin(), safe.end(), [](char c) {
        return !std::isalnum(c);
    }, '_');
    return safe;
}

bool NftManager::run_nft_cmd(const std::vector<std::string>& args) {
    std::string cmd = "nft";
    for (const auto& arg : args) {
        cmd += " " + arg;
    }
    Utils::exec_command(cmd.c_str());
    return true;
}

void NftManager::init_groups() {
    Utils::Logger::info("Initializing NFT Manager...");
    sync_firewall_state();
}

void NftManager::sync_firewall_state() {
    check_and_settle_timeouts();
    std::string now_str = Utils::get_current_time_str();
    std::string current_date = now_str.substr(0, 10);
    std::string current_time = now_str.substr(11, 5);
    std::time_t now = std::time(nullptr);
    
    DB& db = DB::getInstance();
    auto holiday = db.query("SELECT id FROM holidays WHERE '" + current_date + "' BETWEEN start_date AND end_date LIMIT 1");
    std::string day_type = holiday.empty() ? "workday" : "holiday";
    if (day_type == "workday") {
        std::tm* ltm = std::localtime(&now);
        if (ltm->tm_wday == 0 || ltm->tm_wday == 6) day_type = "weekend";
    }

    bool license_valid = LicenseManager::getInstance().is_license_valid();
    Utils::Logger::info("Syncing firewall: " + current_date + " " + current_time + " (" + day_type + ")");
    
    // --- Data Source: Direct DB Query to ensure 100% consistency ---
    auto all_groups_db = db.query("SELECT id, name, kind, allow_vpn, is_blocked, holiday_kind FROM groups");
    auto all_devices_db = db.query("SELECT id, mac, groups_id, hotpot FROM devices");

    std::map<std::string, std::string> mac_policy_map;
    std::set<std::string> no_hotspot_set;
    std::set<std::string> dns_accept_set;
    std::stringstream group_chains;

    RedisManager& redis = RedisManager::getInstance();
    std::string redis_key = "pc:device_group_map";
    redis.del(redis_key);

    for (const auto& g : all_groups_db) {
        int g_id = std::stoi(g.at("id"));
        std::string g_name = g.at("name");
        int g_kind = std::stoi(g.at("kind"));
        int allow_vpn = std::stoi(g.at("allow_vpn"));
        int is_blocked = std::stoi(g.at("is_blocked"));
        std::string holiday_kind = "period";
        if (g.find("holiday_kind") != g.end() && !g.at("holiday_kind").empty()) {
            holiday_kind = g.at("holiday_kind");
        }
        
        std::string q_chain = "ch_g" + std::to_string(g_id);
        
        for (const auto& dev : all_devices_db) {
            if (std::stoi(dev.at("groups_id")) != g_id) continue;

            std::string mac = dev.at("mac");
            if (mac.empty()) continue;
            
            mac.erase(0, mac.find_first_not_of(" \t\r\n"));
            mac.erase(mac.find_last_not_of(" \t\r\n") + 1);
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
            
            redis.hset(redis_key, mac, g_name);
            mac_policy_map[mac] = "jump " + q_chain;
            dns_accept_set.insert(mac);
            
            if (std::stoi(dev.at("hotpot")) == 0) {
                no_hotspot_set.insert(mac);
            }
        }

        group_chains << "    chain " << q_chain << " {\n";
        std::string upper_id = std::to_string(g_id);

        auto temps = db.query("SELECT id, end_at FROM temporary WHERE groups_id = " + std::to_string(g_id) + " AND " + std::to_string(now) + " BETWEEN from_at AND end_at ORDER BY id DESC LIMIT 1");
        
        if (!license_valid) {
            group_chains << "        accept\n";
        } else if (!temps.empty()) {
            group_chains << "        accept\n";
        } else if (is_blocked == 1) {
            group_chains << "        limit rate 2/minute burst 10 packets log prefix \"BLOCK_MANUAL_G" << upper_id << ":\"\n";
            group_chains << "        drop\n";
        } else if (g_kind == 1) {
            group_chains << "        ip daddr " << LOCAL_IP << " accept\n";
            group_chains << "        limit rate 2/minute burst 10 packets log prefix \"REJECT_TIMEOUT_TEMP:\"\n";
            group_chains << "        drop\n";
        } else if (g_kind == 3) {
            group_chains << "        accept\n";
        } else {
            if (allow_vpn == 0) {
                group_chains << "        udp dport 53 ip daddr != " << LOCAL_IP << " drop\n";
                group_chains << "        udp dport != { 53, 123, 3478-3479, 8000-8010, 8800-8899 } drop\n";
                group_chains << "        ip protocol { gre, esp, ah } drop\n";
                group_chains << "        tcp dport != { 80, 443, 8080 } drop\n";
            }
            bool is_allowed = false;
            if (day_type == "holiday") {
                if (holiday_kind == "duration") {
                    auto active_session = db.query("SELECT id FROM group_internet_sessions WHERE groups_id = " + std::to_string(g_id) + " AND date = '" + current_date + "' AND end_time IS NULL LIMIT 1");
                    if (!active_session.empty()) {
                        is_allowed = true;
                    }
                } else {
                    auto policies = db.query("SELECT id FROM policies WHERE groups_id = " + std::to_string(g_id) + " AND type = 'holiday' AND limit_type = 'period' AND is_enabled = 1 AND '" + current_time + "' BETWEEN from_at AND end_at");
                    if (!policies.empty()) {
                        is_allowed = true;
                    }
                }
            } else {
                auto policies = db.query("SELECT id FROM policies WHERE groups_id = " + std::to_string(g_id) + " AND type = '" + day_type + "' AND is_enabled = 1 AND '" + current_time + "' BETWEEN from_at AND end_at");
                if (!policies.empty()) {
                    is_allowed = true;
                }
            }

            if (is_allowed) {
                group_chains << "        accept\n";
            } else {
                group_chains << "        limit rate 2/minute burst 10 packets log prefix \"REJECT_TIMEOUT_G" << upper_id << ":\"\n";
                group_chains << "        drop\n";
            }
        }
        group_chains << "    }\n\n";
    }

    std::stringstream script;
    if (!is_table_exists()) {
        run_nft_cmd({"add", "table", "inet", TABLE});
    }

    script << "table inet " << TABLE << " {}\n";
    script << "delete table inet " << TABLE << "\n\n";
    script << "table inet " << TABLE << " {\n\n";

    script << "    map mac_policy {\n";
    script << "        type ether_addr : verdict\n";
    if (!mac_policy_map.empty()) {
        script << "        elements = {\n";
        size_t idx = 0;
        for (const auto& [mac, action] : mac_policy_map) {
            script << "            " << mac << " : " << action << (idx == mac_policy_map.size() - 1 ? "" : ",") << "\n";
            idx++;
        }
        script << "        }\n";
    }
    script << "    }\n\n";

    script << "    set no_hotspot {\n";
    script << "        type ether_addr\n";
    if (!no_hotspot_set.empty()) {
        script << "        elements = { ";
        size_t idx = 0;
        for (const auto& mac : no_hotspot_set) {
            script << mac << (idx == no_hotspot_set.size() - 1 ? "" : ", ");
            idx++;
        }
        script << " }\n";
    }
    script << "    }\n\n";

    script << "    chain filter_input {\n";
    script << "        type filter hook input priority filter; policy accept;\n\n";
    script << "        iifname \"lo\" accept\n\n";
        script << "        iifname \"br-lan\" udp dport 53 accept\n";
    script << "    }\n\n";

    script << "    chain filter_forward {\n";
    script << "        type filter hook forward priority filter; policy drop;\n\n";
    script << "        ct state established,related accept\n\n";
    script << "        iifname \"br-lan\" ether saddr @no_hotspot ip ttl set 64\n";
    script << "        iifname \"br-lan\" ether saddr @no_hotspot ip ttl <= 63 drop\n\n";
    script << "        iifname \"br-lan\" ether saddr vmap @mac_policy\n\n";
    script << "        iifname \"br-lan\" limit rate 2/minute burst 10 packets log prefix \"REJECT_UNAUTHORIZED:\" drop\n\n";
    script << "    }\n\n";

    script << group_chains.str();
    script << "}\n";

    system("mkdir -p /etc/nftables.d");
    std::string script_path = "/etc/nftables.d/pc.nft";
    std::ofstream ofs(script_path);
    if (ofs.is_open()) {
        ofs << script.str();
        ofs.close();
        run_nft_cmd({"-f", script_path});
    } else {
        Utils::Logger::error("Failed to write NFT sync script to " + script_path);
    }
}

std::string NftManager::get_groups_status() {
    using json = nlohmann::json;
    json result = json::array();
    DB& db = DB::getInstance();
    auto groups = db.query("SELECT id, name, remark, is_blocked, kind FROM groups");
    std::string json_output = Utils::exec_command(("nft -j list table inet " + TABLE).c_str());
    std::map<std::string, std::string> chain_verdicts;
    
    try {
        if (!json_output.empty() && json_output.find("Error") == std::string::npos) {
            json j = json::parse(json_output);
            if (j.contains("nftables")) {
                for (const auto& item : j["nftables"]) {
                    if (item.contains("rule")) {
                        const auto& rule = item["rule"];
                        std::string cname = rule.value("chain", "");
                        if (cname.empty()) continue;
                        if (rule.contains("expr")) {
                            for (const auto& expr : rule["expr"]) {
                                if (expr.contains("accept")) chain_verdicts[cname] = "online";
                                else if (expr.contains("drop") || expr.contains("reject")) chain_verdicts[cname] = "offline";
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {}

    for (auto& row : groups) {
        json g;
        g["id"] = std::stoi(row.at("id"));
        g["name"] = row.at("name");
        g["remark"] = row.at("remark");
        g["is_blocked"] = std::stoi(row.at("is_blocked"));
        g["kind"] = std::stoi(row.at("kind"));
        std::string chain_name = "ch_g" + row.at("id");
        g["status"] = chain_verdicts.count(chain_name) ? chain_verdicts[chain_name] : "offline";
        result.push_back(g);
    }
    
    std::string final_json = result.dump();
    RedisManager::getInstance().set("pc:groups_status", final_json, 60);
    return final_json;
}

// Stub implementation for other required methods if they exist in header
void NftManager::sync_authorized_pool() {}
bool NftManager::add_to_group(const std::string& group_name, const std::string& mac) { return true; }
void NftManager::remove_from_all_groups(const std::string& mac) {}

void NftManager::check_and_settle_timeouts() {
    DB& db = DB::getInstance();
    std::string now_str = Utils::get_current_time_str();
    std::string current_date = now_str.substr(0, 10);
    std::time_t now = std::time(nullptr);
    
    auto holiday = db.query("SELECT id FROM holidays WHERE '" + current_date + "' BETWEEN start_date AND end_date LIMIT 1");
    if (holiday.empty()) return;
    
    std::string sql_active = 
        "SELECT s.id, s.groups_id, s.start_time, p.duration_limit, g.name "
        "FROM group_internet_sessions s "
        "JOIN policies p ON s.groups_id = p.groups_id AND p.is_enabled = 1 "
        "JOIN groups g ON s.groups_id = g.id "
        "WHERE s.date = '" + current_date + "' AND s.end_time IS NULL AND p.type = 'holiday' AND p.limit_type = 'duration' AND g.holiday_kind = 'duration'";
    auto active_sessions = db.query(sql_active);
    
    for (const auto& s : active_sessions) {
        int session_id = std::stoi(s.at("id"));
        int g_id = std::stoi(s.at("groups_id"));
        long start_time = std::stol(s.at("start_time"));
        long duration_limit_min = std::stol(s.at("duration_limit"));
        long total_sec = duration_limit_min * 60;
        std::string g_name = s.at("name");
        
        std::string sql_closed = 
            "SELECT SUM(duration) as total_used "
            "FROM group_internet_sessions "
            "WHERE groups_id = " + std::to_string(g_id) + " AND date = '" + current_date + "' AND id != " + std::to_string(session_id);
        auto closed_res = db.query(sql_closed);
        long closed_sec = 0;
        if (!closed_res.empty() && !closed_res[0]["total_used"].empty()) {
            closed_sec = std::stol(closed_res[0]["total_used"]);
        }
        
        long active_sec = std::max(0L, static_cast<long>(now - start_time));
        long total_used = closed_sec + active_sec;
        
        if (total_used >= total_sec) {
            long remaining_quota = std::max(0L, total_sec - closed_sec);
            long actual_end_time = start_time + remaining_quota;
            if (actual_end_time > now) actual_end_time = now;
            long actual_duration = std::max(0L, actual_end_time - start_time);
            
            db.exec("UPDATE group_internet_sessions SET end_time = " + std::to_string(actual_end_time) + 
                    ", duration = " + std::to_string(actual_duration) + ", trigger_reason = 'timeout' WHERE id = " + std::to_string(session_id));
            
            std::string sql_log = 
                "INSERT INTO sys_messages (category, level, event_tag, group_name, content, created_at, updated_at) "
                "VALUES ('security', 'warning', 'TIMEOUT', '" + g_name + "', 'audit.duration_exhausted', datetime('now', 'localtime'), datetime('now', 'localtime'))";
            db.exec(sql_log);
            
            Utils::Logger::info("Group " + g_name + " holiday internet duration exhausted. Automatically disconnected.");
        }
    }
}

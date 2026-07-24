#include "log_monitor.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "common.hpp"
#include "redis_manager.hpp"
#include "license_manager.hpp"
#include "entertainment_manager.hpp"
#include <iostream>
#include <cstdio>
#include <memory>
#include <array>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

LogMonitor& LogMonitor::getInstance() {
    static LogMonitor instance;
    return instance;
}

LogMonitor::LogMonitor() : running(false) {
    // Regex: (REJECT_[A-Z_]+):.*MAC=([0-9a-fA-F:]+).*SRC=([\d\.]+)
    // Note: C++ regex syntax might slightly differ.
    // ".*" is greedy. 
    // Example log: ... kernel: [123.456] REJECT_DNS: IN=br-lan OUT=wan MAC=... SRC=192.168.1.10 ...
    try {
        log_pattern = std::regex(R"(((?:REJECT|BLOCK)_[^:]+):.*MAC=([0-9a-fA-F:]+).*SRC=([\d\.]+))");
    } catch (const std::regex_error& e) {
        Utils::Logger::error("Regex error: " + std::string(e.what()));
    }
}

LogMonitor::~LogMonitor() {
    stop();
}

void LogMonitor::start() {
    if (running) return;
    running = true;
    monitor_thread = std::thread(&LogMonitor::monitor_loop, this);
    Utils::Logger::info("LogMonitor started.");
}

void LogMonitor::stop() {
    if (!running) return;
    running = false;
    if (monitor_thread.joinable()) {
        // We cannot easily kill the popen read, so this might hang until next log line or we kill process.
        // For daemon, we usually just exit.
        // Detaching or using pthread_cancel (unsafe) are options.
        // Or we can use select() on fileno(pipe) with timeout.
        // For simplicity, we assume process exit cleans up.
        monitor_thread.detach(); 
    }
    Utils::Logger::info("LogMonitor stopped.");
}

void LogMonitor::monitor_loop() {
    // Open pipe to logread -f
    // OpenWrt uses logread. 
    // On Mac for testing we might need "tail -f /var/log/system.log" or no-op.
    const char* cmd = "logread -f";
    
    // Check if logread exists? Or assume target env.
    // We assume target env.
    
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        Utils::Logger::error("Failed to run logread -f");
        running = false;
        return;
    }
    
    char buffer[1024];
    while (running) {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            // Remove newline
            if (!line.empty() && line.back() == '\n') line.pop_back();
            
            if (line.find("REJECT_") != std::string::npos || line.find("BLOCK_") != std::string::npos) {
                parse_line(line);
            }
        } else {
            // EOF or error
            // Maybe logread died?
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // Re-open?
             // For now just break
             break;
        }
    }
    
    pclose(pipe);
}

void LogMonitor::parse_line(const std::string& line) {
    if (!LicenseManager::getInstance().is_license_valid()) return;
    
    std::smatch match;
    if (std::regex_search(line, match, log_pattern)) {
        if (match.size() >= 4) {
            std::string raw_prefix = match[1].str(); // REJECT_XXX
            std::string raw_mac = match[2].str();
            std::string src_ip = match[3].str();
            
            // Normalize MAC to lowercase for consistent comparison/lookup
            std::transform(raw_mac.begin(), raw_mac.end(), raw_mac.begin(), ::tolower);
            
            std::string src_mac = raw_mac;
            // Count colons
            int colons = std::count(raw_mac.begin(), raw_mac.end(), ':');
            if (colons >= 13) { 
                 // OpenWrt kernel log often includes destination MAC before source MAC.
                 // We extract the 6 segments representing the source MAC (segments 6-11).
                 std::stringstream ss(raw_mac);
                 std::string segment;
                 std::vector<std::string> parts;
                 while (std::getline(ss, segment, ':')) {
                     parts.push_back(segment);
                 }
                 
                 if (parts.size() >= 12) {
                     src_mac = parts[6] + ":" + parts[7] + ":" + parts[8] + ":" + parts[9] + ":" + parts[10] + ":" + parts[11];
                 }
            }
            
            std::string event_tag = "UNKNOWN";
            std::string group_name = get_device_group(src_mac);
            if (group_name.empty()) group_name = "N/A";

            if (raw_prefix.find("REJECT_TIMEOUT_") == 0) {
                event_tag = "TIMEOUT";
                if (group_name == "N/A") {
                    std::string gid_str = raw_prefix.substr(15);
                    if (!gid_str.empty() && gid_str.back() == ':') gid_str.pop_back();
                    if (gid_str.find('G') == 0) {
                        group_name = get_group_name_by_id(gid_str.substr(1));
                    } else {
                        group_name = gid_str;
                    }
                    if (group_name.empty()) group_name = gid_str;
                }
            } else if (raw_prefix.find("BLOCK_MANUAL_") == 0) {
                // Skip logging status-based manual blocks as requested.
                // Action is logged by Python backend during the button click.
                return; 
            }
 else if (raw_prefix.find("UNAUTHORIZED") != std::string::npos) {
                event_tag = "REJECT_UNAUTHORIZED";
            } else if (raw_prefix.find("TIMEOUT") != std::string::npos) {
                event_tag = "TIMEOUT";
            } else if (raw_prefix.find("DNS") != std::string::npos) {
                event_tag = "REJECT_DNS";
            } else if (raw_prefix.find("NO_POLICY") != std::string::npos) {
                event_tag = "REJECT_NO_POLICY";
            }
            
            save_log(event_tag, group_name, src_ip, src_mac, line);
        }
    }
}

std::string LogMonitor::get_device_group(const std::string& mac) {
    // 1. Try Redis first (populated during firewall sync)
    RedisManager& redis = RedisManager::getInstance();
    std::string group_name = redis.hget("pc:device_group_map", mac);
    if (!group_name.empty()) {
        return group_name;
    }

    // 2. Fallback to DB
    DB& db = DB::getInstance();
    std::string sql = "SELECT g.name FROM devices d JOIN groups g ON d.groups_id = g.id WHERE LOWER(d.mac) = LOWER('" + mac + "') LIMIT 1";
    auto res = db.query(sql);
    if (!res.empty()) {
        return res[0].at("name");
    }
    return "";
}

std::string LogMonitor::get_group_name_by_id(const std::string& id) {
    if (id.empty()) return "";
    DB& db = DB::getInstance();
    std::string sql = "SELECT name FROM groups WHERE id = " + id + " LIMIT 1";
    auto res = db.query(sql);
    if (!res.empty()) {
        return res[0].at("name");
    }
    return "";
}

void LogMonitor::save_log(const std::string& tag, const std::string& group, const std::string& ip, const std::string& mac, const std::string& raw) {
    // --- Deduplication Logic ---
    static std::map<std::string, time_t> last_event_time;
    static std::mutex dedup_mutex;
    
    std::string dedup_key = mac + ":" + tag;
    time_t now_ts = std::time(nullptr);
    
    {
        std::lock_guard<std::mutex> lock(dedup_mutex);
        if (last_event_time.count(dedup_key)) {
            if (now_ts - last_event_time[dedup_key] < 10) {
                // Skip DB write if same event from same MAC occurs within 10s
                return;
            }
        }
        last_event_time[dedup_key] = now_ts;
    }

    std::string now_str = Utils::get_current_time_str();
    
    std::string level = "info";
    std::string content = "cpp." + tag;
    
    if (tag.find("UNAUTHORIZED") != std::string::npos) {
        level = "critical";
    } else if (tag.find("DNS") != std::string::npos) {
        level = "warning";
    } else if (tag.find("TIMEOUT") != std::string::npos) {
        level = "warning";
    } else if (tag.find("BLOCK_MANUAL") != std::string::npos) {
        level = "warning";
    } else if (tag.find("NO_POLICY") != std::string::npos) {
        level = "warning";
    }
    
    // Make payload
    json j;
    j["group_name"] = group;
    j["original_tag"] = tag;
    // Truncate raw
    j["raw_log"] = raw.substr(0, 150);
    std::string payload = j.dump();
    
    DB& db = DB::getInstance();
    
    // Aggregation Logic in DB
    std::string sql_check = "SELECT id, occurrence_count FROM sys_messages WHERE source_ip='" + ip + "' AND content='" + content + "' AND is_read=0 ORDER BY updated_at DESC LIMIT 1";
    auto rows = db.query(sql_check);
    
    bool is_new_event = rows.empty();

    if (!is_new_event) {
        std::string id = rows[0].at("id");
        int count = std::stoi(rows[0].at("occurrence_count"));
        count++;
        db.exec("UPDATE sys_messages SET occurrence_count=" + std::to_string(count) + ", updated_at='" + now_str + "' WHERE id=" + id);
    } else {
        std::string category = EventCategory::SECURITY;
        
        std::string sql_ins = "INSERT INTO sys_messages (category, level, event_tag, source_ip, source_mac, group_name, content, payload, occurrence_count, is_read, created_at, updated_at) VALUES ('" + 
                              category + "', '" + level + "', '" + tag + "', '" + ip + "', '" + mac + "', '" + group + "', '" + content + "', '" + payload + "', 1, 0, '" + now_str + "', '" + now_str + "')";
        db.exec(sql_ins);
    }

    // --- Redis Real-time Stats ---
    // Update Redis stats
    RedisManager& redis = RedisManager::getInstance();
    std::string date_part = now_str.substr(0, 10); // YYYY-MM-DD
    
    redis.hincrby("pc:stats:date:" + date_part, "blocked", 1);
    redis.hincrby("pc:stats:date:" + date_part, "firewall_blocked", 1);
    
    redis.hincrby("pc:stats:date:" + date_part, "device:" + mac + ":blocked", 1);
    if (group != "N/A" && !group.empty()) {
        redis.hincrby("pc:stats:date:" + date_part, "group:" + group + ":blocked", 1);
    }
}

void LogMonitor::cleanup_old_data() {
    DB& db = DB::getInstance();
    AppConfig config = Utils::Config::load_config();
    int days = config.retention_sys_messages;
    if (days <= 0) days = 15; // Safeguard

    std::string sql = "DELETE FROM sys_messages WHERE created_at < date('now', '-" + std::to_string(days) + " days')";
    db.exec(sql);
    db.exec("VACUUM");
    Utils::Logger::info("Cleaned up old sys_messages (retention: " + std::to_string(days) + " days).");
}

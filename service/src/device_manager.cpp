#include "device_manager.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "nft_manager.hpp"

// OpenWrt dnsmasq lease file format:
// timestamp mac ip hostname client-id

std::map<std::string, DeviceManager::LeaseInfo> DeviceManager::load_leases() {
    std::map<std::string, LeaseInfo> leases;
    std::ifstream file(LEASE_FILE); // defined in common.hpp
    
    if (!file.is_open()) {
        Utils::Logger::warn("Could not open lease file: " + LEASE_FILE);
        return leases;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string ts, mac, ip, name;
        if (ss >> ts >> mac >> ip >> name) {
            // Normalize MAC to lowercase just in case
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
            
            // Filter out unknown or * names if needed, but python script kept them
            leases[mac] = {ip, name};
        }
    }
    return leases;
}

#include "redis_manager.hpp"
#include <nlohmann/json.hpp>

void DeviceManager::update_online_status() {
    auto leases = load_leases();
    int now = std::time(nullptr);
    
    DB& db = DB::getInstance();
    RedisManager& redis = RedisManager::getInstance();
    
    bool new_device_added = false;

    // 1. Clear online status in Redis
    redis.del("pc:online_macs");

    // 2. Sync Leases to Redis (Hash: pc:dhcp_leases for raw info)
    redis.del("pc:dhcp_leases");


    // 3. Update online devices
    for (const auto& [mac, info] : leases) {
        // Mark as online in Redis
        redis.sadd("pc:online_macs", mac);

        // Check if device exists in DB
        std::string query_sql = "SELECT id, groups_id FROM devices WHERE mac='" + mac + "'";
        auto rows = db.query(query_sql);

        if (!rows.empty()) {
            // Update existing (IP and Last Seen only)
            std::string update_sql = "UPDATE devices SET ip='" + info.ip + 
                                     "', last_seen=" + std::to_string(now) + 
                                     " WHERE mac='" + mac + "'";
            db.exec(update_sql);
        } else {
            // Insert new device
            std::string name = info.name;
            if (name.empty() || name == "*") name = "Unknown-" + mac.substr(std::max(0, (int)mac.size() - 5));

            // Get real grouping id for kind = 1 (临时组)
            std::string temp_group_id = "1";
            auto g_res = db.query("SELECT id FROM groups WHERE kind = 1 LIMIT 1");
            if (!g_res.empty()) {
                temp_group_id = g_res[0].at("id");
            }

            std::string insert_sql = "INSERT INTO devices (name, mac, remark, status, last_seen, ip, groups_id) "
                                     "VALUES ('" + name + "', '" + mac + "', '" + name + "', 'offline', " + 
                                     std::to_string(now) + ", '" + info.ip + "', " + temp_group_id + ")";
            db.exec(insert_sql);

            new_device_added = true;
            Utils::Logger::info("New device discovered: " + mac + " (" + name + ")");
            
            // Trigger email notification via Python script
            std::string mail_cmd = "python3 /opt/parent-control/backend/scripts/notify_new_device.py '" + mac + "' '" + name + "' '" + info.ip + "' &";
            system(mail_cmd.c_str());
        }

    }

    if (new_device_added) {
        Utils::Logger::info("New device(s) added. Triggering firewall sync to map them into temp group.");
        NftManager::sync_firewall_state();
    }

    // 4. Aggregate all devices to a single JSON list for backend efficiency
    try {
        auto all_devices = db.query("SELECT id, name, mac, remark, status, last_seen, ip, groups_id, type, hotpot FROM devices");
        nlohmann::json device_list = nlohmann::json::array();
        
        for (const auto& row : all_devices) {
            nlohmann::json d;
            d["id"] = row.count("id") ? std::stoi(row.at("id")) : 0;
            d["name"] = row.count("name") ? row.at("name") : "";
            d["mac"] = row.count("mac") ? row.at("mac") : "";
            d["remark"] = row.count("remark") ? row.at("remark") : "";
            d["ip"] = row.count("ip") ? row.at("ip") : "";
            d["groups_id"] = row.count("groups_id") ? std::stoi(row.at("groups_id")) : 0;
            d["type"] = row.count("type") ? row.at("type") : "";
            d["hotpot"] = row.count("hotpot") ? std::stoi(row.at("hotpot")) : 0;
            d["last_seen"] = row.count("last_seen") ? std::stoll(row.at("last_seen")) : 0;
            device_list.push_back(d);
        }
        
        redis.set("pc:devices", device_list.dump());
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to cache all devices to Redis: " + std::string(e.what()));
    }
}

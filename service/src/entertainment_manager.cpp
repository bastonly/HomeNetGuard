#include "entertainment_manager.hpp"
#include "redis_manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

EntertainmentManager& EntertainmentManager::getInstance() {
    static EntertainmentManager instance;
    return instance;
}

EntertainmentManager::EntertainmentManager() {
    // 1. Video / Social
    builtin_categories["youtube.com"] = {"video", "YouTube"};
    builtin_categories["youtu.be"] = {"video", "YouTube"};
    builtin_categories["m.youtube.com"] = {"video", "YouTube Mobile"};
    builtin_categories["tiktok.com"] = {"video", "TikTok"};
    builtin_categories["instagram.com"] = {"video", "Instagram"};
    builtin_categories["snapchat.com"] = {"video", "Snapchat"};
    builtin_categories["netflix.com"] = {"video", "Netflix"};
    builtin_categories["twitch.tv"] = {"video", "Twitch"};
    builtin_categories["discord.com"] = {"video", "Discord"};
    builtin_categories["hulu.com"] = {"video", "Hulu"};
    builtin_categories["disneyplus.com"] = {"video", "Disney+"};
    builtin_categories["disney.com"] = {"video", "Disney"};
    builtin_categories["max.com"] = {"video", "HBO Max"};
    builtin_categories["primevideo.com"] = {"video", "Prime Video"};
    builtin_categories["reddit.com"] = {"video", "Reddit"};
    builtin_categories["whatsapp.com"] = {"video", "WhatsApp"};
    builtin_categories["vimeo.com"] = {"video", "Vimeo"};
    builtin_categories["dailymotion.com"] = {"video", "Dailymotion"};

    // 2. Gaming
    builtin_categories["roblox.com"] = {"gaming", "Roblox"};
    builtin_categories["fortnite.com"] = {"gaming", "Fortnite"};
    builtin_categories["epicgames.com"] = {"gaming", "Epic Games"};
    builtin_categories["minecraft.net"] = {"gaming", "Minecraft"};
    builtin_categories["mojang.com"] = {"gaming", "Mojang"};
    builtin_categories["steampowered.com"] = {"gaming", "Steam"};
    builtin_categories["steamcommunity.com"] = {"gaming", "Steam Community"};
    builtin_categories["ea.com"] = {"gaming", "EA Sports"};
    builtin_categories["callofduty.com"] = {"gaming", "Call of Duty"};
    builtin_categories["activision.com"] = {"gaming", "Activision"};
    builtin_categories["leagueoflegends.com"] = {"gaming", "League of Legends"};
    builtin_categories["riot.com"] = {"gaming", "Riot Games"};
    builtin_categories["playstation.com"] = {"gaming", "PlayStation"};
    builtin_categories["xbox.com"] = {"gaming", "Xbox"};
    builtin_categories["nintendo.com"] = {"gaming", "Nintendo"};
    builtin_categories["blizzard.com"] = {"gaming", "Blizzard"};
    builtin_categories["valorant.com"] = {"gaming", "Valorant"};
    builtin_categories["among-us.io"] = {"gaming", "Among Us"};
    builtin_categories["innersloth.com"] = {"gaming", "Among Us"};
    builtin_categories["hoyoverse.com"] = {"gaming", "Genshin Impact"};
    builtin_categories["rockstargames.com"] = {"gaming", "Rockstar Games"};
    builtin_categories["2k.com"] = {"gaming", "2K Sports"};
    builtin_categories["ubisoft.com"] = {"gaming", "Ubisoft"};
    builtin_categories["supercell.com"] = {"gaming", "Supercell"};
    builtin_categories["king.com"] = {"gaming", "King (Candy Crush)"};
    builtin_categories["pokemongo.com"] = {"gaming", "Pokémon GO"};
    builtin_categories["pokemon.com"] = {"gaming", "Pokémon"};

    // Keywords
    video_keywords = {"video", "tube", "tv", "movie", "film", "stream", "live", "watch", "视频", "直播", 
                      "iqiyi", "youku", "mgtv", "bilibili", "tencent", "ixigua", "youtube", "netflix", 
                      "vimeo", "douyin", "kuaishou", "tiktok"};
    gaming_keywords = {"game", "play", "gaming", "esport", "游戏", "玩", 
                       "steam", "origin", "uplay", "epicgames", "blizzard", "mihoyo", "unity",
                       "fortnite", "leagueoflegends", "pubg", "genshin", "hoyoverse"};
}

void EntertainmentManager::ensure_builtin_categories() {
    Utils::Logger::info("Ensuring built-in categories...");
    DB& db = DB::getInstance();
    
    // Get existing builtins
    auto existing = db.query("SELECT domain FROM entertainment_categories WHERE source = 'builtin'");
    std::set<std::string> existing_set;
    for (const auto& row : existing) {
        existing_set.insert(row.at("domain"));
    }
    
    int added = 0;
    std::string now_str = Utils::get_current_time_str();
    
    for (const auto& [domain, info] : builtin_categories) {
        if (existing_set.find(domain) == existing_set.end()) {
            // Insert
            std::string sql = "INSERT OR IGNORE INTO entertainment_categories (domain, category, name, source, confidence, last_verified) VALUES ('" + 
                              domain + "', '" + info.category + "', '" + info.name + "', 'builtin', 1.0, '" + now_str + "')";
            db.exec(sql);
            added++;
        }
    }
    
    if (added > 0) {
        Utils::Logger::info("Added " + std::to_string(added) + " built-in categories.");
    }
}

std::string EntertainmentManager::get_root_domain(const std::string& domain) {
    // Simple tld extract: take last 2 parts. 
    // real tld list is huge. 
    // python code used tldextract library.
    // For C++, we can try a simple heuristic:
    // if ends with .xx.xx (like .co.uk), take last 3? 
    // For now, let's just take last 2 parts unless it's an IP.
    
    size_t last_dot = domain.rfind('.');
    if (last_dot == std::string::npos) return domain;
    
    size_t second_last = domain.rfind('.', last_dot - 1);
    if (second_last == std::string::npos) return domain;
    
    // Check if second_last part is short (len <= 3) and it's not a common gTLD?
    // This is getting complicated. Let's stick to last 2 for .com, .net etc.
    // Ideally we should import public suffix list.
    // But for this project, let's assume standard domains.
    
    return domain.substr(second_last + 1);
}

std::pair<std::string, double> EntertainmentManager::auto_categorize(const std::string& domain) {
    DB& db = DB::getInstance();
    // 1. Check DB
    auto res = db.query("SELECT category, confidence FROM entertainment_categories WHERE domain = '" + domain + "'");
    if (!res.empty()) {
        double conf = 0.0;
        try { conf = std::stod(res[0].at("confidence")); } catch(...) {}
        return {res[0].at("category"), conf};
    }
    
    // 2. check builtin map (in case db missed it or partial match?)
    if (builtin_categories.count(domain)) {
        return {builtin_categories[domain].category, 1.0};
    }
    
    // 3. Keywords
    std::string domain_lower = domain;
    std::transform(domain_lower.begin(), domain_lower.end(), domain_lower.begin(), ::tolower);
    
    std::string now_str = Utils::get_current_time_str();

    for (const auto& kw : video_keywords) {
        if (domain_lower.find(kw) != std::string::npos) {
            // Auto add
            std::string sql = "INSERT OR IGNORE INTO entertainment_categories (domain, category, name, source, confidence, last_verified) VALUES ('" + 
                              domain + "', 'video', '" + domain + "', 'auto', 0.7, '" + now_str + "')";
            db.exec(sql);
            return {"video", 0.7};
        }
    }
    
    for (const auto& kw : gaming_keywords) {
        if (domain_lower.find(kw) != std::string::npos) {
             std::string sql = "INSERT OR IGNORE INTO entertainment_categories (domain, category, name, source, confidence, last_verified) VALUES ('" + 
                              domain + "', 'gaming', '" + domain + "', 'auto', 0.7, '" + now_str + "')";
            db.exec(sql);
            return {"gaming", 0.7};
        }
    }
    
    return {"", 0.0};
}

void EntertainmentManager::detect_for_date(std::string target_date) {
    if (target_date.empty()) {
        std::time_t now = std::time(nullptr);
        now -= 24 * 60 * 60;
        std::tm* ltm = std::localtime(&now);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", ltm);
        target_date = buf;
    }
    
    Utils::Logger::info("Detecting entertainment for " + target_date);
    
    DB& db = DB::getInstance();
    
    // Get groups with detection enabled
    auto groups = db.query("SELECT id, enable_video_detection, enable_gaming_detection FROM groups WHERE enable_video_detection = 1 OR enable_gaming_detection = 1");
    if (groups.empty()) return;
    
    int total = 0;
    
    for (const auto& g : groups) {
        std::string gid = g.at("id");
        bool enable_video = (g.at("enable_video_detection") == "1");
        bool enable_gaming = (g.at("enable_gaming_detection") == "1");
        
        // Get devices
        auto devices = db.query("SELECT id FROM devices WHERE groups_id = " + gid);
        for (const auto& d : devices) {
            std::string did = d.at("id");
            
            // Query aggregated logs for the device/date
            std::string sql_logs = "SELECT domain, count, begin_at, end_at "
                                   "FROM adguard_device_logs WHERE device_id = " + did + 
                                   " AND access_date = '" + target_date + "'";
            
            auto logs = db.query(sql_logs);
            
            struct SessionInfo {
                int visit_count = 0;
                std::string first_seen;
                std::string last_seen;
                std::string category;
            };
            std::map<std::string, SessionInfo> root_sessions;

            for (const auto& log : logs) {
                std::string domain = log.at("domain");
                std::string root = get_root_domain(domain);
                int row_count = std::stoi(log.at("count"));
                std::string begin_at = log.at("begin_at");
                std::string end_at = log.at("end_at");
                
                auto cat_res = auto_categorize(root);
                std::string category = cat_res.first;
                
                if (category.empty()) continue;
                if (category == "video" && !enable_video) continue;
                if (category == "gaming" && !enable_gaming) continue;

                if (root_sessions.find(root) == root_sessions.end()) {
                    root_sessions[root] = {row_count, begin_at, end_at, category};
                } else {
                    auto& session = root_sessions[root];
                    session.visit_count += row_count;
                    if (begin_at < session.first_seen) session.first_seen = begin_at;
                    if (end_at > session.last_seen) session.last_seen = end_at;
                    if (session.category.empty()) session.category = category;
                }
            }

            // Insert aggregated detections
            db.exec("BEGIN TRANSACTION;");
            for (const auto& [root, session] : root_sessions) {
                std::string sql_ins = "INSERT OR REPLACE INTO entertainment_detections "
                                      "(device_id, domain, category, visit_count, first_seen, last_seen, detection_date) "
                                      "VALUES (" + did + ", '" + root + "', '" + session.category + "', " + 
                                      std::to_string(session.visit_count) + ", '" + session.first_seen + "', '" + 
                                      session.last_seen + "', '" + target_date + "')";
                db.exec(sql_ins);
                total++;
            }
            db.exec("COMMIT;");
        }
    }
    
    Utils::Logger::info("Detection complete. Found " + std::to_string(total) + " items.");
}
void EntertainmentManager::sync_to_db(const std::string& date_str) {
    RedisManager& redis = RedisManager::getInstance();
    DB& db = DB::getInstance();
    
    Utils::Logger::info("Starting entertainment stats migration from Redis to SQLite for " + date_str);
    
    // 获取分类缓存
    auto cats_cache = redis.hgetall("pc:entertainment:categories");
    
    // Get devices and their group detection flags
    auto devices = db.query("SELECT d.id, g.enable_video_detection, g.enable_gaming_detection "
                           "FROM devices d JOIN groups g ON d.groups_id = g.id");
    
    db.exec("BEGIN TRANSACTION;");
    for (const auto& dev : devices) {
        std::string device_id = dev.at("id");
        bool enable_video = (dev.at("enable_video_detection") == "1");
        bool enable_gaming = (dev.at("enable_gaming_detection") == "1");
        
        std::string redis_key = "pc:entertainment:date:" + date_str + ":device:" + device_id;
        auto stats = redis.hgetall(redis_key);
        if (stats.empty()) continue;

        struct AggEnt {
            long count = 0;
            std::string begin;
            std::string end;
        };
        std::map<std::string, AggEnt> ent_map;
        for (const auto& pair : stats) {
            std::string field = pair.first;
            std::string val = pair.second;
            size_t last_colon = field.find_last_of(':');
            if (last_colon == std::string::npos) continue;
            
            std::string root = field.substr(0, last_colon);
            std::string suffix = field.substr(last_colon + 1);
            if (suffix == "count") ent_map[root].count = std::stol(val);
            else if (suffix == "begin") ent_map[root].begin = val;
            else if (suffix == "end") ent_map[root].end = val;
        }

        for (const auto& pair : ent_map) {
            std::string root = pair.first;
            const auto& data = pair.second;
            std::string category = cats_cache.count(root) ? cats_cache.at(root) : "video";
            
            if (category == "video" && !enable_video) continue;
            if (category == "gaming" && !enable_gaming) continue;
            
            std::string sql = "INSERT INTO entertainment_detections (device_id, domain, category, visit_count, first_seen, last_seen, detection_date) VALUES (" +
                              device_id + ", '" + root + "', '" + category + "', " + std::to_string(data.count) + ", '" +
                              data.begin + "', '" + data.end + "', '" + date_str + "') " +
                              "ON CONFLICT(device_id, domain, detection_date) DO UPDATE SET " +
                              "visit_count = visit_count + excluded.visit_count, " +
                              "first_seen = CASE WHEN excluded.first_seen < first_seen THEN excluded.first_seen ELSE first_seen END, " +
                              "last_seen = CASE WHEN excluded.last_seen > last_seen THEN excluded.last_seen ELSE last_seen END";
            db.exec(sql);
        }
        
        // 清理该设备的 Redis 缓存
        redis.del(redis_key);
    }
    db.exec("COMMIT;");
    Utils::Logger::info("Entertainment stats migration completed for " + date_str);
}

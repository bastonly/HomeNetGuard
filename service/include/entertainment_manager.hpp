#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

struct CategoryInfo {
    std::string category;
    std::string name;
};

class EntertainmentManager {
public:
    static EntertainmentManager& getInstance();
    
    // Ensures built-in categories are in DB
    void ensure_builtin_categories();
    
    // Analyzes logs for a specific date (default yesterday if empty)
    void detect_for_date(std::string target_date = "");
    
    // Sync pre-aggregated real-time stats from Redis to DB (New optimized path)
    void sync_to_db(const std::string& date_str);
    
    // Helpers used by AdGuardManager for real-time sync
    std::pair<std::string, double> auto_categorize(const std::string& domain);
    std::string get_root_domain(const std::string& domain);
    
private:
    EntertainmentManager();
    
    // Domain -> {category, name}
    std::map<std::string, CategoryInfo> builtin_categories;
    std::vector<std::string> video_keywords;
    std::vector<std::string> gaming_keywords;
};

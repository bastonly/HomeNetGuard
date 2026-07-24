#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

class AdGuardManager {
public:
    static AdGuardManager& getInstance();
    
    // Syncs local group/device config to AdGuard Home
    void sync_adguard_config();
    
    // Pulls logs from AdGuard Home and saves to local DB
    void analyze_logs();
    void sync_aggregates_to_db(const std::string& date_str);
    // Pulls logs from AdGuard Home and saves to local DB (Daily job)
    void run_daily_analysis();

    // Refresh and cache home summary stats to Redis
    void refresh_home_stats_cache();

    // Helper to sync logs for a specific date (exposed for manual trigger)
    int sync_logs_for_date(const std::string& date_str);

    // Manage custom filtering rules (block/unblock domain)
    bool add_custom_filter_rule(const std::string& domain);
    bool remove_custom_filter_rule(const std::string& domain);
    
    // Get all custom filtering rules (returns JSON string of list)
    std::string get_all_custom_rules();

    // --- New Status and Control Methods ---
    bool is_service_running();
    bool start_service();
    nlohmann::json get_protection_status();
    bool set_protection(bool enabled, int duration_ms = 0);

private:
    AdGuardManager() = default;
    
    std::string base_url;
    std::string auth_name;
    std::string auth_pass;
    
    void load_config_credentials();
    std::map<std::string, std::string> get_auth_headers();
    
    // Helpers
    void collect_hourly_stats();
    void aggregate_daily_stats(const std::string& date_str);
    nlohmann::json get_remote_stats();
    void update_legacy_stats(const std::string& date_str);
    
    // Internal helper to fetch existing clients for add/update decision
    nlohmann::json get_agh_clients();
};

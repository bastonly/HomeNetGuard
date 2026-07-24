#pragma once

#include <string>
#include <vector>
#include <set>

class NftManager {
public:
    static void init_groups();
    static void sync_firewall_state();
    static void check_and_settle_timeouts();
    static void sync_authorized_pool();
    
    static bool add_to_group(const std::string& group_name, const std::string& mac);
    static void remove_from_all_groups(const std::string& mac);
    
    // Helper to run nft commands
    // Helper to run nft commands
    static bool run_nft_cmd(const std::vector<std::string>& args);

    // Helper to get status of all groups
    // Returns a JSON array of objects {id, name, status, ...}
    static std::string get_groups_status();

    static bool is_table_exists();
    static const std::string TABLE;
    static const std::string CHAIN_MAIN;
    static const std::string SET_AUTH_ALL;
    static const std::string LOCAL_IP;
};

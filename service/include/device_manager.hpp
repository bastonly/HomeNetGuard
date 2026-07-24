#pragma once

#include "db.hpp"
#include <string>
#include <map>
#include <vector>

class DeviceManager {
public:
    static void update_online_status();

private:
    struct LeaseInfo {
        std::string ip;
        std::string name;
    };

    static std::map<std::string, LeaseInfo> load_leases();
};

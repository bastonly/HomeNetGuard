#include "utils.hpp"
#include "db.hpp"
#include "scheduler.hpp"
#include "log_monitor.hpp"
#include "nft_manager.hpp"
#include "adguard_manager.hpp"
#include "device_manager.hpp"
#include "license_manager.hpp"
#include "entertainment_manager.hpp"
#include "redis_manager.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <signal.h>

// Global flag for daemon loop
static volatile bool keep_running = true;

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        Utils::Logger::info("Received SIGUSR1. Queuing immediate NFT sync.");
        Scheduler::getInstance().trigger_sync_nft();
    } else {
        keep_running = false;
    }
}


// Helper to parse duration string (10m, 1h, tomorrow) to milliseconds
long parse_duration_ms(const std::string& duration_str) {
    if (duration_str.empty()) return 0; // indefinitely
    
    if (duration_str == "tomorrow") {
        std::time_t now = std::time(nullptr);
        std::tm* tm_now = std::localtime(&now);
        
        std::tm tm_tomorrow = *tm_now;
        tm_tomorrow.tm_mday += 1;
        tm_tomorrow.tm_hour = 0;
        tm_tomorrow.tm_min = 0;
        tm_tomorrow.tm_sec = 0;
        tm_tomorrow.tm_isdst = -1;
        
        std::time_t tomorrow = std::mktime(&tm_tomorrow);
        return static_cast<long>(std::difftime(tomorrow, now) * 1000);
    }
    
    try {
        if (duration_str.back() == 'm') {
            return std::stol(duration_str.substr(0, duration_str.size() - 1)) * 60 * 1000;
        } else if (duration_str.back() == 'h') {
            return std::stol(duration_str.substr(0, duration_str.size() - 1)) * 3600 * 1000;
        } else {
            // Assume minutes if no unit
            return std::stol(duration_str) * 60 * 1000;
        }
    } catch (...) {
        return 0;
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [commands]\n"
              << "Commands:\n"
              << "  --daemon                  Start as background service\n"
              << "  --init-nft                Initialize firewall rules\n"
              << "  --sync-nft                Sync firewall state\n"
              << "  --add-to-group <grp> <mac> Add device to group\n"
              << "  --sync-agh                Sync AdGuard Home config\n"
              << "  --analysis-agh            Run AdGuard Daily Analysis\n"
              << "  --agh-status              Get AdGuard status (running, protection etc)\n"
              << "  --agh-start               Manually start AdGuard service\n"
              << "  --agh-pause [dur]         Pause protection (e.g. 10m, 1h, tomorrow)\n"
              << "  --agh-resume              Resume protection\n"
              << "  --analysis-entertainment  Run Entertainment Analysis (from logs)\n"
              << "  --sync-entertainment-redis [date] Sync Real-time Entertainment Redis stats to DB\n"
              << "  --checkin                 Force license checkin\n"
              << "  --activate <code>         Activate license on this device\n"
              << "  --version                 Show version\n"
              << "  --status                  Show firewall status\n"
              << "  --check-redis             Test Redis connection and config\n"
              << "  --schema-db               Force database schema check/migration\n"
              << "  --get-ip                  Show LAN, WAN and Public IP\n";
}

int main(int argc, char* argv[]) {
    // --- 1. Basic CLI Commands (No config/DB needed) ---
    if (argc > 1) {
        std::string cmd = argv[1];
        if (cmd == "--version") {
            std::cout << "Parent Control Service v1.0.0 (C++)\n";
            return 0;
        }
        if (cmd == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (cmd == "--get-ip") {
            std::cout << "LAN IP:    " << Utils::get_lan_ip() << "\n";
            std::cout << "WAN IP:    " << Utils::get_wan_ip() << "\n";
            std::cout << "Public IP: " << Utils::get_public_ip() << "\n";
            return 0;
        }
    }

    // --- 2. Load Configuration ---
    AppConfig config = Utils::Config::load_config();
    LicenseManager::getInstance().ensure_config_defaults();

    // --- 3. Database & Logger Setup (Early init to enable logging) ---
    Utils::Logger::init();
    if (!DB::getInstance().init(DB_PATH)) {
        std::cerr << "Failed to initialize database at " << DB_PATH << std::endl;
        return 1;
    }

    // --- 4. Simple CLI Commands (Config & DB needed) ---
    if (argc > 1) {
        std::string cmd = argv[1];
        if (cmd == "--checkin") {
            bool ok = LicenseManager::getInstance().checkin();
            if (ok) std::cout << "License Checkin: OK\n";
            else std::cout << "License Checkin: Failed or Inactive\n";
            return ok ? 0 : 1;
        }
        if (cmd == "--activate") {
            if (argc < 3) {
                std::cerr << "Usage: --activate <activation_code>\n";
                return 1;
            }
            bool ok = LicenseManager::getInstance().activate(argv[2]);
            if (ok) std::cout << "Activation SUCCESS\n";
            else std::cerr << "Activation FAILED\n";
            return ok ? 0 : 1;
        }
    }

    // --- 5. Complex CLI Commands (Need DB/NFT/AGH) ---
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string cmd = argv[1];

    if (cmd == "--init-nft") {
        if (Utils::acquire_lock()) {
            NftManager::init_groups();
            Utils::release_lock();
            std::cout << "NFT initialized.\n";
            return 0;
        } else {
            std::cerr << "Failed to acquire lock.\n";
            return 1;
        }
    }
    
    if (cmd == "--sync-nft") {
        if (Utils::acquire_lock()) {
            NftManager::sync_firewall_state();
            Utils::release_lock();
            std::cout << "NFT synced.\n";
            return 0;
        } else {
            std::cerr << "Failed to acquire lock.\n";
            return 1;
        }
    }
    
    if (cmd == "--schema-db") {
        Utils::Logger::info("Manually triggering database schema check/migration...");
        DB::getInstance().check_and_migrate_schema();
        std::cout << "Database schema check completed.\n";
        return 0;
    }
    
    if (cmd == "--status") {
        std::string json = NftManager::get_groups_status();
        std::cout << json << std::endl;
        return 0;
    }
    
    if (cmd == "--check-redis") {
        std::cout << "Testing Redis Connection...\n";
        RedisManager& redis = RedisManager::getInstance();
        if (redis.set("pc:test_connect", "OK", 10)) {
            std::string val = redis.get("pc:test_connect");
            if (val == "OK") {
                std::cout << "✅ Redis Connection: SUCCESS\n";
                return 0;
            }
        }
        std::cerr << "❌ Redis Connection: FAILED. Check /etc/parent-control/base.conf\n";
        return 1;
    }
    
    if (cmd == "--add-to-group") {
        if (argc < 4) {
            std::cerr << "Usage: --add-to-group <group_name> <mac>\n";
            return 1;
        }
        std::string group = argv[2];
        std::string mac = argv[3];
        
        if (Utils::acquire_lock()) {
            bool ok = NftManager::add_to_group(group, mac);
            Utils::release_lock();
            return ok ? 0 : 1;
        } else {
            std::cerr << "Failed to acquire lock.\n";
            return 1;
        }
    }
    
    if (cmd == "--sync-agh") {
        AdGuardManager::getInstance().sync_adguard_config();
        std::cout << "AdGuard config synced.\n";
        return 0;
    }
    
    if (cmd == "--analysis-agh") {
        AdGuardManager::getInstance().run_daily_analysis();
        std::cout << "Analysis completed.\n";
        return 0;
    }
    
    if (cmd == "--sync-agh-logs") {
        std::string date = "";
        if (argc >= 3) date = argv[2];
        else {
             std::time_t now = std::time(nullptr);
             char buf[20];
             std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&now));
             date = buf;
        }
        int count = AdGuardManager::getInstance().sync_logs_for_date(date);
        std::cout << "Synced " << count << " logs for " << date << ".\n";
        return 0;
    }
    
    if (cmd == "--analysis-entertainment") {
        std::string date = "";
        if (argc >= 3) date = argv[2];
        EntertainmentManager::getInstance().detect_for_date(date);
        std::cout << "Entertainment detection completed.\n";
        return 0;
    }

    if (cmd == "--sync-entertainment-redis") {
        std::string date = "";
        if (argc >= 3) date = argv[2];
        else {
            std::string now = Utils::get_current_time_str();
            date = now.substr(0, 10);
        }
        EntertainmentManager::getInstance().sync_to_db(date);
        std::cout << "Redis entertainment stats synced to DB for " << date << ".\n";
        return 0;
    }

    if (cmd == "--agh-list-rules") {
        std::cout << AdGuardManager::getInstance().get_all_custom_rules() << std::endl;
        return 0;
    }
    
    if (cmd == "--agh-block") {
        if (argc < 3) {
            std::cerr << "Usage: --agh-block <domain>\n";
            return 1;
        }
        bool ok = AdGuardManager::getInstance().add_custom_filter_rule(argv[2]);
        if (ok) std::cout << "Blocked " << argv[2] << "\n";
        else std::cerr << "Failed to block " << argv[2] << "\n";
        return ok ? 0 : 1;
    }
    
    if (cmd == "--agh-unblock") {
        if (argc < 3) {
            std::cerr << "Usage: --agh-unblock <domain>\n";
            return 1;
        }
        bool ok = AdGuardManager::getInstance().remove_custom_filter_rule(argv[2]);
        if (ok) std::cout << "Unblocked " << argv[2] << "\n";
        else std::cerr << "Failed to unblock " << argv[2] << "\n";
        return ok ? 0 : 1;
    }

    if (cmd == "--agh-status") {
        bool running = AdGuardManager::getInstance().is_service_running();
        nlohmann::json status = AdGuardManager::getInstance().get_protection_status();
        
        nlohmann::json result;
        result["running"] = running;
        result["protection_enabled"] = status.value("protection_enabled", false);
        result["protection_disabled_duration"] = status.value("protection_disabled_duration", 0);
        
        std::cout << result.dump() << std::endl;
        return 0;
    }
    
    if (cmd == "--agh-start") {
        bool ok = AdGuardManager::getInstance().start_service();
        if (ok) {
            std::cout << "OK" << std::endl;
            return 0;
        } else {
            std::cerr << "Failed to start AdGuard Home service" << std::endl;
            return 1;
        }
    }
    
    if (cmd == "--agh-pause") {
        std::string duration = "";
        if (argc >= 3) duration = argv[2];
        
        long ms = parse_duration_ms(duration);
        bool ok = AdGuardManager::getInstance().set_protection(false, (int)ms);
        if (ok) std::cout << "OK" << std::endl;
        else std::cerr << "Failed to pause" << std::endl;
        return ok ? 0 : 1;
    }
    
    if (cmd == "--agh-resume") {
        bool ok = AdGuardManager::getInstance().set_protection(true);
        if (ok) std::cout << "OK" << std::endl;
        else std::cerr << "Failed to resume" << std::endl;
        return ok ? 0 : 1;
    }

    // --- 6. Daemon Mode ---
    if (cmd == "--daemon") {
        Utils::Logger::info("Starting PC Service Daemon...");
        
        // Note: Schema check is NOT automatic here anymore to optimize startup.
        // It should be run manually via --schema-db if needed.

        if (Utils::acquire_lock()) {
            NftManager::init_groups();
            Utils::release_lock();
        } else {
            Utils::Logger::error("Failed to acquire lock for initial NFT setup.");
        }

        LogMonitor::getInstance().start();
        Scheduler::getInstance().start();
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGUSR1, signal_handler);
        
        while (keep_running) {
             sleep(1);
        }
        
        Utils::Logger::info("Stopping services...");
        Scheduler::getInstance().stop();
        LogMonitor::getInstance().stop();
        Utils::Logger::info("Bye.");
        return 0;
    }
    
    print_usage(argv[0]);
    return 1;
}

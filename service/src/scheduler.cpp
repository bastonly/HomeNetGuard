#include "scheduler.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "nft_manager.hpp"
#include "device_manager.hpp"
#include "adguard_manager.hpp"
#include "entertainment_manager.hpp"
#include "license_manager.hpp"
#include <iostream>
#include <chrono>
#include <unistd.h>

Scheduler& Scheduler::getInstance() {
    static Scheduler instance;
    return instance;
}

Scheduler::Scheduler() : running(false) {}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running) return;
    running = true;
    scheduler_thread = std::thread(&Scheduler::loop, this);
    Utils::Logger::info("Scheduler started.");
}

void Scheduler::stop() {
    if (!running) return;
    running = false;
    if (scheduler_thread.joinable()) {
        scheduler_thread.join();
    }
    Utils::Logger::info("Scheduler stopped.");
}

void Scheduler::trigger_sync_nft() {
    force_sync_nft.store(true);
}

void Scheduler::loop() {
    int tick = 0;
    Utils::Logger::info("Entering scheduler main loop.");
    
    // Initial pre-warm: Ensure built-in categories and heartbeat
    EntertainmentManager::getInstance().ensure_builtin_categories();
    
    AppConfig startup_config = Utils::Config::load_config();
    LicenseManager::getInstance().log_security_event(EventLevel::INFO, "SYSTEM", "cpp.SERVICE_STARTED", "", "", "SYSTEM");
    
    // Initialize last_heartbeat_date from config to prevent double-checkin on startup if already done today
    if (!startup_config.last_checkin_time.empty() && startup_config.last_checkin_time.length() >= 10) {
        last_heartbeat_date = startup_config.last_checkin_time.substr(0, 10);
        Utils::Logger::info("Initialized last checkin date from config: " + last_heartbeat_date);
    }

    // Only perform startup checkin if the last one was more than 12 hours ago
    long now_startup = std::time(nullptr);
    if (now_startup - startup_config.last_checkin_ts > 12 * 3600) {
        Utils::Logger::info("Last checkin was long ago. Performing startup checkin...");
        task_license_heartbeat();
    } else {
        Utils::Logger::info("Recent checkin found (" + startup_config.last_checkin_time + "). Skipping startup checkin.");
    }

    while (running) {
        AppConfig config = Utils::Config::load_config();
        long now = std::time(nullptr);
        
        // 2. Sync NFT (Default 60s or SIGUSR1 triggered)
        if (force_sync_nft.load() || now - last_sync_nft >= config.scheduler.interval_sync_nft) {
            if (force_sync_nft.load()) {
                Utils::Logger::info("Immediate NFT sync triggered by signal.");
            }
            force_sync_nft.store(false);
            task_sync_nft(); 
            last_sync_nft = now;
        }
        
        // 3. Device Update (Default 10s for Redis real-time sync)
        if (now - last_device_update >= 10 && LicenseManager::getInstance().is_license_valid()) {
            task_device_update();
            last_device_update = now;
        }

        // 5. Update Groups Status Cache (Every 30s)
        if (now - last_status_cache_update >= 30) {
            NftManager::get_groups_status();
            last_status_cache_update = now;
        }

        // 6. Refresh Home Stats Cache (Every 30s)
        if (now - last_stats_refresh >= 20) {
            AdGuardManager::getInstance().refresh_home_stats_cache();
            last_stats_refresh = now;
        }

        // 7. Background AdGuard Log Sync (Every 60s)
        if (now - last_adguard_log_sync >= 60 && LicenseManager::getInstance().is_license_valid()) {
            AdGuardManager::getInstance().analyze_logs();
            last_adguard_log_sync = now;
        }
        
        // 6. Daily Tasks
        std::string current_time_str = Utils::get_current_time_str(); // YYYY-MM-DD HH:MM:SS
        if (current_time_str.length() >= 16) {
            std::string date_part = current_time_str.substr(0, 10);
            std::string time_part = current_time_str.substr(11, 5); // HH:MM
            
            try {
                // AdGuard Analysis
                if (time_part == config.scheduler.adguard_analysis_time && last_adguard_analysis_date != date_part) {
                    task_adguard_analysis();
                    last_adguard_analysis_date = date_part;
                }

                // Entertainment Analysis & Statistics
                if (time_part == config.scheduler.entertainment_collect_time && last_entertainment_sync_date != date_part) {
                    task_entertainment_sync();
                    last_entertainment_sync_date = date_part;
                }
                // DB Backup (Daily 04:30)
                if (time_part == "04:30" && last_db_backup_date != date_part) {
                    task_db_backup();
                    last_db_backup_date = date_part;
                }

                // Cleanup (Daily 04:45)
                if (time_part == "04:45" && last_cleanup_date != date_part) {
                    task_cleanup();
                    last_cleanup_date = date_part;
                }

                // 1. License Checkin (Daily at 00:05)
                if (time_part == "00:05" && last_heartbeat_date != date_part) {
                    if (LicenseManager::getInstance().is_license_valid()) {
                        task_license_heartbeat();
                    } else {
                        Utils::Logger::warn("License grace period expired. Automatic check-in skipped.");
                    }
                    last_heartbeat_date = date_part;
                }
            } catch (const std::exception& e) {
                Utils::Logger::error("Exception in daily task loop: " + std::string(e.what()));
            } catch (...) {
                Utils::Logger::error("Unknown exception in daily task loop.");
            }
        }
        
        // Sleep 1s
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tick++;
    }
}

void Scheduler::task_sync_nft() {
    if (Utils::acquire_lock()) {
        NftManager::sync_firewall_state();
        NftManager::get_groups_status(); 
        Utils::release_lock();
    }
}

void Scheduler::task_device_update() {
    DeviceManager::update_online_status();
}

void Scheduler::task_adguard_analysis() {
    Utils::Logger::info("Running daily AdGuard analysis...");
    AdGuardManager::getInstance().run_daily_analysis();
}

void Scheduler::task_entertainment_sync() {
    Utils::Logger::info("Running daily entertainment analysis & statistics...");
    
    std::time_t t = std::time(nullptr);
    t -= 86400; // yesterday
    std::tm* tm_info = std::localtime(&t);
    char buf[12];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
    std::string yesterday(buf);

    EntertainmentManager::getInstance().sync_to_db(yesterday);
}

void Scheduler::task_cleanup() {
    Utils::Logger::info("Running daily cleanup...");
    
    std::string clean_cmd = "find " + LOG_DIR + " -name '" + LOG_FILE_PATTERN + "*.log' -mtime +20 -delete 2>/dev/null";
    Utils::exec_command(clean_cmd.c_str());
    Utils::Logger::info("Service log cleanup completed (Retention: 20 days).");

    AppConfig config = Utils::Config::load_config();
    int msg_days = config.retention_sys_messages;
    if (msg_days > 0) {
        std::string sql = "DELETE FROM sys_messages WHERE updated_at < datetime('now', '-" + std::to_string(msg_days) + " days', 'localtime')";
        DB::getInstance().exec(sql);
        Utils::Logger::info("SysMessages cleanup completed (Retention: " + std::to_string(msg_days) + " days).");
    }

    int detail_days = config.retention_adguard_detail;
    if (detail_days > 0) {
        std::string sql = "DELETE FROM adguard_device_logs WHERE access_date < date('now', '-" + std::to_string(detail_days) + " days', 'localtime')";
        DB::getInstance().exec(sql);
        Utils::Logger::info("AdGuard details cleanup completed (Retention: " + std::to_string(detail_days) + " days).");
    }

    int summary_days = config.retention_adguard_summary;
    if (summary_days > 0) {
        std::string sql_sum = "DELETE FROM adguard WHERE stat_dt < date('now', '-" + std::to_string(summary_days) + " days', 'localtime')";
        DB::getInstance().exec(sql_sum);
        
        std::string sql_ent = "DELETE FROM entertainment_detections WHERE detection_date < date('now', '-" + std::to_string(summary_days) + " days', 'localtime')";
        DB::getInstance().exec(sql_ent);
        
        Utils::Logger::info("AdGuard summaries & Entertainment detections cleanup completed (Retention: " + std::to_string(summary_days) + " days).");
    }

    std::time_t t = std::time(nullptr);
    std::tm* tm_info = std::localtime(&t);
    if (tm_info->tm_wday == 0) { // 0 is Sunday
        Utils::Logger::info("Weekly scheduled VACUUM starting...");
        DB::getInstance().exec("VACUUM;");
        Utils::Logger::info("Database VACUUM completed.");
    } else {
        Utils::Logger::info("Daily cleanup finished (VACUUM skipped until Sunday).");
    }
}

void Scheduler::task_db_backup() {
    Utils::Logger::info("Starting daily database backup...");
    DB::getInstance().exec("PRAGMA wal_checkpoint(TRUNCATE);");
    
    std::string cmd = "day=$(date +%A); cp /opt/parent-control/backend/data/pc.db /opt/parent-control/backend/data/pc.db.bak.$day";
    std::string result = Utils::exec_command(cmd.c_str());
    
    if (result.empty()) {
        Utils::Logger::info("Database backup completed.");
    } else {
        Utils::Logger::warn("Database backup output: " + result);
    }
}

void Scheduler::task_license_heartbeat() {
    Utils::Logger::info("Initiating scheduled license checkin...");
    bool active = LicenseManager::getInstance().checkin();
    if (!active) {
        Utils::Logger::warn("License suspended or checkin failed during heartbeat.");
    }
}

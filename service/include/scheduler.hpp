#pragma once

#include <thread>
#include <atomic>
#include <string>

class Scheduler {
public:
    static Scheduler& getInstance();
    
    void start();
    void stop();

    // Remote Triggers
    void trigger_sync_nft();

private:
    Scheduler();
    ~Scheduler();
    
    std::atomic<bool> running;
    std::thread scheduler_thread;
    
    void loop();
    
    // Task Helpers
    void task_sync_nft();
    void task_device_update();
    void task_adguard_analysis();
    void task_entertainment_sync();
    void task_cleanup();
    void task_license_heartbeat();
    void task_db_backup();
    
    // State
    std::atomic<bool> force_sync_nft{false};
    long last_sync_nft = 0;
    long last_device_update = 0;
    long last_status_cache_update = 0;
    long last_stats_refresh = 0;
    long last_adguard_log_sync = 0;
    std::string last_adguard_analysis_date = "";
    std::string last_entertainment_sync_date = "";
    std::string last_db_backup_date = "";
    std::string last_cleanup_date = "";
    std::string last_heartbeat_date = "";
};

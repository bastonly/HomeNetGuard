#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <regex>

class LogMonitor {
public:
    static LogMonitor& getInstance();
    
    // Starts the monitoring thread
    void start();
    
    // Stops the monitoring thread
    void stop();

private:
    LogMonitor();
    ~LogMonitor();
    
    std::atomic<bool> running;
    std::thread monitor_thread;
    
    // Regex for parsing log lines
    std::regex log_pattern;
    
    void monitor_loop();
    void parse_line(const std::string& line);
    void save_log(const std::string& tag, const std::string& group, const std::string& ip, const std::string& mac, const std::string& raw);
    
    std::string get_device_group(const std::string& mac);
    std::string get_group_name_by_id(const std::string& id);
    void cleanup_old_data();
};

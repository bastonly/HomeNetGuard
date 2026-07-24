#pragma once

#include "common.hpp"
#include <string>
#include <nlohmann/json.hpp>
#include <mutex>

namespace Utils {

    // Logger
    namespace Logger {
        void init();
        void log(const std::string& level, const std::string& message);
        void info(const std::string& message);
        void error(const std::string& message);
        void warn(const std::string& message);
        void debug(const std::string& message);
    }

    // Configuration
    namespace Config {
        AppConfig load_config();
        bool save_config(const AppConfig& config); // Optional, mostly read-only for service
        nlohmann::json read_json_file(const std::string& path);
    }

    // HTTP Helpers
    struct HttpResponse {
        long status_code;
        std::string body;
        std::string error_msg;
    };
    
    HttpResponse http_get(const std::string& url, const std::map<std::string, std::string>& headers = {}, int timeout_sec = 10);
    HttpResponse http_post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}, int timeout_sec = 10);

    // System & Fingerprint
    std::string get_device_fingerprint();
    std::string exec_command(const char* cmd);
    std::string get_current_time_str(); // Returns US/Eastern time string
    std::string get_model();
    std::string get_os_version();
    std::string get_hostname();
    std::string get_lan_ip();
    std::string get_wan_ip();
    std::string get_public_ip();
    
    // File Locking
    bool acquire_lock();
    void release_lock();

    // String helpers
    std::string sha256(const std::string& str);
    std::string hmac_sha256(const std::string& key, const std::string& data);
    std::string format_date_string(const std::string& iso_str, const std::string& format);
    std::string format_time(long timestamp);
    long parse_time(const std::string& time_str);
    long parse_est_time(const std::string& time_str);
    std::string reformat_iso_to_fixed(const std::string& iso_str, bool append_est = false);
    std::string escape_sql(const std::string& str);
    std::vector<std::string> split_string(const std::string& s, char delimiter);
}

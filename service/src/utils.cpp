#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <array>
#include <memory>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "sha256.hpp"
#include <thread>
#include <openssl/hmac.h>
#include "redis_manager.hpp"

#include <curl/curl.h>

namespace Utils {

    // --- HTTP Helpers ---
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    HttpResponse perform_http_request(const std::string& url, const std::string& method, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_sec) {
        HttpResponse response = {0, "", ""};
        CURL* curl;
        CURLcode res;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* chunk = nullptr;
            std::string auth_user, auth_pass;
            for (const auto& [key, val] : headers) {
                if (key == "X-Auth-User") auth_user = val;
                else if (key == "X-Auth-Pass") auth_pass = val;
                else {
                    std::string h = key + ": " + val;
                    chunk = curl_slist_append(chunk, h.c_str());
                }
            }
            
            if (!auth_user.empty() && !auth_pass.empty()) {
                std::string auth = auth_user + ":" + auth_pass;
                curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
                curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
            }

            // Allow self-signed certs if needed (or use CAPATH)
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
            
            // Basic Auth if provided
            auto user_it = headers.find("X-Auth-User");
            auto pass_it = headers.find("X-Auth-Pass");
            if (user_it != headers.end() && pass_it != headers.end()) {
                std::string auth = user_it->second + ":" + pass_it->second;
                curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
                curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
            }

            if (method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }

            res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                response.error_msg = curl_easy_strerror(res);
            } else {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(chunk);
        } else {
            response.error_msg = "Failed to init curl";
        }
        return response;
    }

    HttpResponse http_get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_sec) {
        return perform_http_request(url, "GET", "", headers, timeout_sec);
    }

    HttpResponse http_post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_sec) {
        return perform_http_request(url, "POST", body, headers, timeout_sec);
    }

    // --- Logger ---
    namespace Logger {
        std::mutex log_mutex;
        
        void init() {
            // Ensure log directory exists is handled by shell script or manual mkdir
        }

        void log(const std::string& level, const std::string& message) {
            std::lock_guard<std::mutex> lock(log_mutex);
            
            std::string time_str = get_current_time_str();
            std::string date_str = time_str.substr(0, 10); // YYYY-MM-DD
            
            std::string log_file_path = LOG_DIR + "/" + LOG_FILE_PATTERN + date_str + ".log";
            
            std::ofstream log_file(log_file_path, std::ios::app);
            if (log_file.is_open()) {
                log_file << time_str << " [" << level << "] " << message << std::endl;
                
                // Only print ERROR and WARNING to stderr for debugging (avoids flooding system logs with INFO)
                if (level == "ERROR" || level == "WARNING") {
                    std::cerr << time_str << " [" << level << "] " << message << std::endl;
                }
            } else {
                // Fallback to the legacy log file if we can't open the daily one
                std::ofstream legacy_file(LOG_FILE, std::ios::app);
                if (legacy_file.is_open()) {
                    legacy_file << time_str << " [" << level << "] " << message << std::endl;
                }
                
                if (level == "ERROR" || level == "WARNING") {
                    std::cerr << time_str << " [" << level << "] " << message << std::endl;
                }
            }
        }

        void info(const std::string& message) {
            log("INFO", message);
        }

        void error(const std::string& message) {
            log("ERROR", message);
        }

        void warn(const std::string& message) {
            log("WARNING", message);
        }

        void debug(const std::string& message) {
            log("DEBUG", message);
        }
    }

    // --- Config ---
    namespace Config {
        using json = nlohmann::json;

        json read_json_file(const std::string& path) {
            std::ifstream f(path);
            if (!f.is_open()) {
                Logger::error("Failed to open config file: " + path);
                return json{};
            }
            try {
                return json::parse(f);
            } catch (json::parse_error& e) {
                Logger::error("JSON parse error in " + path + ": " + e.what());
                return json{};
            }
        }

        static AppConfig cached_config;
        static time_t last_mtime = 0;
        static bool has_cached = false;

        AppConfig load_config() {
            struct stat st;
            if (stat(CONFIG_FILE.c_str(), &st) == 0) {
                if (has_cached && st.st_mtime == last_mtime) {
                    return cached_config;
                }
                last_mtime = st.st_mtime;
            }

            AppConfig config;
            json j = read_json_file(CONFIG_FILE);
            
            if (j.empty()) {
                if (has_cached) return cached_config;
                return config;
            }

            try {
                if (j.contains("SERVER")) {
                    auto& s = j["SERVER"];
                    if (s.contains("URL")) config.server_url = s["URL"].get<std::string>();
                    if (s.contains("API_Token")) config.api_token = s["API_Token"].get<std::string>();
                    if (s.contains("LICENSE_ID")) config.license_id = s["LICENSE_ID"].get<std::string>();
                    if (s.contains("CURRENT_API_VERSION")) config.current_api_version = s["CURRENT_API_VERSION"].get<std::string>();
                    if (s.contains("LAST_CHECKIN_TIME")) config.last_checkin_time = s["LAST_CHECKIN_TIME"].get<std::string>();
                    if (s.contains("LAST_CHECKIN_TS")) config.last_checkin_ts = s["LAST_CHECKIN_TS"].get<long>();
                }

                if (j.contains("JWT")) {
                    auto& jwt = j["JWT"];
                    if (jwt.contains("JWT_SECRET_KEY")) config.jwt_secret = jwt["JWT_SECRET_KEY"].get<std::string>();
                }

                if (j.contains("ADGUARD")) {
                    auto& adg = j["ADGUARD"];
                    if (adg.contains("SERVER")) {
                        auto& s = adg["SERVER"];
                        if (s.contains("URL")) config.adguard_url = s["URL"].get<std::string>();
                        if (s.contains("USER")) config.adguard_user = s["USER"].get<std::string>();
                        if (s.contains("PASSWORD")) config.adguard_password = s["PASSWORD"].get<std::string>();
                    }
                }
                
                // Scheduler Config
                if (j.contains("SCHEDULER")) {
                    auto& sched = j["SCHEDULER"];
                    if (sched.contains("SYNC_NFT_INTERVAL")) config.scheduler.interval_sync_nft = sched["SYNC_NFT_INTERVAL"].get<int>();
                    if (sched.contains("DEVICE_UPDATE_INTERVAL")) config.scheduler.interval_device_update = sched["DEVICE_UPDATE_INTERVAL"].get<int>();
                    if (sched.contains("ADGUARD_ANALYSIS_TIME")) config.scheduler.adguard_analysis_time = sched["ADGUARD_ANALYSIS_TIME"].get<std::string>();
                    if (sched.contains("ENTERTAINMENT_COLLECT_TIME")) config.scheduler.entertainment_collect_time = sched["ENTERTAINMENT_COLLECT_TIME"].get<std::string>();
                    if (sched.contains("CLEANUP_TIME")) config.scheduler.cleanup_time = sched["CLEANUP_TIME"].get<std::string>();
                }

                if (j.contains("SYSTEM")) {
                    auto& sys = j["SYSTEM"];
                    if (sys.contains("TIMEZONE")) config.timezone = sys["TIMEZONE"].get<std::string>();
                    if (sys.contains("MODEL")) config.model = sys["MODEL"].get<std::string>();
                    if (sys.contains("OS_VERSION")) config.os_version = sys["OS_VERSION"].get<std::string>();
                }

                if (j.contains("REDIS")) {
                    auto& r = j["REDIS"];
                    if (r.contains("HOST")) config.redis.host = r["HOST"].get<std::string>();
                    if (r.contains("PORT")) config.redis.port = r["PORT"].get<int>();
                    if (r.contains("PASSWORD")) config.redis.password = r["PASSWORD"].get<std::string>();
                    if (r.contains("DB")) config.redis.db = r["DB"].get<int>();
                }

                // Retention Config
                auto retention = j.value("RETENTION", json::object());
                config.retention_adguard_summary = retention.value("ADGUARD_SUMMARY", 30);
                config.retention_adguard_detail = retention.value("ADGUARD_DETAIL", 5);
                config.retention_sys_messages = retention.value("SYS_MESSAGES", 15);

                cached_config = config;
                has_cached = true;

            } catch (const std::exception& e) {
                Logger::error("Error parsing config: " + std::string(e.what()));
            }

            return config;
        }
    }

    // --- System & Fingerprint ---
    std::string exec_command(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd_str = std::string(cmd) + " 2>/dev/null";
        FILE* p = popen(cmd_str.c_str(), "r");
        if (!p) {
            Logger::error("popen() failed for cmd: " + std::string(cmd) + " | errno=" + std::to_string(errno));
            return "__EXEC_FAILED__";
        }
        
        while (fgets(buffer.data(), buffer.size(), p) != nullptr) {
            result += buffer.data();
        }
        pclose(p);

        // Remove trailing newline and whitespace
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
            result.pop_back();
        }
        return result;
    }
    
    std::string sha256(const std::string& str) {
        return sha256_custom(str);
    }

    std::string hmac_sha256(const std::string& key, const std::string& data) {
        unsigned char* digest;
        unsigned int len = 0;
        
        digest = HMAC(EVP_sha256(), key.c_str(), key.length(), 
                      (unsigned char*)data.c_str(), data.length(), NULL, &len);
                      
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for(unsigned int i = 0; i < len; i++) {
            ss << std::setw(2) << (int)digest[i];
        }
        return ss.str();
    }

    std::string format_date_string(const std::string& iso_str, const std::string& format) {
        if (iso_str.empty()) return "";
        int y, m, d, h = 0, min = 0, s = 0;
        if (std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &min, &s) == 6) { }
        else if (std::sscanf(iso_str.c_str(), "%d-%d-%d", &y, &m, &d) >= 3) { }
        else return iso_str;
        char buf[64];
        if (format == "MM-DD-YYYY") std::sprintf(buf, "%02d-%02d-%04d", m, d, y);
        else if (format == "MM-DD-YYYY HH:MM:SS") std::sprintf(buf, "%02d-%02d-%04d %02d:%02d:%02d", m, d, y, h, min, s);
        else return iso_str;
        return std::string(buf);
    }

    std::string get_device_fingerprint() {
        bool any_exec_failed = false;

        // 1. UUID
        std::string uuid = exec_command("block info | grep -o 'UUID=\"[^\"]*\"' | head -n 1 | awk -F'\"' '{print $2}'");
        if (uuid == "__EXEC_FAILED__") { any_exec_failed = true; uuid = ""; }
        
        // 2. MAC 
        std::string mac = exec_command("cat /sys/class/net/eth0/address");
        if (mac == "__EXEC_FAILED__") { any_exec_failed = true; mac = ""; }
        else if (mac.empty()) {
            mac = exec_command("cat /sys/class/net/br-lan/address");
            if (mac == "__EXEC_FAILED__") { any_exec_failed = true; mac = ""; }
        }
        
        // 3. Serial
        std::string serial = exec_command("cat /proc/cpuinfo | grep Serial | awk '{print $3}'");
        if (serial == "__EXEC_FAILED__") { any_exec_failed = true; serial = ""; }
        
        // CRITICAL: If popen failed during info gathering, return EMPTY string to signal UNSTABLE state
        // This is much safer than returning a partial or empty-hashed fingerprint.
        if (any_exec_failed) {
            Logger::error("System Resource Exhausted: popen() failed during fingerprint generation. Marking as UNSTABLE.");
            return ""; 
        }

        if (uuid.empty() && mac.empty() && serial.empty()) {
             Logger::error("CRITICAL: All device fingerprint components are physically empty.");
             return ""; 
        }
        
        std::string raw_str = "";
        bool first = true;
        if (!uuid.empty()) { raw_str += "root-uuid:" + uuid; first = false; }
        if (!mac.empty()) { if (!first) raw_str += "|"; raw_str += "mac:" + mac; first = false; }
        if (!serial.empty()) { if (!first) raw_str += "|"; raw_str += "serial:" + serial; }
        
        return sha256_custom(raw_str);
    }
    
    std::mutex tz_mutex;

    std::string format_time(long timestamp) {
        std::lock_guard<std::mutex> lock(tz_mutex);
        std::time_t t = static_cast<std::time_t>(timestamp);
        char buf[80];
        // Note: we use the configured TZ if set, similar to get_current_time_str
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return std::string(buf);
    }

    long parse_time(const std::string& time_str) {
        if (time_str.empty()) return 0;
        
        // Check for pure numeric timestamp first (legacy format)
        bool is_num = true;
        for (char c : time_str) if (!isdigit(c)) { is_num = false; break; }
        if (is_num) return std::stol(time_str);

        std::tm tm = {};
        std::istringstream ss(time_str);
        
        // Try YYYY-MM-DD HH:MM:SS
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            // Try ISO format (T separator)
            ss.clear();
            ss.str(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        }
        if (ss.fail()) {
            // Try YYYY-MM-DD
            ss.clear();
            ss.str(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%d");
        }
        
        if (ss.fail()) return 0;
        tm.tm_isdst = -1; // 让系统自动判断夏令时，避免转换偏移
        return static_cast<long>(std::mktime(&tm));
    }

    long parse_est_time(const std::string& time_str) {
        if (time_str.empty()) return 0;
        
        std::tm tm = {};
        std::istringstream ss(time_str);
        
        // The server API usually returns "YYYY-MM-DD HH:MM:SS"
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            ss.clear();
            ss.str(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        }
        if (ss.fail()) {
            ss.clear();
            ss.str(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%d");
        }
        if (ss.fail()) return 0;

        std::lock_guard<std::mutex> lock(tz_mutex);
        
        // Save current TZ
        const char* old_tz = getenv("TZ");
        std::string old_tz_str = old_tz ? old_tz : "";
        
        // Force US/Eastern
        setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
        tzset();
        
        long result = static_cast<long>(std::mktime(&tm));
        
        // Restore TZ
        if (!old_tz_str.empty()) {
            setenv("TZ", old_tz_str.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
        
        return result;
    }


    std::string reformat_iso_to_fixed(const std::string& iso_str, bool append_est) {
        if (iso_str.empty()) return "";
        
        // Handle fractional part if present by truncating at the first '.'
        std::string clean_str = iso_str;
        size_t dot_pos = clean_str.find('.');
        if (dot_pos != std::string::npos) {
            clean_str = clean_str.substr(0, dot_pos);
        }

        std::tm tm = {};
        std::istringstream ss(clean_str);
        
        // Try common formats until one works
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail()) {
            ss.clear();
            ss.str(clean_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        }
        if (ss.fail()) {
            ss.clear();
            ss.str(clean_str);
            ss >> std::get_time(&tm, "%Y-%m-%d");
        }
        
        if (ss.fail()) {
            // Just return as is if parsing fails
            return iso_str + (append_est ? " EST" : "");
        }
        
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        std::string res(buf);
        if (append_est) res += " EST";
        return res;
    }

    std::string get_current_time_str() {
        AppConfig config = Config::load_config();
        
        // POSIX TZ strings for common zones if they are selected
        // This is a robust fallback for OpenWrt without zoneinfo
        std::map<std::string, std::string> tz_map = {
            /* ===== North America ===== */
            {"America/New_York",    "EST5EDT,M3.2.0,M11.1.0"},
            {"America/Chicago",     "CST6CDT,M3.2.0,M11.1.0"},
            {"America/Denver",      "MST7MDT,M3.2.0,M11.1.0"},
            {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
            {"America/Phoenix",     "MST7"},
            {"America/Anchorage",   "AKST9AKDT,M3.2.0,M11.1.0"},
            {"Pacific/Honolulu",    "HST10"},
            {"America/Toronto",     "EST5EDT,M3.2.0,M11.1.0"},
            {"America/Vancouver",   "PST8PDT,M3.2.0,M11.1.0"},
            {"America/Mexico_City", "CST6"},

            /* ===== South America ===== */
            {"America/Sao_Paulo",   "BRT3"},
            {"America/Argentina/Buenos_Aires", "ART3"},
            {"America/Santiago",    "CLT4CLST,M9.1.6/24,M4.1.6/24"},
            {"America/Bogota",      "COT5"},
            {"America/Lima",        "PET5"},

            /* ===== Europe ===== */
            {"Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0/2"},
            {"Europe/Paris",        "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Berlin",       "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Madrid",       "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Rome",         "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Amsterdam",    "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Zurich",       "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Stockholm",    "CET-1CEST,M3.5.0/2,M10.5.0/3"},
            {"Europe/Athens",       "EET-2EEST,M3.5.0/3,M10.5.0/4"},
            {"Europe/Moscow",       "MSK-3"},

            /* ===== Asia ===== */
            {"Asia/Shanghai",       "CST-8"},
            {"Asia/Hong_Kong",      "HKT-8"},
            {"Asia/Tokyo",          "JST-9"},
            {"Asia/Seoul",          "KST-9"},
            {"Asia/Singapore",      "SGT-8"},
            {"Asia/Bangkok",        "ICT-7"},
            {"Asia/Kolkata",        "IST-5:30"},
            {"Asia/Dubai",          "GST-4"},
            {"Asia/Jerusalem",      "IST-2IDT,M3.4.5/2,M10.5.0/2"},
            {"Asia/Riyadh",         "AST-3"},

            /* ===== Africa ===== */
            {"Africa/Cairo",        "EET-2"},
            {"Africa/Johannesburg", "SAST-2"},
            {"Africa/Nairobi",      "EAT-3"},
            {"Africa/Lagos",        "WAT-1"},
            {"Africa/Casablanca",   "WET0WEST,M3.5.0,M10.5.0"},

            /* ===== Oceania ===== */
            {"Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3"},
            {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
            {"Australia/Perth",     "AWST-8"},
            {"Pacific/Auckland",    "NZST-12NZDT,M9.5.0,M4.1.0/3"},
            {"Pacific/Fiji",        "FJT-12"},
        };

        std::string tz_str = config.timezone;
        if (tz_map.count(config.timezone)) {
            tz_str = tz_map[config.timezone];
        }

        std::lock_guard<std::mutex> lock(tz_mutex);
        setenv("TZ", tz_str.c_str(), 1);
        tzset();
        
        std::time_t now = std::time(nullptr);
        char buf[80];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }

    // --- Socket/File Locking ---
    bool acquire_lock() {
        RedisManager& redis = RedisManager::getInstance();
        int attempts = 0;
        const int max_attempts = 100; // 10 seconds total wait
        
        while (attempts < max_attempts) {
            if (redis.try_lock("global_pc_lock", 30)) { // 30s TTL
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        Logger::error("Failed to acquire Redis lock after timeout");
        return false;
    }
    
    void release_lock() {
        RedisManager::getInstance().unlock("global_pc_lock");
    }

    std::string get_model() {
        std::string json_str = exec_command("ubus call system board");
        try {
            auto j = nlohmann::json::parse(json_str);
            return j.value("model", "OpenWrt Router");
        } catch (...) {
            return "OpenWrt Router";
        }
    }

    std::string get_os_version() {
        std::string json_str = exec_command("ubus call system board");
        try {
            auto j = nlohmann::json::parse(json_str);
            if (j.contains("release") && j["release"].contains("description")) {
                return j["release"]["description"].get<std::string>();
            }
        } catch (...) {}
        return "OpenWrt Linux";
    }

    std::string get_hostname() {
        std::string json_str = exec_command("ubus call system board");
        try {
            auto j = nlohmann::json::parse(json_str);
            return j.value("hostname", "ImmortalWrt");
        } catch (...) {
            return "ImmortalWrt";
        }
    }
    std::string escape_sql(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            if (c == '\'') escaped += "''";
            else escaped += c;
        }
        return escaped;
    }

    std::vector<std::string> split_string(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::string get_ip_from_interface(const std::string& iface) {
        std::string json_str = exec_command(("ubus call network.interface." + iface + " status").c_str());
        try {
            auto j = nlohmann::json::parse(json_str);
            if (j.contains("ipv4-address") && j["ipv4-address"].is_array() && j["ipv4-address"].size() > 0) {
                return j["ipv4-address"][0].value("address", "");
            }
        } catch (...) {}
        return "";
    }

    std::string get_lan_ip() {
        std::string ip = get_ip_from_interface("lan");
        if (ip.empty()) ip = get_ip_from_interface("br-lan");
        return ip;
    }

    std::string get_wan_ip() {
        std::string ip = get_ip_from_interface("wan");
        if (ip.empty()) ip = get_ip_from_interface("pppoe-wan");
        if (ip.empty()) ip = get_ip_from_interface("eth0"); // bypass mode?
        return ip;
    }

    std::string get_public_ip() {
        std::string wan_ip = get_wan_ip();
        
        // If no WAN IP, fallback to LAN IP for private check
        std::string check_ip = wan_ip.empty() ? get_lan_ip() : wan_ip;
        
        // Private IP ranges: 10.x.x.x, 172.16-31.x.x, 192.168.x.x
        bool is_private = false;
        if (check_ip.empty()) is_private = true;
        else if (check_ip.find("192.168.") == 0) is_private = true;
        else if (check_ip.find("10.") == 0) is_private = true;
        else if (check_ip.find("172.") == 0) {
            try {
                size_t p = check_ip.find('.', 4);
                if (p != std::string::npos) {
                    int scd = std::stoi(check_ip.substr(4, p - 4));
                    if (scd >= 16 && scd <= 31) is_private = true;
                }
            } catch (...) {}
        }

        // If it's a real public IP on the interface, return it
        if (!is_private && !check_ip.empty()) return check_ip;

        // Otherwise (or if check_ip is private), MUST use external API
        HttpResponse res = http_get("http://api.ipify.org");
        if (res.status_code == 200 && !res.body.empty()) {
            return res.body;
        }
        
        return wan_ip; // Last resort
    }
}

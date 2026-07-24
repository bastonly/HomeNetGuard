#include "license_manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <ctime>
#include <thread>
#include <sys/stat.h>

using json = nlohmann::json;

LicenseManager& LicenseManager::getInstance() {
    static LicenseManager instance;
    return instance;
}

std::string LicenseManager::get_license_status_text(int status_code) {
    switch(status_code) {
        case 0: return "license.status.pending";
        case 1: return "license.status.active";
        case 2: return "license.status.expired";
        case 3: return "license.status.revoked";
        case 4: return "license.status.suspended";
        default: return "license.status.unknown";
    }
}

std::string LicenseManager::get_grace_reason_text(GraceReason reason) {
    switch(reason) {
        case GraceReason::INACTIVE_STATUS: return "cpp.GRACE_REASON_INACTIVE";
        case GraceReason::CONNECTION_FAILED: return "cpp.GRACE_REASON_CONN_FAIL";
        case GraceReason::TIMEOUT: return "cpp.GRACE_REASON_TIMEOUT";
        case GraceReason::SERVER_ERROR: return "cpp.GRACE_REASON_SERVER_ERR";
        case GraceReason::UNKNOWN: return "cpp.GRACE_REASON_UNKNOWN";
        default: return "";
    }
}

void LicenseManager::update_local_license_info(const std::string& license_id, const std::string& status, const std::string& device_status, const std::string& expired_date, const std::string& server_time, const std::string& latest_version, long last_checkin_ts, const std::string& grace_reason, long grace_start_ts) {
    json j = Utils::Config::read_json_file(CONFIG_FILE); 
    if (j.empty()) return;

    if (!j.contains("SERVER")) j["SERVER"] = json::object();
    
    if (!license_id.empty()) j["SERVER"]["LICENSE_ID"] = license_id;
    j["SERVER"]["LICENSE_STATUS"] = status;
    j["SERVER"]["DEVICE_STATUS"] = device_status;
    
    std::string fixed_exp = Utils::reformat_iso_to_fixed(expired_date);
    if (!fixed_exp.empty()) j["SERVER"]["EXPIRED_DATE"] = fixed_exp;
    
    long expired_ts = Utils::parse_time(fixed_exp);
    j["SERVER"]["EXPIRED_TS"] = expired_ts;

    if (!server_time.empty()) {
        j["SERVER"]["SERVER_TIME"] = Utils::reformat_iso_to_fixed(server_time);
    }
    
    if (!latest_version.empty()) {
        j["SERVER"]["LASTEST_API_VERSION"] = latest_version;
    }

    if (last_checkin_ts > 0) {
        j["SERVER"]["LAST_CHECKIN_TIME"] = Utils::format_time(last_checkin_ts);
        j["SERVER"]["LAST_CHECKIN_TS"] = last_checkin_ts;
    }

    // --- 统一宽限期时长定义 (写入配置供前端显示) ---
    j["SERVER"]["GRACE_PERIOD_MAX"] = DEFAULT_GRACE_PERIOD_DAYS;
    
    // 计算剩余宽限时间并存入配置
    long current_grace_start = j["SERVER"].value("GRACE_START_TS", 0L);
    if (grace_start_ts > 0) current_grace_start = grace_start_ts;
    
    if (current_grace_start > 0 && status != "active") {
        long now = std::time(nullptr);
        long diff = now - current_grace_start;
        long remaining_sec = GRACE_PERIOD_SECONDS - diff;
        int remaining_days = (remaining_sec < 0) ? -1 : (int)(remaining_sec / (24 * 3600));
        j["SERVER"]["GRACE_REMAINING_DAYS"] = remaining_days;
    } else {
        // 如果不在宽限期，清除剩余天数或设为默认
        if (j["SERVER"].contains("GRACE_REMAINING_DAYS")) j["SERVER"].erase("GRACE_REMAINING_DAYS");
    }

    // 处理宽限期逻辑
    if (!grace_reason.empty()) {
        j["SERVER"]["GRACE_REASON"] = grace_reason;
    }
    
    if (grace_start_ts > 0) {
        j["SERVER"]["GRACE_START_TIME"] = Utils::format_time(grace_start_ts);
        j["SERVER"]["GRACE_START_TS"] = grace_start_ts;
    }

    // 状态清理：恢复为双 Active 或变为 Unbound 时彻底删除宽限期所有残留字段
    if ((status == "active" && device_status == "active") || device_status == "unbound") {
        if (j["SERVER"].contains("GRACE_START_TIME")) j["SERVER"].erase("GRACE_START_TIME");
        if (j["SERVER"].contains("GRACE_START_TS"))   j["SERVER"].erase("GRACE_START_TS");
        if (j["SERVER"].contains("GRACE_REASON"))     j["SERVER"].erase("GRACE_REASON");
    }

    // --- 核心加固：同步重算 HMAC 签名 ---
    long calc_checkin_ts = j["SERVER"].value("LAST_CHECKIN_TS", 0L);
    long calc_grace_ts = j["SERVER"].value("GRACE_START_TS", 0L);
    long calc_expired_ts = j["SERVER"].value("EXPIRED_TS", 0L);
    
    std::string payload_str = status + ":" + 
                            std::to_string(calc_checkin_ts) + ":" + 
                            std::to_string(calc_grace_ts) + ":" + 
                            std::to_string(calc_expired_ts);
    
    std::string fp = Utils::get_device_fingerprint();
    if (!fp.empty()) {
        j["SERVER"]["OFFLINE_MAC"] = Utils::hmac_sha256(fp, payload_str);
    } else {
        Utils::Logger::warn("Device fingerprint unavailable during info update. Skipping HMAC re-signing to maintain stability.");
    }

    // Save back
    std::ofstream o(CONFIG_FILE);
    if (o.is_open()) {
        o << std::setw(4) << j << std::endl;
    }
}

void LicenseManager::log_security_event(const std::string& level, const std::string& tag, const std::string& content, const std::string& payload, const std::string& source_ip, const std::string& source_mac) const {
    std::string now_str = Utils::get_current_time_str();
    std::string ip = source_ip.empty() ? "127.0.0.1" : source_ip;
    std::string mac = source_mac.empty() ? "SYSTEM" : source_mac;
    
    std::string sql = "INSERT INTO sys_messages (category, level, event_tag, source_ip, source_mac, group_name, content, payload, occurrence_count, is_read, created_at, updated_at) VALUES ('" + 
                      EventCategory::SECURITY + "', '" + level + "', '" + tag + "', '" + ip + "', '" + mac + "', 'System', '" + Utils::escape_sql(content) + "', '" + Utils::escape_sql(payload) + "', 1, 0, '" + now_str + "', '" + now_str + "')";
    
    DB::getInstance().exec(sql);
}

void LicenseManager::suspend_tampered_license(const std::string& reason) const {
    Utils::Logger::error("suspend_tampered_license triggered: " + reason);

    json j = Utils::Config::read_json_file(CONFIG_FILE); 
    if (j.empty()) return;
    if (!j.contains("SERVER")) j["SERVER"] = json::object();
    
    // 1. Locally revoke (hard stop — no grace period, same as server-side revoke)
    j["SERVER"]["LICENSE_STATUS"] = "revoked";
    if (j["SERVER"].contains("GRACE_START_TIME")) j["SERVER"].erase("GRACE_START_TIME");
    if (j["SERVER"].contains("GRACE_START_TS"))   j["SERVER"].erase("GRACE_START_TS");
    if (j["SERVER"].contains("GRACE_REASON"))     j["SERVER"].erase("GRACE_REASON");
    
    // --- Re-sign to avoid infinite tampering alerts once revoked ---
    long last_checkin_ts = j["SERVER"].value("LAST_CHECKIN_TS", 0L);
    long expired_ts = j["SERVER"].value("EXPIRED_TS", 0L);
    std::string payload_str = std::string("revoked:") + 
                            std::to_string(last_checkin_ts) + ":0:" + 
                            std::to_string(expired_ts);
    std::string fp = Utils::get_device_fingerprint();
    if (fp.empty()) {
        Utils::Logger::error("CRITICAL: Cannot re-sign revoked license because fingerprint is unavailable. Aborting revocation to prevent accidental lockout.");
        return; 
    }
    j["SERVER"]["OFFLINE_MAC"] = Utils::hmac_sha256(fp, payload_str);

    std::ofstream o(CONFIG_FILE);
    if (o.is_open()) o << std::setw(4) << j << std::endl;
    
    // 2. Audit log
    json payload = {{"event", "tampering_detected"}, {"reason", reason}};
    log_security_event(EventLevel::WARNING, "AUTHORIZATION_FAILURE", "cpp.TAMPER_DETECTED", payload.dump());

    // 3. Async remote alert
    std::thread([j = std::move(j), reason]() {
        AppConfig config = Utils::Config::load_config();
        if (config.server_url.empty()) return;

        std::string ver = "1"; 
        if (!config.current_api_version.empty()) {
            size_t dot_pos = config.current_api_version.find('.');
            if (dot_pos != std::string::npos) {
                ver = config.current_api_version.substr(0, dot_pos);
            } else {
                ver = config.current_api_version;
            }
        }
        std::string url = config.server_url + "/api/v" + ver + "/alert";
        
        json alert_payload = {
            {"license_id", config.license_id},
            {"device_fingerprint", Utils::get_device_fingerprint()},
            {"alert_type", "tampering_detected"},
            {"details", reason},
            {"meta_info", {
                {"hostname", Utils::get_hostname()},
                {"client_date", Utils::get_current_time_str()},
                {"timezone", config.timezone},
                {"client_lan", Utils::get_lan_ip()},
                {"client_wan", Utils::get_wan_ip()},
                {"public_ip", Utils::get_public_ip()}
            }}
        };
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        if (!config.api_token.empty()) headers["X-Customer-Token"] = config.api_token;
        
        Utils::Logger::info("Sending tampering alert to remote server...");
        Utils::http_post(url, alert_payload.dump(), headers, 10);
    }).detach();
}

bool LicenseManager::is_license_valid() const {
    static bool cached_result = false;
    static time_t last_check_time = 0;
    static time_t last_conf_mtime = 0;

    time_t now = std::time(nullptr);

    // Check file mtime to see if config changed
    struct stat st;
    time_t current_mtime = 0;
    if (stat(CONFIG_FILE.c_str(), &st) == 0) {
        current_mtime = st.st_mtime;
    }

    // Cache for 6 minutes (360s) UNLESS the config file has been modified
    if (last_check_time > 0 && (now - last_check_time < 360) && (current_mtime == last_conf_mtime)) {
        return cached_result;
    }

    auto perform_check = [&]() -> bool {
        json j = Utils::Config::read_json_file(CONFIG_FILE);
        if (j.empty() || !j.contains("SERVER")) return false;
        
        std::string status = j["SERVER"].value("LICENSE_STATUS", "inactive");
        std::string device_status = j["SERVER"].value("DEVICE_STATUS", "fail");
        
        // 1. 执行全维度的 HMAC 签名校验
        if (j["SERVER"].contains("OFFLINE_MAC")) {
            std::string stored_mac = j["SERVER"].value("OFFLINE_MAC", "");
            
            long last_checkin_ts = j["SERVER"].value("LAST_CHECKIN_TS", 0L);
            long grace_start_ts = j["SERVER"].value("GRACE_START_TS", 0L);
            long expired_ts = j["SERVER"].value("EXPIRED_TS", 0L);
            
            if (last_checkin_ts == 0) last_checkin_ts = Utils::parse_time(j["SERVER"].value("LAST_CHECKIN_TIME", "0"));
            if (grace_start_ts == 0) grace_start_ts = Utils::parse_time(j["SERVER"].value("GRACE_START_TIME", "0"));

            std::string fp = Utils::get_device_fingerprint();

            if (fp.empty()) {
                // System unstable, assume valid if it was valid before to avoid lockout
                return cached_result; 
            } else {
                std::string payload_new = status + ":" + std::to_string(last_checkin_ts) + ":" + 
                                        std::to_string(grace_start_ts) + ":" + std::to_string(expired_ts);
                
                std::string payload_old = status + ":" + std::to_string(last_checkin_ts) + ":" + 
                                        std::to_string(grace_start_ts);

                if (stored_mac != Utils::hmac_sha256(fp, payload_new) && stored_mac != Utils::hmac_sha256(fp, payload_old)) {
                    suspend_tampered_license("HMAC verification failed. Local configuration tampering detected.");
                    return false;
                }
            }
            
            if (status == "revoked") return false;
            if (status == "active" && device_status == "active") return true;
            if (device_status == "unbound") return false;

            // Grace period logic
            if (grace_start_ts > 0) {
                if (now < grace_start_ts - 86400) {
                     suspend_tampered_license("System time is before grace start time. Time tampering detected.");
                     return false;
                }
                if (now - grace_start_ts > GRACE_PERIOD_SECONDS) return false;
                return true;
            }
        }
        return false;
    };

    cached_result = perform_check();
    last_check_time = now;
    last_conf_mtime = current_mtime;
    return cached_result;
}

bool LicenseManager::activate(const std::string& code) {
    Utils::Logger::info("Activating license with code: " + code);
    
    AppConfig config = Utils::Config::load_config();
    if (config.server_url.empty()) {
        Utils::Logger::error("Server URL missing. Cannot activate.");
        return false;
    }

    std::string ver = "1"; 
    if (!config.current_api_version.empty()) {
        size_t dot_pos = config.current_api_version.find('.');
        if (dot_pos != std::string::npos) {
            ver = config.current_api_version.substr(0, dot_pos);
        } else {
            ver = config.current_api_version;
        }
    }

    std::string url = config.server_url + "/api/v" + ver + "/activate";

    json payload = {
        {"activation_code", code}, 
        {"customer_api_token", config.api_token}, 
        {"device_fingerprint", Utils::get_device_fingerprint()},
        {"model", Utils::get_model()},
        {"os_version", Utils::get_os_version()},
        {"app_version", config.current_api_version.empty() ? "1.01" : config.current_api_version},
        {"meta_info", {
            {"hostname", Utils::get_hostname()},
            {"client_date", Utils::get_current_time_str()},
            {"timezone", config.timezone},
            {"client_lan", Utils::get_lan_ip()},
            {"client_wan", Utils::get_wan_ip()},
            {"public_ip", Utils::get_public_ip()}
        }}
    };

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    if (!config.api_token.empty()) {
        headers["X-Customer-Token"] = config.api_token;
    }

    Utils::HttpResponse res = Utils::http_post(url, payload.dump(), headers, 15);
    
    if (res.status_code == 200) {
        try {
            json response = json::parse(res.body);
            if (response.value("status", "fail") == "ok") {
                std::string lic_id = response.value("license_id", "");
                std::string exp_date = response.value("expired_date", "");
                std::string server_time = response.value("server_time", "");
                std::string ver = response.value("lastest_api_version", "");
                
                // 解析状态码 (如果后端激活接口返回了状态码，优先使用；否则默认为 active)
                int lic_code = response.value("license_status", 1); // Default 1 = active
                int dev_code = response.value("device_status", 1);  // Default 1 = active
                
                std::string lic_status = (lic_code == 3) ? "revoked" : (lic_code == 2) ? "expired" : "active";
                std::string dev_status = (dev_code == 3) ? "revoked" : (dev_code == 1) ? "active" : "fail";

                // 使用统一辅助函数更新所有信息并自动重算签名
                update_local_license_info(lic_id, lic_status, dev_status, exp_date, server_time, ver, std::time(nullptr), "", 0);

                Utils::Logger::info("Activation successful! License ID: " + lic_id + ", Status: " + lic_status);
                
                return true;
            } else {
                std::string msg = response.value("message", "Unknown error");
                log_security_event(EventLevel::WARNING, "ACTIVATE", "cpp.ACTIVATE_FAILED", response.dump());
                Utils::Logger::error("Activation failed: " + msg);
                return false;
            }
        } catch (const std::exception& e) {
            Utils::Logger::error("Failed to parse activation response: " + std::string(e.what()));
            return false;
        }
    } else {
        log_security_event(EventLevel::ERROR, "ACTIVATE", "cpp.ACTIVATE_ERROR", res.error_msg, "Server", "SYSTEM");
        Utils::Logger::error("Activation request failed. HTTP Code: " + std::to_string(res.status_code) + ", Error: " + res.error_msg + ", Body: " + res.body);
        return false;
    }
}

bool LicenseManager::checkin() {
    Utils::Logger::info("Performing license checkin...");
    
    // 1. 加载配置（用于后续接口请求）
    AppConfig config = Utils::Config::load_config();
    
    // 2. 检查激活状态（从原始 JSON 读取，因为 AppConfig 暂不包含状态字段）
    json j = Utils::Config::read_json_file(CONFIG_FILE);
    std::string status = "unknown";
    if (j.contains("SERVER")) status = j["SERVER"].value("LICENSE_STATUS", "unknown");

    // Revoked: 允许向服务端发起真实的 checkin，以便管理员在服务端恢复后可以在本地恢复。
    // Unknown / Inactive: 未激活，不尝试。
    if (status == "unknown" || status == "inactive") {
        log_security_event(EventLevel::CRITICAL, "ILLEGAL_CHECKIN", "cpp.ILLEGAL_CHECKIN", "{}", "", "SYSTEM");
        Utils::Logger::warn("Checkin skipped: License status is " + status + ".");
        return false;
    }
    
    if (config.license_id.empty() || config.server_url.empty()) {
        Utils::Logger::error("License ID or Server URL missing. Cannot checkin.");
        return false;
    }

    std::string ver = "1"; 
    if (!config.current_api_version.empty()) {
        size_t dot_pos = config.current_api_version.find('.');
        if (dot_pos != std::string::npos) {
            ver = config.current_api_version.substr(0, dot_pos);
        } else {
            ver = config.current_api_version;
        }
    }
    std::string url = config.server_url + "/api/v" + ver + "/checkin";
    
    // Parse domain from server_url for logging (source_mac)
    std::string domain = "";
    size_t p1 = config.server_url.find("://");
    if (p1 != std::string::npos) {
        domain = config.server_url.substr(p1 + 3);
    } else {
        domain = config.server_url;
    }
    
    // Remote terminal parts: / and :
    size_t p2 = domain.find("/");
    if (p2 != std::string::npos) domain = domain.substr(0, p2);
    size_t p3 = domain.find(":");
    if (p3 != std::string::npos) domain = domain.substr(0, p3);
    
    if (domain.empty()) domain = "Server";
    
    std::string lan_ip = Utils::get_lan_ip();
    std::string wan_ip = Utils::get_wan_ip();
    std::string public_ip = Utils::get_public_ip();

    // Construct payload
    json payload = {
        {"license_id", config.license_id},
        {"device_fingerprint", Utils::get_device_fingerprint()},
        {"model", Utils::get_model()},
        {"os_version", Utils::get_os_version()},
        {"app_version", config.current_api_version.empty() ? "1.01" : config.current_api_version},
        {"meta_info", {
            {"client_date", Utils::get_current_time_str()},
            {"hostname", Utils::get_hostname()},
            {"timezone", config.timezone},
            {"client_lan", lan_ip},
            {"client_wan", wan_ip},
            {"public_ip", public_ip}
        }}
    };

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    if (!config.api_token.empty()) {
        headers["X-Customer-Token"] = config.api_token;
    } else {
        Utils::Logger::warn("Missing API Token for checkin.");
    }

    Utils::HttpResponse res = Utils::http_post(url, payload.dump(), headers, 10);
    
    long now = std::time(nullptr);

    if (res.status_code == 200) {
        try {
            json response = json::parse(res.body);
            // Check API status
            std::string api_status = response.value("status", "fail");
            
            // Parse status fields regardless of result
            int license_status_code = response.value("license_status", 0);
            int device_status_code = response.value("device_status", 0);
            std::string expired_date = response.value("expired_date", "");
            std::string latest_version = response.value("lastest_api_version", "");
            std::string server_time = response.value("server_time", "");

            // Map integer statuses to strings
            std::string lic_status_str = "inactive";
            switch(license_status_code) {
                case 0: lic_status_str = "pending"; break;
                case 1: lic_status_str = "active"; break;
                case 2: lic_status_str = "expired"; break;
                case 3: lic_status_str = "revoked"; break;
                case 4: lic_status_str = "suspended"; break;
                default: lic_status_str = "inactive"; break;
            }

            std::string dev_status_str = "fail";
            switch(device_status_code) {
                case 1: dev_status_str = "active"; break;
                case 2: dev_status_str = "unbound"; break;
                // case 3 is not used for device status as per business rules
                default: dev_status_str = "fail"; break;
            }

            // Reformat response object for logging early (ensures audit logs are human-readable)
            response["license_status"] = lic_status_str;
            response["device_status"] = dev_status_str;
            response["expired_date"] = Utils::reformat_iso_to_fixed(expired_date);
            response["server_time"] = Utils::reformat_iso_to_fixed(server_time, true);

            // 解析过期时间戳
            long expired_ts = 0;
            if (!expired_date.empty()) {
                expired_ts = Utils::parse_time(expired_date);
            }
            
            // 实时维护和保存 
            // -----------------------------------------------------
            // 调用统一辅助函数：更新状态、ID、时间戳并同步重算 HMAC 签名
            // -----------------------------------------------------
            update_local_license_info(config.license_id, lic_status_str, dev_status_str, expired_date, server_time, latest_version, now, "", 0);

            json j = Utils::Config::read_json_file(CONFIG_FILE);
            
            // [Trigger Grace Period] Any non-active status triggers grace period, EXCEPT "unbound"
            // [Trigger Grace Period] Any non-active status triggers grace period, 
            // EXCEPT "unbound" (Safe Uninstall) and "revoked" (Hard Stop).
            if ((lic_status_str != "active" || dev_status_str != "active") && 
                 dev_status_str != "unbound" && 
                 lic_status_str != "revoked" && 
                 dev_status_str != "revoked") {
                // Determine detailed error message
                std::string specific_err = "";
                if (lic_status_str != "active") {
                    specific_err = "cpp.DEVICE_AUTH_ERROR";
                }

                std::string reason_text = get_grace_reason_text(GraceReason::INACTIVE_STATUS);

                bool first_time = !j["SERVER"].contains("GRACE_START_TIME");
                
                long grace_start_ts = now;
                if (lic_status_str == "expired" && expired_ts > 0) {
                    grace_start_ts = expired_ts;
                } else {
                    // 对于其他异常（挂起/断网），如果是第一次触发，起算点为现在；
                    // 如果之前已经有起算点了，严格保持不变，确保用户拥有完整的 30 天宽限期。
                    if (!first_time) {
                        grace_start_ts = j["SERVER"].value("GRACE_START_TS", now);
                    }
                }
                
                // 保存更新后的起算点
                j["SERVER"]["GRACE_START_TIME"] = Utils::format_time(grace_start_ts);
                j["SERVER"]["GRACE_START_TS"] = grace_start_ts;
                j["SERVER"]["GRACE_REASON"] = reason_text;

                // If it's the first time triggering grace period, or if license expired,
                // update the grace start timestamp. The `update_local_license_info` will handle
                // the logic of not overwriting an existing grace start time unless it's an expired license.
                
                if (first_time) {
                    // Audit Log for first triggger
                    log_security_event(EventLevel::WARNING, "GRACE_PERIOD_TRIGGERED", "cpp.GRACE_PERIOD_START", response.dump(), domain, "SYSTEM");
                }
                
                // 再次更新配置以保存 GRACE 相关字段，并重新计算带 GRACE_START_TS 的签名
                update_local_license_info(config.license_id, lic_status_str, dev_status_str, expired_date, server_time, latest_version, now, reason_text, grace_start_ts);
            }
            
            // -----------------------------------------------------
            // 防篡改签名更新区 (只要成功 checkin 就必须要计算 / 用最新数据重写 HMAC)
            // 此时 j 对象里已经包含了最新的 LAST_CHECKIN_TS, EXPIRED_TS, 以及可能的 GRACE_START_TS
            // This is now handled by update_local_license_info
            // -----------------------------------------------------

            // If Active or Revoked, ensure grace period fields are cleared.
            // This is also handled by update_local_license_info if grace_reason_str is empty.
            // We need to re-read the config to ensure we have the latest state after potential grace period updates.
            j = Utils::Config::read_json_file(CONFIG_FILE);

            if (api_status == "ok") {
                if (lic_status_str == "active" && dev_status_str == "active") {
                    Utils::Logger::info("License verified successfully (Status: 1/Active).");
                    log_security_event(EventLevel::INFO, "CHECKIN", "cpp.CHECKIN_SUCCESS", response.dump(), domain, "SYSTEM");
                } else {
                    // Status is ok from API perspective (request processed), but resource state is not active
                    std::string specific_err = "cpp.CHECKIN_WARNING";
                    if (lic_status_str == "expired") {
                         int grace_days = -1;
                         if (j.contains("SERVER") && j["SERVER"].contains("GRACE_REMAINING_DAYS")) {
                             grace_days = j["SERVER"].value("GRACE_REMAINING_DAYS", -1);
                         }
                         if (grace_days >= 0) {
                             specific_err = "cpp.CHECKIN_GRACE";
                         } else {
                             specific_err = "cpp.CHECKIN_EXPIRED";
                         }
                    }
                    else if (dev_status_str == "unbound") specific_err = "cpp.CHECKIN_UNBOUND";
                    else if (lic_status_str == "revoked") specific_err = "cpp.DEVICE_AUTH_ERROR"; // Clearer for revoked
                    
                    log_security_event(EventLevel::WARNING, "CHECKIN", specific_err, response.dump(), domain, "SYSTEM");
                }
                return is_license_valid();
            } else {
                 std::string res_body = res.body;
                 std::string msg = response.value("message", "Unknown error");
                                  log_security_event(EventLevel::WARNING, "CHECKIN", "cpp.CHECKIN_ABNORMAL", response.dump(), domain, "SYSTEM");
                 Utils::Logger::warn("Checkin API returned non-ok status: " + res_body);
                 
                 // Even if status is "fail", if we are in a valid grace period, we return true
                 return is_license_valid();
            }
        } catch (const std::exception& e) {
            Utils::Logger::error("Failed to parse checkin response: " + std::string(e.what()));
        }
    } else {
        Utils::Logger::error("Checkin failed. HTTP Code: " + std::to_string(res.status_code) + ", Error: " + res.error_msg);
        
        // Attempt to start offline grace period
        json j = Utils::Config::read_json_file(CONFIG_FILE);
        if (j.contains("SERVER") && j["SERVER"].value("LICENSE_STATUS", "") == "active") {
             if (!j["SERVER"].contains("GRACE_START_TIME")) {
                  long now = std::time(nullptr);
                  
                  j["SERVER"]["GRACE_START_TIME"] = Utils::format_time(now);
                  j["SERVER"]["GRACE_START_TS"] = now;
                  
                  // Generate HMAC signature using numerical TS
                  long expired_ts = j["SERVER"].value("EXPIRED_TS", 0L);
                  long last_checkin_ts = j["SERVER"].value("LAST_CHECKIN_TS", 0L);
                  
                  // Payload: status:checkin_ts:grace_ts:expired_ts
                  std::string payload_str = std::string("active:") + 
                                          std::to_string(last_checkin_ts) + ":" + 
                                          std::to_string(now) + ":" + 
                                          std::to_string(expired_ts);
                  
                  std::string fp = Utils::get_device_fingerprint();
                  std::string mac = Utils::hmac_sha256(fp, payload_str);
                  
                  j["SERVER"]["OFFLINE_MAC"] = mac;

                  // Determine reason
                  GraceReason reason = GraceReason::UNKNOWN;
                  if (res.status_code == 0) {
                      reason = GraceReason::CONNECTION_FAILED;
                  } else if (res.status_code == 408) {
                      reason = GraceReason::TIMEOUT;
                  } else if (res.status_code >= 500) {
                      reason = GraceReason::SERVER_ERROR;
                  }

                  std::string reason_text = get_grace_reason_text(reason);
                  j["SERVER"]["GRACE_REASON"] = reason_text;
                  
                  // Compute GRACE_REMAINING_DAYS
                  long remaining_sec = GRACE_PERIOD_SECONDS;
                  j["SERVER"]["GRACE_REMAINING_DAYS"] = (int)(remaining_sec / (24 * 3600));
                  
                  std::ofstream o(CONFIG_FILE);
                  if (o.is_open()) o << std::setw(4) << j << std::endl;
                  
                  Utils::Logger::warn("Network error (" + std::to_string(res.status_code) + ") during checkin. Initiating Offline Grace Period: " + reason_text);

                   // Audit Log
                   log_security_event(EventLevel::WARNING, "GRACE_PERIOD_TRIGGERED", "cpp.GRACE_PERIOD_START", res.error_msg, domain, "SYSTEM");
              } else {
                   // Already in grace period, but let's log the failed attempt OR update reason if missing
                   GraceReason current_reason = GraceReason::UNKNOWN;
                   if (res.status_code == 0) current_reason = GraceReason::CONNECTION_FAILED;
                   else if (res.status_code == 408) current_reason = GraceReason::TIMEOUT;
                   else if (res.status_code >= 500) current_reason = GraceReason::SERVER_ERROR;
                   
                   std::string reason_text = get_grace_reason_text(current_reason);
                   if (!reason_text.empty()) {
                       j["SERVER"]["GRACE_REASON"] = reason_text;
                   }
                   
                   // Update GRACE_REMAINING_DAYS
                   long grace_start = j["SERVER"].value("GRACE_START_TS", 0L);
                   long diff = std::time(nullptr) - grace_start;
                   long remaining_sec = GRACE_PERIOD_SECONDS - diff;
                   int remaining_days = (remaining_sec < 0) ? -1 : (int)(remaining_sec / (24 * 3600));
                   j["SERVER"]["GRACE_REMAINING_DAYS"] = remaining_days;

                   std::ofstream o(CONFIG_FILE);
                   if (o.is_open()) o << std::setw(4) << j << std::endl;

                   log_security_event(EventLevel::WARNING, "CHECKIN_FAILED", "cpp.CHECKIN_FAILED", res.error_msg, domain, "SYSTEM");
              }
         } else {
              // Not active, but failed checkin
              log_security_event(EventLevel::ERROR, "CHECKIN_FAILED", "cpp.CHECKIN_ERROR", res.error_msg, domain, "SYSTEM");
         }
        
        // 无论何种情况都调用一次基于当前配置重算 GRACE_REMAINING_DAYS 以确保 CLI 直接执行能刷新
        ensure_config_defaults();
        
        return is_license_valid();
    }
    
    return is_license_valid();

}

int LicenseManager::get_grace_period_days() const {
    json j = Utils::Config::read_json_file(CONFIG_FILE);
    if (j.empty() || !j.contains("SERVER")) return DEFAULT_GRACE_PERIOD_DAYS;
    
    if (j["SERVER"].contains("GRACE_START_TS")) {
        long grace_start = j["SERVER"].value("GRACE_START_TS", 0L);
        long now = std::time(nullptr);
        long diff = now - grace_start;
        long remaining_sec = GRACE_PERIOD_SECONDS - diff;
        
        if (remaining_sec < 0) return -1;
        return (int)(remaining_sec / (24 * 3600));
    }
    
    return DEFAULT_GRACE_PERIOD_DAYS;
}

void LicenseManager::ensure_config_defaults() {
    json j = Utils::Config::read_json_file(CONFIG_FILE);
    if (j.empty()) return;
    
    if (!j.contains("SERVER")) j["SERVER"] = json::object();
    
    // 写入默认设置
    j["SERVER"]["GRACE_PERIOD_MAX"] = DEFAULT_GRACE_PERIOD_DAYS;
    
    // 如果存在宽限期，顺便更新一下当前的剩余天数
    if (j["SERVER"].contains("GRACE_START_TS")) {
        long grace_start = j["SERVER"].value("GRACE_START_TS", 0L);
        long now = std::time(nullptr);
        long diff = now - grace_start;
        long remaining_sec = GRACE_PERIOD_SECONDS - diff;
        int remaining_days = (remaining_sec < 0) ? -1 : (int)(remaining_sec / (24 * 3600));
        j["SERVER"]["GRACE_REMAINING_DAYS"] = remaining_days;
    }
    
    // 保存回文件
    std::ofstream o(CONFIG_FILE);
    if (o.is_open()) {
        o << std::setw(4) << j << std::endl;
    }
}

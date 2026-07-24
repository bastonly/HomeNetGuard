#pragma once

#include <string>
#include <map>

enum class GraceReason {
    NONE,
    INACTIVE_STATUS,
    CONNECTION_FAILED,
    TIMEOUT,
    SERVER_ERROR,
    UNKNOWN
};

class LicenseManager {
public:
    static LicenseManager& getInstance();
    
    // Constant for default grace period duration (in days)
    static constexpr int DEFAULT_GRACE_PERIOD_DAYS = 3;
    static constexpr long GRACE_PERIOD_SECONDS = DEFAULT_GRACE_PERIOD_DAYS * 24 * 3600;
    
    // Ensure that DEFAULT_GRACE_PERIOD_DAYS is written to the config on startup
    void ensure_config_defaults();
    
    // Performs checkin with the server
    // Returns true if license is active (verified or within grace period)
    bool checkin();
    
    // Performs activation with the server
    // Returns true if activation is successful
    bool activate(const std::string& code);
    
    // Checks local state only (fast check)
    bool is_license_valid() const;
    
    // Returns remaining days in grace period (-1 if online, 0 if expired)
    int get_grace_period_days() const;

    // Helper to get human-readable license status in Chinese
    static std::string get_license_status_text(int status_code);

    // Helper to get human-readable grace reason in Chinese
    static std::string get_grace_reason_text(GraceReason reason);

    // Write audit log to local db
    void log_security_event(const std::string& level, const std::string& tag, const std::string& content, const std::string& payload = "", const std::string& source_ip = "", const std::string& source_mac = "") const;

private:
    LicenseManager() = default;
    
    // Helper to update config file
    void update_local_license_info(const std::string& license_id, const std::string& status, const std::string& device_status, const std::string& expired_date, const std::string& server_time, const std::string& latest_version, long last_checkin_ts, const std::string& grace_reason, long grace_start_ts);
    
    // Helpr to handle tampering detected
    void suspend_tampered_license(const std::string& reason) const;
};

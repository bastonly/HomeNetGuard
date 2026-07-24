#pragma once

#include <string>
#include <vector>
#include <map>

// 基础文件路径与锁文件配置 / Base File Paths & Lock Configurations
const std::string CONFIG_FILE = "/opt/parent-control/backend/config/base.conf";
const std::string DB_PATH = "/opt/parent-control/backend/data/pc.db"; 
const std::string LOG_DIR = "/opt/parent-control/service/logs";
const std::string LOG_FILE_PATTERN = "service-"; // 产生日志如 / Log pattern: service-2026-02-24.log
const std::string LOG_FILE = "/opt/parent-control/service/logs/service.log"; 
const std::string LOCK_FILE = "/var/lock/pc_service.lock";
const std::string LEASE_FILE = "/tmp/dhcp.leases";

// 系统消息分类常量 / SysMessages Event Categories
namespace EventCategory {
    const std::string SECURITY = "security"; // 安全 / Security
    const std::string SYSTEM = "system";     // 系统 / System
    const std::string AUDIT = "audit";       // 审计 / Audit
}

// 系统消息级别常量 / SysMessages Event Levels
namespace EventLevel {
    const std::string INFO = "info";
    const std::string WARNING = "warning";
    const std::string ERROR = "error";
    const std::string CRITICAL = "critical";
}

// 系统消息事件标签 / SysMessages Event Tags
namespace EventTag {
    const std::string BLOCK = "BLOCK";
    const std::string LOGIN = "LOGIN";
    const std::string ACTIVATE = "ACTIVATE";
    const std::string OTA = "OTA";
    const std::string SYSTEM = "SYSTEM";
    const std::string DEVICE = "DEVICE";
}

// 调度任务配置结构体 / Scheduler Configuration Structure
struct SchedulerConfig {
    int interval_sync_nft = 60;
    int interval_device_update = 30;
    std::string adguard_analysis_time = "00:05";
    std::string entertainment_collect_time = "00:10";
    std::string cleanup_time = "00:20";
};

// Redis 连接配置结构体 / Redis Connection Configuration Structure
struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password = ""; // 默认密码为空 / Default password empty
    int db = 0;
};

// 应用全局配置结构体 / Application Global Configuration Structure
struct AppConfig {
    std::string server_url;
    std::string api_token;
    std::string license_id;
    std::string current_api_version;
    std::string jwt_secret;
    std::string adguard_url;
    std::string adguard_user;
    std::string adguard_password;
    
    SchedulerConfig scheduler;
    RedisConfig redis;
    
    std::string timezone = "Asia/Shanghai";
    std::string model;
    std::string os_version;

    // 授权状态信息 / License State Information
    std::string last_checkin_time;
    long last_checkin_ts = 0;

    // 数据保留天数 / Data Retention Policy (in Days)
    int retention_adguard_summary = 30;
    int retention_adguard_detail = 5;
    int retention_sys_messages = 15;
};

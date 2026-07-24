#include "db.hpp"
#include "utils.hpp"
#include <iostream>
#include <filesystem>

DB& DB::getInstance() {
    static DB instance;
    return instance;
}

DB::~DB() {
    close_db();
}

bool DB::init(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        Utils::Logger::error("Can't open database: " + std::string(sqlite3_errmsg(db)));
        return false;
    }
    
    // Set busy timeout to 5 seconds to handle contention
    sqlite3_busy_timeout(db, 5000);

    // Enable WAL mode and optimize synchronous setting for better I/O performance on OpenWrt
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    Utils::Logger::info("Database opened successfully: " + db_path);
    
    return true;
}

void DB::close_db() {
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool DB::exec(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    if (!db) return false;

    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown SQL Error";
        Utils::Logger::error("SQL error: " + err + " | SQL: " + sql);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::vector<std::map<std::string, std::string>> DB::query(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    std::vector<std::map<std::string, std::string>> results;
    if (!db) return results;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Utils::Logger::error("SQL prepare error: " + std::string(sqlite3_errmsg(db)) + " | SQL: " + sql);
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::map<std::string, std::string> row;
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const char* val = (const char*)sqlite3_column_text(stmt, i);
            row[name] = val ? val : "";
        }
        results.push_back(row);
    }

    sqlite3_finalize(stmt);
    return results;
}

bool DB::column_exists(const std::string& table, const std::string& column) {
    // Cannot rely on user input here, but table/column are internal strings
    std::string sql = "PRAGMA table_info(" + table + ");";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* col_ptr = (const char*)sqlite3_column_text(stmt, 1);
        if (col_ptr) {
            std::string colName(col_ptr);
            if (column == colName) {
                found = true;
                break;
            }
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

void DB::check_and_migrate_schema() {
    Utils::Logger::info("Checking database schema...");
    
    // 1. Ensure sys_messages table exists
    std::string create_table_sql = 
        "CREATE TABLE IF NOT EXISTS sys_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "category TEXT NOT NULL DEFAULT 'system', "
        "level TEXT DEFAULT 'info', "
        "event_tag TEXT DEFAULT '', "
        "source_ip TEXT, "
        "source_mac TEXT, "
        "source_user TEXT DEFAULT '', "
        "group_name TEXT, "
        "content TEXT, "
        "payload TEXT, "
        "occurrence_count INTEGER DEFAULT 1, "
        "is_read BOOLEAN DEFAULT 0, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, create_table_sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown Error";
        Utils::Logger::error("Failed to create/check sys_messages table: " + err);
        sqlite3_free(errMsg);
        return; // Critical error
    }

    // 2. Check for missing columns and migrate if necessary (for existing tables)
    // type field is removed as it's redundant. Ensure other columns exist.
    
    if (!column_exists("sys_messages", "category")) {
        Utils::Logger::info("Migrating schema: adding 'category' column");
        exec("ALTER TABLE sys_messages ADD COLUMN category TEXT NOT NULL DEFAULT 'legacy'");
    }
    
    if (!column_exists("sys_messages", "event_tag")) {
        Utils::Logger::info("Migrating schema: adding 'event_tag' column");
        exec("ALTER TABLE sys_messages ADD COLUMN event_tag TEXT DEFAULT ''");
    }

    if (!column_exists("sys_messages", "source_user")) {
        Utils::Logger::info("Migrating schema: adding 'source_user' column");
        exec("ALTER TABLE sys_messages ADD COLUMN source_user TEXT DEFAULT ''");
    }

    // Ensure devices table exists (for DeviceManager)
    const char* create_devices_sql = 
        "CREATE TABLE IF NOT EXISTS devices ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "mac TEXT NOT NULL UNIQUE,"
        "remark TEXT,"
        "status TEXT,"
        "last_seen INTEGER,"
        "ip TEXT,"
        "groups_id INTEGER,"
        "enable BOOLEAN DEFAULT 1,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    exec(create_devices_sql);
    
    // Ensure entertainment related tables (for EntertainmentManager)
    const char* create_ent_cat_sql = 
        "CREATE TABLE IF NOT EXISTS entertainment_categories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "domain TEXT UNIQUE NOT NULL,"
        "category TEXT NOT NULL,"
        "name TEXT,"
        "source TEXT DEFAULT 'auto',"
        "confidence REAL DEFAULT 0.0,"
        "last_verified DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    exec(create_ent_cat_sql);

    const char* create_ent_det_sql = 
        "CREATE TABLE IF NOT EXISTS entertainment_detections ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_id INTEGER NOT NULL,"
        "domain TEXT NOT NULL,"
        "category TEXT NOT NULL,"
        "visit_count INTEGER DEFAULT 1,"
        "first_seen TEXT,"
        "last_seen TEXT,"
        "detection_date TEXT NOT NULL,"
        "created_at TEXT DEFAULT (datetime('now', 'localtime')),"
        "FOREIGN KEY (device_id) REFERENCES devices(id)"
        ");";
    exec(create_ent_det_sql);
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_agg ON entertainment_detections (device_id, domain, detection_date);");
    
    // Cleanup any existing duplicates in entertainment_detections before creating unique index
    exec("DELETE FROM entertainment_detections WHERE id NOT IN (SELECT MIN(id) FROM entertainment_detections GROUP BY device_id, domain, detection_date);");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_det_unique ON entertainment_detections (device_id, domain, detection_date);");
    
    // 3. 重构 adguard_device_logs (采用聚合存储模式)
    // 根据用户要求，删除旧表重新建立
    if (!column_exists("adguard_device_logs", "begin_at")) {
        Utils::Logger::info("Recreating adguard_device_logs for aggregation storage...");
        exec("DROP TABLE IF EXISTS adguard_device_logs;");
        
        std::string sql_create = 
            "CREATE TABLE adguard_device_logs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "device_id INTEGER NOT NULL, "
            "domain TEXT NOT NULL, "
            "query_type TEXT, "
            "is_blocked INTEGER DEFAULT 0, "
            "count INTEGER DEFAULT 1, "
            "access_date TEXT NOT NULL, " // YYYY-MM-DD
            "begin_at TEXT, "
            "end_at TEXT, "
            "FOREIGN KEY (device_id) REFERENCES devices(id)"
            ");";
        exec(sql_create);
        exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_adguard_agg ON adguard_device_logs (device_id, domain, query_type, is_blocked, access_date);");
    }

    Utils::Logger::info("Schema check completed.");
}

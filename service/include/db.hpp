#pragma once

#include <string>
#include <vector>
#include <map>
#include <sqlite3.h>
#include <mutex>
#include <functional>
#include "common.hpp"

class DB {
public:
    static DB& getInstance();
    
    // Lifecycle
    bool init(const std::string& db_path);
    void close_db();

    // Query execution
    bool exec(const std::string& sql);
    
    // Query with results (simplified for common use cases)
    // Returns a vector of maps (column -> value)
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);
    
    // Specific helper for schema migration
    void check_and_migrate_schema();

private:
    DB() : db(nullptr) {}
    ~DB();
    
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;

    sqlite3* db;
    std::recursive_mutex db_mutex;
    
    bool column_exists(const std::string& table, const std::string& column);
};

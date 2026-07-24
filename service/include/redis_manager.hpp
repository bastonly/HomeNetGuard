#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <hiredis/hiredis.h>
#include "common.hpp"

class RedisManager {
public:
    static RedisManager& getInstance();

    bool connect();
    void disconnect();

    // Generic Commands
    bool set(const std::string& key, const std::string& value, int expire_sec = 0);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, int seconds);

    // Hash Commands
    bool hset(const std::string& dict, const std::string& key, const std::string& value);
    bool hsetnx(const std::string& dict, const std::string& key, const std::string& value);
    bool hincrby(const std::string& dict, const std::string& key, int increment);
    std::string hget(const std::string& dict, const std::string& key);
    std::map<std::string, std::string> hgetall(const std::string& dict);

    // Distributed Lock
    bool try_lock(const std::string& lock_name, int timeout_sec = 10);
    void unlock(const std::string& lock_name);

    // Atomic Counter
    long long incr(const std::string& key);

    // List Commands
    bool lpush(const std::string& list, const std::string& value);

    // Set Commands
    bool sadd(const std::string& set, const std::string& value);
    bool srem(const std::string& set, const std::string& value);
    bool sismember(const std::string& set, const std::string& value);
    std::vector<std::string> smembers(const std::string& set);


    // Connectivity check
    bool isConnected();

private:
    RedisManager();
    ~RedisManager();
    RedisManager(const RedisManager&) = delete;
    RedisManager& operator=(const RedisManager&) = delete;

    redisContext* context;
    std::mutex mtx;
    bool connected;
    
    AppConfig config;
};

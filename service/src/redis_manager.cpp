#include "redis_manager.hpp"
#include "utils.hpp"
#include <iostream>

RedisManager& RedisManager::getInstance() {
    static RedisManager instance;
    return instance;
}

RedisManager::RedisManager() : context(nullptr), connected(false) {
    config = Utils::Config::load_config();
}

RedisManager::~RedisManager() {
    disconnect();
}

bool RedisManager::connect() {
    std::lock_guard<std::mutex> lock(mtx);
    if (connected && context) return true;

    if (context) {
        redisFree(context);
        context = nullptr;
    }

    struct timeval timeout = { 0, 500000 }; // 0.5 seconds
    context = redisConnectWithTimeout(config.redis.host.c_str(), config.redis.port, timeout);

    if (context == nullptr || context->err) {
        if (context) {
            Utils::Logger::error("Redis connection error: " + std::string(context->errstr));
            redisFree(context);
            context = nullptr;
        } else {
            Utils::Logger::error("Redis connection error: can't allocate redis context");
        }
        connected = false;
        return false;
    }

    // Authenticate if password provided
    if (!config.redis.password.empty()) {
        redisReply* reply = (redisReply*)redisCommand(context, "AUTH %s", config.redis.password.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            Utils::Logger::error("Redis authentication failed");
            if (reply) freeReplyObject(reply);
            redisFree(context);
            context = nullptr;
            connected = false;
            return false;
        }
        freeReplyObject(reply);
    }

    // Select DB
    if (config.redis.db != 0) {
        redisReply* reply = (redisReply*)redisCommand(context, "SELECT %d", config.redis.db);
        if (reply) freeReplyObject(reply);
    }

    connected = true;
    Utils::Logger::info("Connected to Redis at " + config.redis.host + ":" + std::to_string(config.redis.port));
    return true;
}

void RedisManager::disconnect() {
    std::lock_guard<std::mutex> lock(mtx);
    if (context) {
        redisFree(context);
        context = nullptr;
    }
    connected = false;
}

bool RedisManager::isConnected() {
    return connected && context != nullptr;
}

bool RedisManager::set(const std::string& key, const std::string& value, int expire_sec) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply;
    if (expire_sec > 0) {
        reply = (redisReply*)redisCommand(context, "SETEX %s %d %b", key.c_str(), expire_sec, value.c_str(), (size_t)value.length());
    } else {
        reply = (redisReply*)redisCommand(context, "SET %s %b", key.c_str(), value.c_str(), (size_t)value.length());
    }

    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

std::string RedisManager::get(const std::string& key) {
    if (!connect()) return "";
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "GET %s", key.c_str());
    std::string result = "";
    if (reply && reply->type == REDIS_REPLY_STRING) {
        result = reply->str;
    }
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisManager::del(const std::string& key) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "DEL %s", key.c_str());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::expire(const std::string& key, int seconds) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "EXPIRE %s %d", key.c_str(), seconds);
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::hset(const std::string& dict, const std::string& key, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "HSET %s %s %b", dict.c_str(), key.c_str(), value.c_str(), (size_t)value.length());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::hsetnx(const std::string& dict, const std::string& key, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "HSETNX %s %s %b", dict.c_str(), key.c_str(), value.c_str(), (size_t)value.length());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::hincrby(const std::string& dict, const std::string& key, int increment) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "HINCRBY %s %s %d", dict.c_str(), key.c_str(), increment);
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

std::string RedisManager::hget(const std::string& dict, const std::string& key) {
    if (!connect()) return "";
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "HGET %s %s", dict.c_str(), key.c_str());
    std::string result = "";
    if (reply && reply->type == REDIS_REPLY_STRING) {
        result = reply->str;
    }
    if (reply) freeReplyObject(reply);
    return result;
}

std::map<std::string, std::string> RedisManager::hgetall(const std::string& dict) {
    std::map<std::string, std::string> result;
    if (!connect()) return result;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "HGETALL %s", dict.c_str());
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                result[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }
    if (reply) freeReplyObject(reply);
    return result;
}

long long RedisManager::incr(const std::string& key) {
    if (!connect()) return -1;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "INCR %s", key.c_str());
    long long result = -1;
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    }
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisManager::try_lock(const std::string& lock_name, int timeout_sec) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string key = "lock:" + lock_name;
    // SET lock:name 1 NX EX timeout
    redisReply* reply = (redisReply*)redisCommand(context, "SET %s 1 NX EX %d", key.c_str(), timeout_sec);
    
    bool success = (reply && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK");
    if (reply) freeReplyObject(reply);
    return success;
}

void RedisManager::unlock(const std::string& lock_name) {
    if (!connect()) return;
    std::lock_guard<std::mutex> lock(mtx);
    std::string key = "lock:" + lock_name;
    redisReply* reply = (redisReply*)redisCommand(context, "DEL %s", key.c_str());
    if (reply) freeReplyObject(reply);
}

bool RedisManager::lpush(const std::string& list, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "LPUSH %s %b", list.c_str(), value.c_str(), (size_t)value.length());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::sadd(const std::string& set, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "SADD %s %b", set.c_str(), value.c_str(), (size_t)value.length());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::srem(const std::string& set, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "SREM %s %b", set.c_str(), value.c_str(), (size_t)value.length());
    bool success = (reply && reply->type != REDIS_REPLY_ERROR);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisManager::sismember(const std::string& set, const std::string& value) {
    if (!connect()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "SISMEMBER %s %s", set.c_str(), value.c_str());
    bool result = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (reply) freeReplyObject(reply);
    return result;
}

std::vector<std::string> RedisManager::smembers(const std::string& set) {
    std::vector<std::string> result;
    if (!connect()) return result;
    std::lock_guard<std::mutex> lock(mtx);
    
    redisReply* reply = (redisReply*)redisCommand(context, "SMEMBERS %s", set.c_str());
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            result.push_back(reply->element[i]->str);
        }
    }
    if (reply) freeReplyObject(reply);
    return result;
}


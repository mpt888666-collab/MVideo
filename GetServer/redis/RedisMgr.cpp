//
// Created by mpt on 2026/10/15.
//

#include "redis/RedisMgr.h"
#include "config/ConfigMgr.h"
#include <iostream>
#include <thread>

RedisMgr::RedisMgr() {
    try {
        auto& cfg = ConfigMgr::Inst();
        auto host = cfg["RedisServer"]["Host"];
        auto port = stoi(cfg["RedisServer"]["Port"]);
        auto db = stoi(cfg["RedisServer"]["Db"]);
        int size = 0;
        try {
            const auto configured = cfg["RedisServer"]["Size"];
            if (!configured.empty())
                size = stoi(configured);
        } catch (...) {
        }
        if (size <= 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            size = hardware > 0 ? static_cast<int>(hardware) : 16;
        }
        if (size < 8)
            size = 8;
        if (size > 64)
            size = 64;

        sw::redis::ConnectionOptions options;
        options.host = host;
        options.port = port;
        options.db = db;
        options.keep_alive = true;
        options.connect_timeout = std::chrono::milliseconds(500);
        options.socket_timeout = std::chrono::milliseconds(500);

        sw::redis::ConnectionPoolOptions pool;
        pool.size = size;
        pool.wait_timeout = std::chrono::milliseconds(500);

        _redis = std::make_unique<sw::redis::Redis>(options, pool);

        // redis++ 是懒连接：构造成功不代表连得上，这里真发一次 PING 才能确认
        _redis->ping();
        std::cout << "RedisMgr init success!" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "RedisMgr init fail: " << e.what() << std::endl;
        _redis.reset();
    }
}

bool RedisMgr::Set(const std::string& key, const std::string& value) {
    return SetEx(key, value, std::chrono::seconds(0));
}

bool RedisMgr::SetEx(const std::string& key, const std::string& value, const std::chrono::seconds ttl) {
    if (!_redis) {
        std::cerr << "RedisMgr not connected, Set(" << key << ") skipped" << std::endl;
        return false;
    }
    try {
        if (ttl.count() > 0) {
            _redis->set(key, value, ttl);
        } else {
            _redis->set(key, value);
        }
        return true;
    } catch (const sw::redis::Error& e) {
        std::cerr << "Redis set [" << key << "] failed: " << e.what() << std::endl;
        return false;
    }
}

LimiterResult RedisMgr::SetNx(const std::string& key, const std::string& value, const std::chrono::seconds ttl) {
    if (!_redis) {
        std::cerr << "RedisMgr not connected, SetNx(" << key << ") skipped" << std::endl;
        return LimiterError;
    }
    try {
        const bool ok = _redis->set(key, value, ttl, sw::redis::UpdateType::NOT_EXIST);
        return ok ? LimiterAllowed : LimiterDenied;
    } catch (const sw::redis::Error& e) {
        std::cerr << "Redis setnx [" << key << "] failed: " << e.what() << std::endl;
        return LimiterError;
    }
}

bool RedisMgr::Get(const std::string& key, std::string& value) {
    if (!_redis) {
        std::cerr << "RedisMgr not connected, Get(" << key << ") skipped" << std::endl;
        return false;
    }
    try {
        auto val = _redis->get(key);
        if (!val) {
            return false;
        }
        value = *val;
        return true;
    } catch (const sw::redis::Error& e) {
        std::cerr << "Redis get [" << key << "] failed: " << e.what() << std::endl;
        return false;
    }
}

long long RedisMgr::IncrWithTtl(const std::string& key, std::chrono::seconds ttl) {
    if (!_redis)
        return -1;
    static const std::string kScript = R"(
        local n = redis.call('INCR', KEYS[1])
        if n == 1 then redis.call('EXPIRE', KEYS[1], ARGV[1]) end
        return n
    )";
    try {
        return _redis->eval<long long>(kScript, {key}, {std::to_string(ttl.count())});
    } catch (const std::exception& e) {
        std::cerr << "[Redis] Incr err: " << e.what() << std::endl;
        return -1;
    }
}

bool RedisMgr::Del(const std::string& key) {
    if (!_redis)
        return false;
    try {
        return _redis->del(key) > 0;
    } catch (const std::exception& e) {
        std::cerr << "[Redis] Del err: " << e.what() << std::endl;
        return false;
    }
}

long long RedisMgr::EvalInt(
        const std::string& script, const std::vector<std::string>& keys, const std::vector<std::string>& args
) {
    if (!_redis)
        return -1;
    try {
        return _redis->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
    } catch (const std::exception& e) {
        std::cerr << "[Redis] EvalInt err: " << e.what() << std::endl;
        return -1;
    }
}

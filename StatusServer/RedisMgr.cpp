//
// Created by mpt on 2026/10/15.
//

#include "RedisMgr.h"

#include "ConfigMgr.h"

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
            if (!configured.empty()) size = stoi(configured);
        } catch (...) {
        }
        if (size <= 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            size = hardware > 0 ? static_cast<int>(hardware) : 16;
        }
        if (size < 8) size = 8;
        if (size > 64) size = 64;

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

        // redis++ connects lazily; PING verifies the configuration at startup.
        _redis->ping();
        std::cout << "RedisMgr init success!" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "RedisMgr init fail: " << e.what() << std::endl;
        _redis.reset();
    }
}

std::vector<std::string> RedisMgr::EvalStrings(
        const std::string& script,
        const std::vector<std::string>& keys,
        const std::vector<std::string>& args) {
    if (!_redis) {
        return {};
    }
    try {
        return _redis->eval<std::vector<std::string>>(
                script, keys.begin(), keys.end(), args.begin(), args.end());
    } catch (const std::exception& e) {
        std::cerr << "[Redis] EvalStrings err: " << e.what() << std::endl;
        return {};
    }
}

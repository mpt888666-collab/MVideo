//
// Created by mpt on 2026/10/19.
//

#ifndef GETSERVER_COMMON_H
#define GETSERVER_COMMON_H
#include "config/ConfigMgr.h"
static constexpr int kRpcDeadlineMs = 3000;

namespace rpc {
inline std::size_t RpcThreadCount() {
    static const std::size_t count = [] {
        int value = 0;
        try {
            value = std::stoi(ConfigMgr::Inst()["GetServer"]["RpcThreads"]);
        } catch (...) {
        }
        if (value <= 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            value = hardware > 0 ? static_cast<int>(hardware / 2) : 8;
            if (value < 8)
                value = 8;
            if (value > 16)
                value = 16;
        }
        if (value > 32)
            value = 32;
        return static_cast<std::size_t>(value);
    }();
    return count;
}

inline std::size_t RpcQueueLimit() {
    static const std::size_t limit = [] {
        int value = 0;
        try {
            value = std::stoi(ConfigMgr::Inst()["GetServer"]["RpcQueueLimit"]);
        } catch (...) {
        }
        if (value < 32)
            value = 32;
        if (value > 2048)
            value = 2048;
        return static_cast<std::size_t>(value);
    }();
    return limit;
}

inline boost::asio::thread_pool& RpcWorkerPool() {
    static boost::asio::thread_pool pool(RpcThreadCount());
    return pool;
}

inline std::atomic<std::size_t> g_rpcQueued{0};
constexpr int kRpcQueueBudgetMs = 2500;
}  // namespace rpc
#endif  // GETSERVER_COMMON_H

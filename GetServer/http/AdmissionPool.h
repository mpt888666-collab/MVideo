#ifndef GETSERVER_ADMISSIONPOOL_H
#define GETSERVER_ADMISSIONPOOL_H

#include "config/ConfigMgr.h"

#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <cstddef>
#include <thread>

namespace admission {
inline std::size_t ThreadCount() {
    static const std::size_t count = [] {
        int value = 0;
        try {
            value = std::stoi(ConfigMgr::Inst()["GetServer"]["AdmissionThreads"]);
        } catch (...) {
        }
        if (value <= 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            value = hardware > 0 ? static_cast<int>(hardware / 8) : 4;
            if (value < 4)
                value = 4;
            if (value > 8)
                value = 8;
        }
        if (value < 2)
            value = 2;
        if (value > 16)
            value = 16;
        return static_cast<std::size_t>(value);
    }();
    return count;
}

inline std::size_t QueueLimit() {
    static const std::size_t limit = [] {
        int value = 0;
        try {
            value = std::stoi(ConfigMgr::Inst()["GetServer"]["AdmissionQueueLimit"]);
        } catch (...) {
        }
        if (value <= 0)
            value = 1024;
        if (value < 64)
            value = 64;
        if (value > 8192)
            value = 8192;
        return static_cast<std::size_t>(value);
    }();
    return limit;
}

inline boost::asio::thread_pool& WorkerPool() {
    static boost::asio::thread_pool pool(ThreadCount());
    return pool;
}

inline std::atomic<std::size_t> queued{0};
constexpr int kQueueBudgetMs = 1000;

}  // namespace admission
#endif  // GETSERVER_ADMISSIONPOOL_H

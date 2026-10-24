//
// Shared bounded blocking-work pools.
//

#ifndef CHATSERVER1_THREADPOOL_H
#define CHATSERVER1_THREADPOOL_H

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include "config/ConfigMgr.h"

namespace bounded_pool {

inline std::size_t ResolveCount(
        const std::string& section, const std::string& key, std::size_t fallback, std::size_t min_value,
        std::size_t max_value
) {
    std::size_t value = 0;
    try {
        auto text = ConfigMgr::Inst()[section][key];
        if (!text.empty()) {
            value = static_cast<std::size_t>(std::stoul(text));
        }
    } catch (...) {
    }
    if (value == 0) {
        value = fallback;
    }
    if (value < min_value) {
        value = min_value;
    }
    if (value > max_value) {
        value = max_value;
    }
    return value;
}

inline bool TrySubmit(
        boost::asio::thread_pool& pool, std::atomic<std::size_t>& queued, std::size_t queue_limit,
        std::function<void()> task
) {
    if (!task) {
        return false;
    }

    const std::size_t current = queued.fetch_add(1, std::memory_order_relaxed);
    if (current >= queue_limit) {
        queued.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }

    try {
        boost::asio::post(pool.get_executor(), [&queued, task = std::move(task)]() mutable {
            struct QueueGuard {
                std::atomic<std::size_t>& counter;

                ~QueueGuard() { counter.fetch_sub(1, std::memory_order_relaxed); }
            } guard{queued};

            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[Pool] task exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[Pool] task unknown exception" << std::endl;
            }
        });
        return true;
    } catch (const std::exception& e) {
        queued.fetch_sub(1, std::memory_order_relaxed);
        std::cerr << "[Pool] submit failed: " << e.what() << std::endl;
        return false;
    } catch (...) {
        queued.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }
}

}  // namespace bounded_pool

namespace redisPool {

inline std::size_t GetThreadCount() {
    static const std::size_t count = bounded_pool::ResolveCount("RedisServer", "RedisWorkThread", 4, 1, 8);
    return count;
}

inline std::size_t RedisPoolQueueLimit() {
    static const std::size_t limit =
            bounded_pool::ResolveCount("RedisServer", "RedisWorkQueueLimit", 1024, 32, 8192);
    return limit;
}

inline boost::asio::thread_pool& GetRedisPool() {
    static boost::asio::thread_pool pool(GetThreadCount());
    return pool;
}

inline std::atomic<std::size_t> redisQueued{0};
inline std::atomic<std::size_t> redisRejected{0};
constexpr int kRedisQueueBudgetMs = 1500;

inline bool TrySubmit(std::function<void()> task) {
    if (bounded_pool::TrySubmit(GetRedisPool(), redisQueued, RedisPoolQueueLimit(), std::move(task))) {
        return true;
    }
    redisRejected.fetch_add(1, std::memory_order_relaxed);
    return false;
}

}  // namespace redisPool

namespace cleanupPool {

inline std::size_t GetThreadCount() {
    static const std::size_t count = bounded_pool::ResolveCount("RedisServer", "RedisCleanupThreads", 2, 1, 4);
    return count;
}

inline std::size_t CleanupPoolQueueLimit() {
    static const std::size_t limit =
            bounded_pool::ResolveCount("RedisServer", "RedisCleanupQueueLimit", 2048, 64, 8192);
    return limit;
}

inline boost::asio::thread_pool& GetCleanupPool() {
    static boost::asio::thread_pool pool(GetThreadCount());
    return pool;
}

inline std::atomic<std::size_t> cleanupQueued{0};
inline std::atomic<std::size_t> cleanupRejected{0};

inline bool TrySubmit(std::function<void()> task) {
    if (bounded_pool::TrySubmit(GetCleanupPool(), cleanupQueued, CleanupPoolQueueLimit(), std::move(task))) {
        return true;
    }
    cleanupRejected.fetch_add(1, std::memory_order_relaxed);
    return false;
}

}  // namespace cleanupPool

#endif  // CHATSERVER1_THREADPOOL_H

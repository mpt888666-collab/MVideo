#ifndef GETSERVER_REDIS_RATELIMITER_H
#define GETSERVER_REDIS_RATELIMITER_H

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

class RedisMgr;

namespace redis {
struct RateRule {
    std::string key;
    long long limit;
    std::chrono::seconds window;
};

class RateLimiter {
public:
    // 返回 0=放行；>0=命中的规则下标(1-based)；<0=Redis 错误
    static int Acquire(RedisMgr& redis, const std::vector<RateRule>& rules);

    // 释放时先删除前 lock_count 个锁 key，再递减后面的计数 key
    static void Release(RedisMgr& redis, std::size_t lock_count, const std::vector<std::string>& keys);
};
}  // namespace redis
#endif  // GETSERVER_REDIS_RATELIMITER_H

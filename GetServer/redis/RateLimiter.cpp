#include "redis/RateLimiter.h"

#include "redis/RedisMgr.h"

#include <string>
#include <vector>

namespace redis {
int RateLimiter::Acquire(RedisMgr& redis, const std::vector<RateRule>& rules) {
    static const std::string kScript = R"(
        -- KEYS[i] = 计数 key；ARGV[2i-1] = limit，ARGV[2i] = windowSec
        for i = 1, #KEYS do
            local cur = tonumber(redis.call('GET', KEYS[i]) or '0')
            if cur >= tonumber(ARGV[2 * i - 1]) then return i end
        end
        for i = 1, #KEYS do
            local n = redis.call('INCR', KEYS[i])
            if n == 1 then redis.call('EXPIRE', KEYS[i], ARGV[2 * i]) end
        end
        return 0
    )";

    std::vector<std::string> keys;
    std::vector<std::string> args;
    keys.reserve(rules.size());
    args.reserve(rules.size() * 2);
    for (const auto& rule : rules) {
        keys.push_back(rule.key);
        args.push_back(std::to_string(rule.limit));
        args.push_back(std::to_string(rule.window.count()));
    }
    return static_cast<int>(redis.EvalInt(kScript, keys, args));
}

void RateLimiter::Release(RedisMgr& redis, std::size_t lock_count, const std::vector<std::string>& keys) {
    static const std::string kScript = R"(
        -- ARGV[1] = lock_count；前 lock_count 个 KEYS 是锁，后面是计数
        local lock_count = tonumber(ARGV[1])
        for i = 1, lock_count do
            redis.call('DEL', KEYS[i])
        end
        for i = lock_count + 1, #KEYS do
            local v = tonumber(redis.call('GET', KEYS[i]) or '0')
            if v > 0 then
                v = redis.call('DECR', KEYS[i])
                if v <= 0 then redis.call('DEL', KEYS[i]) end
            end
        end
        return 1
    )";
    redis.EvalInt(kScript, keys, {std::to_string(lock_count)});
}
}  // namespace redis

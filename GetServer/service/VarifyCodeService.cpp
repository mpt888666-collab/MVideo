#include "service/VarifyCodeService.h"

#include "redis/RedisMgr.h"
#include "const.h"
#include "redis/RateLimiter.h"
#include "redis/VarifyCodeStore.h"

#include <chrono>
#include <string>
#include <vector>

namespace service {
VarifyCodeService::SlotResult VarifyCodeService::AcquireSlot(
        RedisMgr& redis, const std::string& email, const std::string& ip
) {
    if (redis::VarifyCodeStore::Has(redis, email)) {
        return SlotResult::CacheHit;
    }

    LimiterResult lock = redis.SetNx(VarifyCodeCdKey + email, "1", std::chrono::seconds(kMailCooldown));
    if (lock == LimiterError) {
        return SlotResult::RedisError;
    }
    if (lock == LimiterDenied) {
        return SlotResult::MailCooldown;
    }

    lock = redis.SetNx(IpCdKey + ip, "1", std::chrono::seconds(kIpCooldown));
    if (lock != LimiterAllowed) {
        redis.Del(VarifyCodeCdKey + email);
        return lock == LimiterError ? SlotResult::RedisError : SlotResult::IpCooldown;
    }

    const auto now_tp = std::chrono::system_clock::now().time_since_epoch();
    const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now_tp).count();
    const std::vector<redis::RateRule> rules{
            {ServerCdKey + std::to_string(now_sec), kGlobalPerSec, std::chrono::seconds(2)},
            {VarifyCodeCdKeyDay + email, kMailPerDay, std::chrono::seconds(86400)},
            {IpCdKeyHour + ip, kIpPerHour, std::chrono::seconds(3600)},
            {IpCdKeyDay + ip, kIpPerDay, std::chrono::seconds(86400)},
    };

    const int hit = redis::RateLimiter::Acquire(redis, rules);
    if (hit == 0) {
        return SlotResult::Allowed;
    }

    redis.Del(VarifyCodeCdKey + email);
    redis.Del(IpCdKey + ip);

    if (hit < 0) {
        return SlotResult::RedisError;
    }
    return hit == 1 ? SlotResult::GlobalLimited : SlotResult::QuotaExceeded;
}

void VarifyCodeService::ReleaseSlot(RedisMgr& redis, const std::string& email, const std::string& ip) {
    const std::vector<std::string> keys{
            VarifyCodeCdKey + email, IpCdKey + ip, VarifyCodeCdKeyDay + email, IpCdKeyHour + ip, IpCdKeyDay + ip
    };
    redis::RateLimiter::Release(redis, 2, keys);
}
}  // namespace service

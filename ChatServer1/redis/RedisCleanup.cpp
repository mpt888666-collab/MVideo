//
// Asynchronous release of Redis login state.
//

#include "redis/RedisCleanup.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "common/ThreadPool.h"
#include "redis/ChatRedis.h"
#include "redis/RedisMgr.h"

namespace {

void ReleaseTokenNow(const std::string& token) {
    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        std::cerr << "[RedisCleanup] redis is not available" << std::endl;
        return;
    }

    const auto result = redis->EvalInts(
            chat_redis::kReleaseLoginScript, {chat_redis::ActiveKey(), chat_redis::TokenKey(token)},
            std::vector<std::string>{}
    );

    if (result.empty()) {
        std::cerr << "[RedisCleanup] release failed for token " << token << std::endl;
    }
}

}  // namespace

bool redis_cleanup::QueueRelease(std::string token) {
    if (token.empty()) {
        return true;
    }

    return cleanupPool::TrySubmit([token = std::move(token)]() { ReleaseTokenNow(token); });
}

void redis_cleanup::ResetServerState() {
    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return;
    }

    const auto result = redis->EvalInts(
            chat_redis::kResetServerStateScript, {chat_redis::ActiveKey(), chat_redis::PendingKey()},
            std::vector<std::string>{}
    );
    if (result.empty()) {
        std::cerr << "[RedisCleanup] reset server state failed" << std::endl;
        return;
    }

    std::cout << "[RedisCleanup] reset active/pending state for " << chat_redis::ServerName() << std::endl;
}

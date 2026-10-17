#include "redis/VarifyCodeStore.h"

#include "redis/RedisMgr.h"
#include "const.h"

#include <string>

namespace redis {

bool VarifyCodeStore::Has(RedisMgr& redis, const std::string& email) {
    std::string code;
    return redis.Get(VarifyCodeKey + email, code);
}

VarifyCodeStatus VarifyCodeStore::Check(RedisMgr& redis, const std::string& email,
                                        const std::string& code) {
    static const std::string kScript = R"(
        local v = redis.call('GET', KEYS[1])
        if not v then return 2 end
        if string.lower(v) == string.lower(ARGV[1]) then return 1 end
        return 0
    )";
    const long long result = redis.EvalInt(kScript, {VarifyCodeKey + email}, {code});
    switch (result) {
        case 1:
            return VarifyCodeStatus::Match;
        case 0:
            return VarifyCodeStatus::Mismatch;
        case 2:
            return VarifyCodeStatus::NotFound;
        default:
            return VarifyCodeStatus::Error;
    }
}

void VarifyCodeStore::Consume(RedisMgr& redis, const std::string& email,
                              const std::string& code) {
    static const std::string kScript = R"(
        local v = redis.call('GET', KEYS[1])
        if v and string.lower(v) == string.lower(ARGV[1]) then
            redis.call('DEL', KEYS[1])
            return 1
        end
        return 0
    )";
    redis.EvalInt(kScript, {VarifyCodeKey + email}, {code});
}

} // namespace redis

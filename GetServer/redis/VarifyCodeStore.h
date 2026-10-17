#ifndef GETSERVER_REDIS_VARIFYCODESTORE_H
#define GETSERVER_REDIS_VARIFYCODESTORE_H

#include <string>

class RedisMgr;

namespace redis {

enum class VarifyCodeStatus {
    Match,
    Mismatch,
    NotFound,
    Error
};

class VarifyCodeStore {
public:
    static bool Has(RedisMgr& redis, const std::string& email);

    // 只校验，不消费验证码
    static VarifyCodeStatus Check(RedisMgr& redis, const std::string& email,
                                  const std::string& code);

    // 校验通过才删除，避免并发请求把新验证码误删
    static void Consume(RedisMgr& redis, const std::string& email,
                        const std::string& code);
};

} // namespace redis

#endif // GETSERVER_REDIS_VARIFYCODESTORE_H

#ifndef GETSERVER_SERVICE_VARIFYCODESERVICE_H
#define GETSERVER_SERVICE_VARIFYCODESERVICE_H

#include <string>

class RedisMgr;

namespace service {

    class VarifyCodeService {
    public:
        enum class SlotResult {
            Allowed,
            GlobalLimited,
            MailCooldown,
            IpCooldown,
            QuotaExceeded,
            CacheHit,
            RedisError
        };

        static SlotResult AcquireSlot(RedisMgr& redis, const std::string& email,
                                      const std::string& ip);

        static void ReleaseSlot(RedisMgr& redis, const std::string& email,
                                const std::string& ip);
    };

}

#endif // GETSERVER_SERVICE_VARIFYCODESERVICE_H

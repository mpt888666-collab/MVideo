//
// Redis key layout and atomic scripts shared by ChatServer modules.
//

#ifndef CHATSERVER1_CHATREDIS_H
#define CHATSERVER1_CHATREDIS_H

#include "config/ConfigMgr.h"

#include <string>

namespace chat_redis {

inline std::string ServerName() {
    try {
        auto name = ConfigMgr::Inst()["SelfServer"]["Name"];
        if (!name.empty()) {
            return name;
        }
    } catch (...) {
    }
    return "ChatServer1";
}

inline std::string ActiveKey() {
    return "chat:active:" + ServerName();
}

inline std::string PendingKey() {
    return "chat:pending:" + ServerName();
}

inline std::string TokenKey(const std::string& token) {
    return "chat:token:" + token;
}

// Consume the lease issued by StatusServer. The whole check + reservation
// release + active increment is one Redis atomic operation.
//
// KEYS[1] = chat:pending:<server>
// KEYS[2] = chat:active:<server>
// KEYS[3] = chat:token:<token>
// ARGV[1] = token
// ARGV[2] = user_id
// ARGV[3] = server name
// ARGV[4] = session id
//
// Returns {1, active} on success.
// -2 token missing, -3 wrong server, -4 wrong user, -5 already consumed,
// -6 lease expired, -7 pending reservation missing.
inline constexpr char kConsumeLoginScript[] = R"lua(
if redis.replicate_commands then
    redis.replicate_commands()
end

local token_key = KEYS[3]
local server = redis.call('HGET', token_key, 'server')
if not server then
    return {-2}
end
if server ~= ARGV[3] then
    return {-3}
end
if redis.call('HGET', token_key, 'user_id') ~= ARGV[2] then
    return {-4}
end
if redis.call('HGET', token_key, 'state') ~= 'pending' then
    return {-5}
end

local t = redis.call('TIME')
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)
local expire_at = tonumber(redis.call('HGET', token_key, 'expire_at') or '0')
if expire_at <= now then
    redis.call('ZREM', KEYS[1], ARGV[1])
    redis.call('DEL', token_key)
    return {-6}
end

if redis.call('ZREM', KEYS[1], ARGV[1]) == 0 then
    redis.call('DEL', token_key)
    return {-7}
end

local active = redis.call('INCR', KEYS[2])
redis.call('HSET', token_key,
    'state', 'active',
    'session_id', ARGV[4],
    'active_at', string.format('%.0f', now))
redis.call('PEXPIRE', token_key, 86400000)
return {1, active}
)lua";

// Release one active connection. Idempotent: a token may be released by the
// normal close path and by a failed hand-off at the same time, but active must
// only be decremented once.
//
// KEYS[1] = chat:active:<server>
// KEYS[2] = chat:token:<token>
inline constexpr char kReleaseLoginScript[] = R"lua(
local active = tonumber(redis.call('GET', KEYS[1]) or '0')
if redis.call('EXISTS', KEYS[2]) == 0 then
    return {0, active}
end

redis.call('DEL', KEYS[2])
if active > 0 then
    active = redis.call('DECR', KEYS[1])
end
return {1, active}
)lua";

// A ChatServer process owns its local active/pending counters. Clearing them on
// startup prevents a crash or forced restart from permanently inflating the
// load reported to StatusServer.
//
// KEYS[1] = chat:active:<server>
// KEYS[2] = chat:pending:<server>
inline constexpr char kResetServerStateScript[] = R"lua(
redis.call('DEL', KEYS[1], KEYS[2])
return {1}
)lua";

}  // namespace chat_redis

#endif  // CHATSERVER1_CHATREDIS_H

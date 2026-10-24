//
// Created by mpt on 2026/10/19.
//

#include "StatusServerImp.h"

#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "const.h"

#include <boost/asio/post.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kLeaseTtlSeconds = 30;
constexpr int kLeaseTtlMs = kLeaseTtlSeconds * 1000;
constexpr int kReplaySafetyMarginMs = 5000;

std::string Trim(std::string value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool IsSafeRequestId(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '-' || c == '_';
    });
}

bool IsValidPort(const std::string& value) {
    if (value.empty() || value.size() > 5) {
        return false;
    }
    if (!std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) {
        return false;
    }
    const int port = std::stoi(value);
    return port > 0 && port <= 65535;
}

std::string GenerateToken() {
    thread_local std::random_device rd;
    static constexpr char kHex[] = "0123456789abcdef";

    std::string token;
    token.reserve(32);
    for (int i = 0; i < 16; ++i) {
        const unsigned int byte = rd() & 0xFFu;
        token.push_back(kHex[byte >> 4]);
        token.push_back(kHex[byte & 0x0Fu]);
    }
    return token;
}
constexpr const char* kSelectServerScript = R"lua(
-- Redis 5/6 need effect replication before TIME is used in a script.
-- Redis 7 makes this a no-op, and older servers simply do not expose it.
if redis.replicate_commands then
    redis.replicate_commands()
end

local ttl_ms = tonumber(ARGV[1])
local token = ARGV[2]
local user_id = ARGV[3]
local request_id = ARGV[4]
local n = tonumber(ARGV[5])

if not n or n <= 0 then
    return {'0'}
end

local redis_time = redis.call('TIME')
local now = tonumber(redis_time[1]) * 1000
        + math.floor(tonumber(redis_time[2]) / 1000)

local function configured_index(name)
    for i = 1, n do
        if ARGV[5 + i] == name then
            return i
        end
    end
    return nil
end

local req_key = nil
if request_id ~= '' then
    req_key = 'chat:req:' .. user_id .. ':' .. request_id
    local old_token = redis.call('GET', req_key)
    if old_token then
        local token_key = 'chat:token:' .. old_token
        local old_server = redis.call('HGET', token_key, 'server')
        local old_expire = tonumber(
            redis.call('HGET', token_key, 'expire_at') or '0')
        local old_state = redis.call('HGET', token_key, 'state')

        -- Reuse only a lease that is still comfortably valid and still points
        -- at a configured server. A token that is about to expire is replaced
        -- so the client does not receive one that dies during the handshake.
        if old_server and old_expire > now + 5000
                and configured_index(old_server) then
            return {'1', old_server, old_token, '1'}
        end

        -- Roll back only a pending reservation. An active session must not be
        -- touched by a retried login request.
        if old_server and old_state == 'pending' then
            redis.call('ZREM', 'chat:pending:' .. old_server, old_token)
            redis.call('DEL', token_key)
        end
        redis.call('DEL', req_key)
    end
end

local best = 0
local best_load = -1
local start = (now % n) + 1

for k = 0, n - 1 do
    local i = ((start + k - 1) % n) + 1
    local name = ARGV[5 + i]
    local pending_key = 'chat:pending:' .. name

    -- Drop leases from clients that never connected.
    redis.call('ZREMRANGEBYSCORE', pending_key, '-inf', now)

    local active = tonumber(redis.call('GET', 'chat:active:' .. name) or '0')
    local pending = tonumber(redis.call('ZCARD', pending_key))
    local load = active + pending

    if best == 0 or load < best_load then
        best = i
        best_load = load
    end
end

if best == 0 then
    return {'0'}
end

local name = ARGV[5 + best]
local expire_at = now + ttl_ms

redis.call('ZADD', 'chat:pending:' .. name, expire_at, token)
redis.call('HSET', 'chat:token:' .. token,
    'server', name,
    'user_id', user_id,
    'request_id', request_id,
    'state', 'pending',
    'expire_at', string.format('%.0f', expire_at))
redis.call('PEXPIRE', 'chat:token:' .. token, ttl_ms)

if req_key then
    redis.call('SET', req_key, token, 'PX', ttl_ms)
end

return {'1', name, token, '0'}
)lua";

} // namespace

StatusServerImp::StatusServerImp(boost::asio::thread_pool& pool) : _pool(pool) {
    auto& cfg = ConfigMgr::Inst();

    std::stringstream server_list(cfg["ChatServers"]["Name"]);
    std::string item;
    while (std::getline(server_list, item, ',')) {
        const std::string section_name = Trim(item);
        if (section_name.empty()) {
            continue;
        }

        auto section = cfg[section_name];
        ChatServer server;
        server.name = Trim(section["Name"]);
        if (server.name.empty()) {
            server.name = section_name;
        }
        server.host = Trim(section["Host"]);
        server.port = Trim(section["Port"]);

        if (server.host.empty() || !IsValidPort(server.port)) {
            LogThrottled("StatusServer config",
                         "invalid chat server: " + section_name);
            continue;
        }
        if (_chatServerMap.find(server.name) != _chatServerMap.end()) {
            LogThrottled("StatusServer config",
                         "duplicate chat server: " + server.name);
            continue;
        }

        _chatServers.push_back(server);
        _chatServerMap[server.name] = server;
    }

    if (_chatServers.empty()) {
        LogThrottled("StatusServer config", "no chat server configured");
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        LogThrottled("StatusServer", "Redis is not available");
    }
}

grpc::ServerUnaryReactor* StatusServerImp::GetChatServer(
        grpc::CallbackServerContext* context,
        const message::GetChatServerReq* request,
        message::GetChatServerRsp* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    const unsigned long long user_id = request->user_id();
    std::string request_id = request->request_id();
    if (!request_id.empty() && !IsSafeRequestId(request_id)) {
        LogThrottled("StatusServer", "invalid request_id, idempotency disabled");
        request_id.clear();
    }

    boost::asio::post(
            _pool,
            [this, reactor, response, user_id, request_id]() {
                bool finished = false;
                auto finish = [&finished, reactor, response](int error) {
                    if (finished) {
                        return;
                    }
                    response->set_error(error);
                    finished = true;
                    reactor->Finish(grpc::Status::OK);
                };

                try {
                    auto redis = RedisMgr::GetInstance();
                    if (!redis->IsValid()) {
                        finish(ErrorCodes::RPCFailed);
                        return;
                    }
                    if (_chatServers.empty()) {
                        finish(ErrorCodes::RPCFailed);
                        return;
                    }

                    const std::string token = GenerateToken();

                    std::vector<std::string> args;
                    args.reserve(5 + _chatServers.size());
                    args.push_back(std::to_string(kLeaseTtlMs));
                    args.push_back(token);
                    args.push_back(std::to_string(user_id));
                    args.push_back(request_id);
                    args.push_back(std::to_string(_chatServers.size()));
                    for (const auto& server : _chatServers) {
                        args.push_back(server.name);
                    }

                    const auto result = redis->EvalStrings(
                            kSelectServerScript, {}, args);

                    if (result.size() < 3 || result[0] != "1") {
                        LogThrottled("StatusServer", "no chat server selected");
                        finish(ErrorCodes::RPCFailed);
                        return;
                    }

                    const auto it = _chatServerMap.find(result[1]);
                    if (it == _chatServerMap.end()) {
                        LogThrottled("StatusServer",
                                     "selected server not in local config: " +
                                             result[1]);
                        finish(ErrorCodes::RPCFailed);
                        return;
                    }

                    response->set_error(ErrorCodes::Success);
                    response->set_host(it->second.host);
                    response->set_port(it->second.port);
                    response->set_token(result[2]);
                    response->set_expires_in(kLeaseTtlSeconds);

                    finished = true;
                    reactor->Finish(grpc::Status::OK);
                } catch (const std::exception& e) {
                    LogThrottled("StatusServer::GetChatServer", e.what());
                    finish(ErrorCodes::RPCFailed);
                } catch (...) {
                    LogThrottled("StatusServer::GetChatServer", "unknown error");
                    finish(ErrorCodes::RPCFailed);
                }
            });

    return reactor;
}

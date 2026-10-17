//
// Created by mpt on 2026/10/15.
//

#include "http/LogicSystem.h"
#include "const.h"
#include "net/HttpConnection.h"
#include "redis/RedisMgr.h"
#include "rpc/VarifyGrpcClient.h"
#include "common/Validators.h"
#include "service/RegisterService.h"
#include "service/VarifyCodeService.h"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <atomic>
#include <iostream>
#include <utility>

#include "config/ConfigMgr.h"

namespace {

    boost::beast::http::status RegisterHttpStatus(int error) {
        switch (error) {
            case ErrorCodes::Success:
                return boost::beast::http::status::ok;
            case ErrorCodes::TooManyRequests:
                return boost::beast::http::status::too_many_requests;
            case ErrorCodes::UserExists:
                return boost::beast::http::status::conflict;
            case ErrorCodes::VarifyBackErr:
                return boost::beast::http::status::service_unavailable;
            case ErrorCodes::DBError:
                return boost::beast::http::status::internal_server_error;
            default:
                return boost::beast::http::status::bad_request;
        }
    }

    std::size_t RpcThreadCount() {
        static const std::size_t count = [] {
            int value = 0;
            try {
                value = std::stoi(ConfigMgr::Inst()["GetServer"]["RpcThreads"]);
            } catch (...) {
            }
            if (value <= 0) value = 16;
            if (value > 64) value = 64;
            return static_cast<std::size_t>(value);
        }();
        return count;
    }

    std::size_t RpcQueueLimit() {
        static const std::size_t limit = [] {
            int value = 0;
            try {
                value = std::stoi(ConfigMgr::Inst()["GetServer"]["RpcQueueLimit"]);
            } catch (...) {
            }
            return static_cast<std::size_t>(value > 0 ? value : 64);
        }();
        return limit;
    }

    boost::asio::thread_pool& RpcWorkerPool() {
        static boost::asio::thread_pool pool(RpcThreadCount());
        return pool;
    }

    std::atomic<std::size_t> g_rpcQueued{0};
    constexpr int kRpcQueueBudgetMs = 2500;

}

LogicSystem::LogicSystem() {
    initGetHandlers();
    initPostHandlers();
}

void LogicSystem::RegGet(std::string url,
                         std::function<void(std::shared_ptr<HttpConnection>)> handler) {
    _get_handlers.insert(std::make_pair(std::move(url), std::move(handler)));
}

void LogicSystem::RegPost(std::string url,
                          std::function<void(std::shared_ptr<HttpConnection>)> handler) {
    _post_handlers.insert(std::make_pair(std::move(url), std::move(handler)));
}

bool LogicSystem::HandleGet(const std::string& url, std::shared_ptr<HttpConnection> conn) {
    auto it = _get_handlers.find(url);
    if (it == _get_handlers.end()) {
        return false;
    }
    it->second(conn);
    return true;
}

bool LogicSystem::HandlePost(const std::string& url, std::shared_ptr<HttpConnection> conn) {
    auto it = _post_handlers.find(url);
    if (it == _post_handlers.end()) {
        return false;
    }
    it->second(conn);
    return true;
}

void LogicSystem::initGetHandlers() {
}

void LogicSystem::initPostHandlers() {
    RegPost("/get_varifycode", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        auto send_json = std::make_shared<json>();

        auto write = [connection, send_json](int err,boost::beast::http::status status =boost::beast::http::status::ok, const int retry_after = 0) {
            (*send_json)["error"] = err;
            connection->_response.result(status);
            if (retry_after > 0) {
                connection->_response.set(boost::beast::http::field::retry_after, std::to_string(retry_after));
            }
            boost::beast::ostream(connection->_response.body()) << send_json->dump();
            connection->WriteResponse();
        };

        auto writeFromWorker = [connection, write](int err, boost::beast::http::status status,
                                                   int retry_after) {
            boost::asio::post(connection->GetSocket().get_executor(),
                              [write, err, status, retry_after]() {
                                  write(err, status, retry_after);
                              });
        };

        try {
            auto req_json = json::parse(body_str);
            auto email = validators::NormalizeEmail(
                    req_json.value("email", std::string{}));
            if (!validators::IsValidEmail(email)) {
                write(ErrorCodes::Error_Email);
                return;
            }

            auto redis = RedisMgr::GetInstance();
            if (!redis->IsValid()) {
                std::cerr << "RedisMgr not connected" << std::endl;
                write(ErrorCodes::VarifyBackErr);
                return;
            }

            std::string ip;
            try {
                ip = connection->GetSocket().remote_endpoint().address().to_string();
            } catch (...) {
                write(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request);
                return;
            }

            switch (service::VarifyCodeService::AcquireSlot(*redis, email, ip)) {
                case service::VarifyCodeService::SlotResult::Allowed:
                    break;
                case service::VarifyCodeService::SlotResult::GlobalLimited:
                    write(ErrorCodes::TooManyRequests,
                          boost::beast::http::status::too_many_requests, 1);
                    return;
                case service::VarifyCodeService::SlotResult::MailCooldown:
                    write(ErrorCodes::TooManyRequests,
                          boost::beast::http::status::too_many_requests, kMailCooldown);
                    return;
                case service::VarifyCodeService::SlotResult::IpCooldown:
                    write(ErrorCodes::TooManyRequests,
                          boost::beast::http::status::too_many_requests, kIpCooldown);
                    return;
                case service::VarifyCodeService::SlotResult::QuotaExceeded:
                    write(ErrorCodes::TooManyRequests,
                          boost::beast::http::status::too_many_requests, 3600);
                    return;
                case service::VarifyCodeService::SlotResult::CacheHit:
                    write(ErrorCodes::Success);
                    return;
                default:
                    write(ErrorCodes::VarifyBackErr,
                          boost::beast::http::status::service_unavailable);
                    return;
            }

            if (g_rpcQueued.fetch_add(1, std::memory_order_relaxed) >= RpcQueueLimit()) {
                g_rpcQueued.fetch_sub(1, std::memory_order_relaxed);
                service::VarifyCodeService::ReleaseSlot(*redis, email, ip);
                write(ErrorCodes::TooManyRequests,
                      boost::beast::http::status::service_unavailable, 1);
                return;
            }

            const auto enqueued_at = std::chrono::steady_clock::now();
            connection->DeferResponse();
            boost::asio::post(RpcWorkerPool(),
                              [email, redis, writeFromWorker, ip, connection, enqueued_at]() {
                struct QueueGuard {
                    std::atomic<std::size_t>& count;
                    ~QueueGuard() { count.fetch_sub(1, std::memory_order_relaxed); }
                } guard{g_rpcQueued};

                if (!connection->IsOpen()) {
                    service::VarifyCodeService::ReleaseSlot(*redis, email, ip);
                    return;
                }
                if (std::chrono::steady_clock::now() - enqueued_at >
                    std::chrono::milliseconds(kRpcQueueBudgetMs)) {
                    service::VarifyCodeService::ReleaseSlot(*redis, email, ip);
                    writeFromWorker(ErrorCodes::TooManyRequests,
                                    boost::beast::http::status::service_unavailable, 1);
                    return;
                }

                int error = ErrorCodes::VarifyBackErr;
                std::string code;
                try {
                    auto response = VarifyGrpcClient::GetInstance()->GetVarityCode(email);
                    error = response.error();
                    code = response.code();
                } catch (const std::exception& e) {
                    std::cerr << "[RPC] GetVarityCode exception: " << e.what() << std::endl;
                }

                if (error != ErrorCodes::Success) {
                    service::VarifyCodeService::ReleaseSlot(*redis, email, ip);
                    writeFromWorker(error, boost::beast::http::status::ok, 0);
                    return;
                }
                if (!redis->SetEx(VarifyCodeKey + email, code, std::chrono::seconds(300))) {
                    service::VarifyCodeService::ReleaseSlot(*redis, email, ip);
                    writeFromWorker(ErrorCodes::VarifyBackErr,
                                    boost::beast::http::status::ok, 0);
                    return;
                }
                writeFromWorker(ErrorCodes::Success, boost::beast::http::status::ok, 0);
            });
        } catch (const std::exception&) {
            write(ErrorCodes::Error_Json);
        }
    });

    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        auto send_json = std::make_shared<json>();

        auto send = [connection, send_json](int err, boost::beast::http::status status,
                                            const std::string& message, int retry_after,
                                            long long user_id) {
            (*send_json)["error"] = err;
            if (!message.empty()) {
                (*send_json)["msg"] = message;
            }
            if (user_id > 0) {
                (*send_json)["user_id"] = user_id;
            }
            connection->_response.result(status);
            connection->_response.set(boost::beast::http::field::content_type, "application/json");
            if (retry_after > 0) {
                connection->_response.set(boost::beast::http::field::retry_after,
                                          std::to_string(retry_after));
            }
            boost::beast::ostream(connection->_response.body()) << send_json->dump();
            connection->WriteResponse();
        };

        auto sendFromWorker = [connection, send](int err, boost::beast::http::status status,
                                                 const std::string& message, int retry_after,
                                                 long long user_id) {
            boost::asio::post(connection->GetSocket().get_executor(),
                              [send, err, status, message, retry_after, user_id]() {
                                  send(err, status, message, retry_after, user_id);
                              });
        };

        try {
            auto req_json = json::parse(body_str);
            if (!req_json.is_object()) {
                send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request,
                     "请求体必须是 JSON 对象", 0, 0);
                return;
            }

            service::RegisterRequest request;
            request.account = req_json.value("account", std::string{});
            request.password = req_json.value("password", std::string{});
            request.username = req_json.value("username", std::string{});
            request.email = req_json.value("email", std::string{});
            request.phone = req_json.value("phone", std::string{});
            request.code = req_json.value("code", std::string{});

            std::string client_ip;
            try {
                client_ip = connection->GetSocket().remote_endpoint().address().to_string();
            } catch (...) {
                send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request,
                     "无法获取客户端 IP", 0, 0);
                return;
            }

            if (g_rpcQueued.fetch_add(1, std::memory_order_relaxed) >= RpcQueueLimit()) {
                g_rpcQueued.fetch_sub(1, std::memory_order_relaxed);
                send(ErrorCodes::TooManyRequests,
                     boost::beast::http::status::service_unavailable,
                     "服务器繁忙，请稍后重试", 1, 0);
                return;
            }

            const auto enqueued_at = std::chrono::steady_clock::now();
            connection->DeferResponse();
            boost::asio::post(
                    RpcWorkerPool(),
                    [connection, sendFromWorker, request = std::move(request),
                     client_ip = std::move(client_ip), enqueued_at]() {
                        struct QueueGuard {
                            std::atomic<std::size_t>& count;
                            ~QueueGuard() { count.fetch_sub(1, std::memory_order_relaxed); }
                        } guard{g_rpcQueued};

                        try {
                            if (!connection->IsOpen()) {
                                return;
                            }
                            if (std::chrono::steady_clock::now() - enqueued_at >
                                std::chrono::milliseconds(kRpcQueueBudgetMs)) {
                                sendFromWorker(ErrorCodes::TooManyRequests,
                                               boost::beast::http::status::service_unavailable,
                                               "服务器繁忙，请稍后重试", 1, 0);
                                return;
                            }

                            const auto result = service::RegisterService::Register(
                                    request, client_ip);
                            sendFromWorker(result.error, RegisterHttpStatus(result.error),
                                           result.message, result.retry_after, result.user_id);
                        } catch (const std::exception& e) {
                            std::cerr << "[register] worker exception: " << e.what() << std::endl;
                            sendFromWorker(ErrorCodes::DBError,
                                           boost::beast::http::status::internal_server_error,
                                           "注册失败，请稍后重试", 0, 0);
                        }
                    });
        } catch (const json::exception&) {
            send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request,
                 "请求字段类型错误或 JSON 非法", 0, 0);
        } catch (const std::exception& e) {
            std::cerr << "[register] exception: " << e.what() << std::endl;
            send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error,
                 "注册失败，请稍后重试", 0, 0);
        }
    });
}

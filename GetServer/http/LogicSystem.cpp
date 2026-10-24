//
// Created by mpt on 2026/10/15.
//

#include "http/LogicSystem.h"

#include "common/Validators.h"
#include "const.h"
#include "db/MySqlMgr.h"
#include "http/AdmissionPool.h"
#include "http/PostToPool.h"
#include "net/HttpConnection.h"
#include "redis/RedisMgr.h"
#include "rpc/VarifyGrpcClient.h"
#include "service/LogicService.h"
#include "service/RegisterService.h"
#include "service/ResetPwdService.h"
#include "service/VarifyCodeService.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

using http::PostToPool;

namespace {
struct ReplyState {
    bool replied = false;
};

boost::beast::http::status HttpStatusForError(int error) {
    switch (error) {
        case ErrorCodes::Success: return boost::beast::http::status::ok;
        case ErrorCodes::TooManyRequests: return boost::beast::http::status::too_many_requests;
        case ErrorCodes::UserExists: return boost::beast::http::status::conflict;
        case ErrorCodes::RPCFailed: return boost::beast::http::status::bad_gateway;
        case ErrorCodes::VarifyBackErr: return boost::beast::http::status::service_unavailable;
        case ErrorCodes::DBError:
        case ErrorCodes::MySqlError: return boost::beast::http::status::internal_server_error;
        default: return boost::beast::http::status::bad_request;
    }
}

std::string ClientIp(const std::shared_ptr<HttpConnection>& connection) {
    try {
        return connection->GetSocket().remote_endpoint().address().to_string();
    } catch (...) {
        return {};
    }
}
}

LogicSystem::LogicSystem() {
    initGetHandlers();
    initPostHandlers();
}

void LogicSystem::RegGet(std::string url, std::function<void(std::shared_ptr<HttpConnection>)> handler) {
    _get_handlers.insert(std::make_pair(std::move(url), std::move(handler)));
}

void LogicSystem::RegPost(std::string url, std::function<void(std::shared_ptr<HttpConnection>)> handler) {
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

void LogicSystem::initGetHandlers() {}

void LogicSystem::initPostHandlers() {
    RegPost("/get_varifycode", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int error, boost::beast::http::status status, int retry_after) {
            boost::asio::post(
                    connection->GetSocket().get_executor(), [connection, state, error, status, retry_after]() {
                        if (state->replied || !connection->IsOpen()) {
                            return;
                        }
                        state->replied = true;

                        json response;
                        response["error"] = error;
                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        if (retry_after > 0) {
                            connection->_response.set(
                                    boost::beast::http::field::retry_after, std::to_string(retry_after)
                            );
                        }
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();

        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, 0);
            return;
        }

        auto redis = RedisMgr::GetInstance();
        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1); },
                [send]() { send(ErrorCodes::VarifyBackErr, boost::beast::http::status::service_unavailable, 0); }, {},
                [body = std::move(body), client_ip, redis, send, connection]() mutable {
                    json request;
                    try {
                        request = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }
                    if (!request.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }

                    std::string email;
                    try {
                        email = validators::NormalizeEmail(request.value("email", std::string{}));
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }
                    if (!validators::IsValidEmail(email)) {
                        send(ErrorCodes::Error_Email, boost::beast::http::status::bad_request, 0);
                        return;
                    }
                    if (!redis->IsValid()) {
                        send(ErrorCodes::VarifyBackErr, boost::beast::http::status::service_unavailable, 0);
                        return;
                    }

                    switch (service::VarifyCodeService::AcquireSlot(*redis, email, client_ip)) {
                        case service::VarifyCodeService::SlotResult::Allowed: break;
                        case service::VarifyCodeService::SlotResult::GlobalLimited:
                            send(ErrorCodes::TooManyRequests, boost::beast::http::status::too_many_requests, 1);
                            return;
                        case service::VarifyCodeService::SlotResult::MailCooldown:
                            send(ErrorCodes::TooManyRequests, boost::beast::http::status::too_many_requests,
                                 kMailCooldown);
                            return;
                        case service::VarifyCodeService::SlotResult::IpCooldown:
                            send(ErrorCodes::TooManyRequests, boost::beast::http::status::too_many_requests,
                                 kIpCooldown);
                            return;
                        case service::VarifyCodeService::SlotResult::QuotaExceeded:
                            send(ErrorCodes::TooManyRequests, boost::beast::http::status::too_many_requests, 3600);
                            return;
                        case service::VarifyCodeService::SlotResult::CacheHit:
                            send(ErrorCodes::Success, boost::beast::http::status::ok, 0);
                            return;
                        default:
                            send(ErrorCodes::VarifyBackErr, boost::beast::http::status::service_unavailable, 0);
                            return;
                    }

                    auto release_slot = [redis, email, client_ip]() {
                        service::VarifyCodeService::ReleaseSlot(*redis, email, client_ip);
                    };

                    PostToPool(
                            rpc::RpcWorkerPool(), rpc::g_rpcQueued, rpc::RpcQueueLimit(), rpc::kRpcQueueBudgetMs,
                            connection,
                            [release_slot, send]() {
                                release_slot();
                                send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1);
                            },
                            [release_slot, send]() {
                                release_slot();
                                send(ErrorCodes::VarifyBackErr, boost::beast::http::status::service_unavailable, 0);
                            },
                            [release_slot]() { release_slot(); },
                            [email, redis, send, release_slot]() {
                                int error = ErrorCodes::VarifyBackErr;
                                std::string code;
                                try {
                                    auto response = VarifyGrpcClient::GetInstance()->GetVarityCode(email);
                                    error = response.error();
                                    code = response.code();
                                } catch (const std::exception& e) {
                                    LogThrottled("GetVarityCode exception", e.what());
                                }

                                if (error != ErrorCodes::Success) {
                                    release_slot();
                                    send(error, HttpStatusForError(error), 0);
                                    return;
                                }
                                if (!redis->SetEx(VarifyCodeKey + email, code, std::chrono::seconds(300))) {
                                    release_slot();
                                    send(ErrorCodes::VarifyBackErr, boost::beast::http::status::service_unavailable, 0);
                                    return;
                                }
                                send(ErrorCodes::Success, boost::beast::http::status::ok, 0);
                            }
                    );
                }
        );
    });

    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection,
                     state](int error, boost::beast::http::status status, std::string message, int retry_after,
                            long long user_id) {
            boost::asio::post(
                    connection->GetSocket().get_executor(),
                    [connection, state, error, status, message = std::move(message), retry_after, user_id]() {
                        if (state->replied || !connection->IsOpen()) {
                            return;
                        }
                        state->replied = true;

                        json response;
                        response["error"] = error;
                        if (!message.empty()) {
                            response["msg"] = message;
                        }
                        if (user_id > 0) {
                            response["user_id"] = user_id;
                        }
                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        if (retry_after > 0) {
                            connection->_response.set(
                                    boost::beast::http::field::retry_after, std::to_string(retry_after)
                            );
                        }
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();

        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, {}, 0, 0);
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() {
                    send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, "server busy", 1,
                         0);
                },
                [send]() {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, "invalid json", 0, 0);
                },
                {},
                [body = std::move(body), client_ip, send, connection]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, "invalid json", 0, 0);
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request,
                             "request body must be a JSON object", 0, 0);
                        return;
                    }

                    service::RegisterRequest request;
                    try {
                        request.account = request_json.value("account", std::string{});
                        request.password = request_json.value("password", std::string{});
                        request.username = request_json.value("username", std::string{});
                        request.email = request_json.value("email", std::string{});
                        request.phone = request_json.value("phone", std::string{});
                        request.code = request_json.value("code", std::string{});
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, "invalid field type", 0,
                             0);
                        return;
                    }

                    service::RegisterRequest normalized;
                    const auto admitted = service::RegisterService::Admit(request, client_ip, normalized);
                    if (admitted.error != ErrorCodes::Success) {
                        send(admitted.error, HttpStatusForError(admitted.error), admitted.message, admitted.retry_after,
                             0);
                        return;
                    }

                    PostToPool(
                            mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(),
                            mysql::kMysqlQueueBudgetMs, connection,
                            [send]() {
                                send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable,
                                     "server busy", 1, 0);
                            },
                            [send]() {
                                send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error,
                                     "register failed", 0, 0);
                            },
                            {},
                            [normalized = std::move(normalized), send]() mutable {
                                const auto result = service::RegisterService::Register(normalized);
                                send(result.error, HttpStatusForError(result.error), result.message, result.retry_after,
                                     result.user_id);
                            }
                    );
                }
        );
    });

    RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int error, boost::beast::http::status status, int retry_after) {
            boost::asio::post(
                    connection->GetSocket().get_executor(), [connection, state, error, status, retry_after]() {
                        if (state->replied || !connection->IsOpen()) {
                            return;
                        }
                        state->replied = true;

                        json response;
                        response["error"] = error;
                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        if (retry_after > 0) {
                            connection->_response.set(
                                    boost::beast::http::field::retry_after, std::to_string(retry_after)
                            );
                        }
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();

        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, 0);
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1); },
                [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0); }, {},
                [body = std::move(body), client_ip, send, connection]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }

                    service::ResetPwdRequest request;
                    try {
                        request.email = request_json.value("email", std::string{});
                        request.code = request_json.value("code", std::string{});
                        request.new_password = request_json.value("password", std::string{});
                        request.confirm = request_json.value("confirm", std::string{});
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0);
                        return;
                    }

                    service::ResetPwdRequest normalized;
                    const auto admitted = service::ResetPwdService::Admit(request, client_ip, normalized);
                    if (admitted.ec != ErrorCodes::Success) {
                        send(admitted.ec, HttpStatusForError(admitted.ec), admitted.retry_after);
                        return;
                    }

                    PostToPool(
                            mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(),
                            mysql::kMysqlQueueBudgetMs, connection,
                            [send]() {
                                send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1);
                            },
                            [send]() {
                                send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, 0);
                            },
                            {},
                            [normalized = std::move(normalized), send]() mutable {
                                const auto result = service::ResetPwdService::ResetPwd(normalized);
                                send(result.ec, HttpStatusForError(result.ec), result.retry_after);
                            }
                    );
                }
        );
    });

    RegPost("/logic_user", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection,
                     state](int error, boost::beast::http::status status, int retry_after, unsigned long long user_id,
                            const std::string& host = "", const std::string& port = "", const std::string& token = "") {
            boost::asio::post(
                    connection->GetSocket().get_executor(),
                    [connection, state, error, status, retry_after, user_id, host, port, token]() {
                        if (state->replied || !connection->IsOpen()) {
                            return;
                        }
                        state->replied = true;

                        json response;
                        response["ec"] = error;
                        response["host"] = host;
                        response["port"] = port;
                        response["token"] = token;
                        response["user_id"] = user_id;
                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        if (retry_after > 0) {
                            connection->_response.set(
                                    boost::beast::http::field::retry_after, std::to_string(retry_after)
                            );
                        }
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();

        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, 0, 0);
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1, 0); },
                [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, 0); }, {},
                [body = std::move(body), client_ip, send, connection]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, 0);
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, 0);
                        return;
                    }

                    service::LogicRequest request;
                    try {
                        request.email = request_json.value("email", std::string{});
                        request.password = request_json.value("password", std::string{});
                        request.request_id = request_json.value("request_id", std::string{});
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, 0);
                        return;
                    }

                    service::LogicRequest normalized;
                    const auto admitted = service::LogicService::Admit(request, client_ip, normalized);
                    if (admitted.ec != ErrorCodes::Success) {
                        send(admitted.ec, HttpStatusForError(admitted.ec), admitted.retry_after, 0);
                        return;
                    }

                    PostToPool(
                            mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(),
                            mysql::kMysqlQueueBudgetMs, connection,
                            [send]() {
                                send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1,
                                     0);
                            },
                            [send]() {
                                send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, 0, 0);
                            },
                            {},
                            [normalized = std::move(normalized), send, connection]() mutable {
                                service::LogicService::Logic(
                                        normalized, connection, [send](const service::LogicResponse& response) {
                                            send(response.ec, HttpStatusForError(response.ec), response.retry_after,
                                                 response.user_id, response.host, response.port, response.token);
                                        }
                                );
                            }
                    );
                }
        );
    });
}

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
#include "storage/MinioClient.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "service/AvatarService.h"
#include "service/CheckStatusServer.h"
#include "service/UpLoadVideoServer.h"
#include "service/PlayVideoService.h"
using http::PostToPool;

namespace {
struct ReplyState {
    std::atomic<bool> replied{false};
};

// 抢“首答”权：并发的 send 里只有一个能真正写响应。
bool TryClaim(
        const std::shared_ptr<HttpConnection>& connection,
        const std::shared_ptr<ReplyState>& state
) {
    if (!connection->IsOpen()) {
        return false;
    }
    return !state->replied.exchange(true, std::memory_order_acq_rel);
}

constexpr int kCoverUrlTtlSec = 900;  // cover presigned GET lifetime
constexpr int kPlayUrlTtlSec = 900;    // hls master presigned GET lifetime
constexpr int kAvatarUrlTtlSec = 900; // avatar presigned GET lifetime

std::string ClientIp(const std::shared_ptr<HttpConnection>& connection) {
    try {
        return connection->GetSocket().remote_endpoint().address().to_string();
    } catch (...) {
        return {};
    }
}

void SendVideoResponse(
        const std::shared_ptr<HttpConnection>& connection,
        const std::shared_ptr<ReplyState>& state,
        int ec,
        boost::beast::http::status status,
        int retry_after,
        service::ResponseVideoData data
) {
    boost::asio::post(connection->GetSocket().get_executor(),
        [connection, state, data = std::move(data), status, retry_after, ec] {
            if (!TryClaim(connection, state)) {
                return;
            }
            json send_json;
            send_json["error"] = ec;
            send_json["msg"] = data.message;
            send_json["request_id"] = data.requestId;
            send_json["video_id"] = data.videoId;
            send_json["object_key"] = data.objectKey;
            send_json["upload_url"] = data.uploadUrl;
            send_json["expires_in"] = data.expiresIn;
            connection->SendJsonResponse(status, send_json.dump(), retry_after);
        }
    );
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
                        if (!TryClaim(connection, state)) {
                            return;
                        }

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
                                    send(error, service::HttpStatusForError(error), 0);
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
                        if (!TryClaim(connection, state)) {
                            return;
                        }

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
                        send(admitted.error, service::HttpStatusForError(admitted.error), admitted.message, admitted.retry_after,
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
                                send(result.error, service::HttpStatusForError(result.error), result.message, result.retry_after,
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
                        if (!TryClaim(connection, state)) {
                            return;
                        }

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
                        send(admitted.ec, service::HttpStatusForError(admitted.ec), admitted.retry_after);
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
                                send(result.ec, service::HttpStatusForError(result.ec), result.retry_after);
                            }
                    );
                }
        );
    });

    RegPost("/logic_user", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection,
                     state](int error, boost::beast::http::status status, int retry_after,
                            const service::LogicResponse& responsed) {
            boost::asio::post(
                    connection->GetSocket().get_executor(),
                    [connection, state, error, status, retry_after, responsed]() {
                        if (!TryClaim(connection, state)) {
                            return;
                        }

                        json response;
                        response["ec"] = error;
                        response["host"] = responsed.host;
                        response["port"] = responsed.port;
                        response["token"] = responsed.token;
                        response["user_id"] = responsed.user_id;
                        response["account"] = responsed.account;
                        response["email"] = responsed.email;
                        response["phone"] = responsed.phone;
                        response["username"] = responsed.username;
                        response["avatar"] = responsed.avatar;
                        response["avatar_url"] = responsed.avatar.empty()
                                ? std::string{}
                                : MinioClient::Instance().PresignGet(responsed.avatar, kAvatarUrlTtlSec);
                        response["follow_count"] = responsed.follow_count;
                        response["fans_count"] = responsed.fans_count;

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
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, 0, {});
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1, {}); },
                [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, {}); }, {},
                [body = std::move(body), client_ip, send, connection]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, {});
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, {});
                        return;
                    }

                    service::LogicRequest request;
                    try {
                        request.email = request_json.value("email", std::string{});
                        request.password = request_json.value("password", std::string{});
                        request.request_id = request_json.value("request_id", std::string{});
                        if (!request.request_id.empty() &&
                            (request.request_id.size() > 64 ||
                             !std::all_of(request.request_id.begin(), request.request_id.end(), [](unsigned char c) {
                                 return std::isalnum(c) || c == '-' || c == '_';
                             }))) {
                            send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, {});
                            return;
                        }
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, {});
                        return;
                    }

                    service::LogicRequest normalized;
                    const auto admitted = service::LogicService::Admit(request, client_ip, normalized);
                    if (admitted.ec != ErrorCodes::Success) {
                        send(admitted.ec, service::HttpStatusForError(admitted.ec), admitted.retry_after, {});
                        return;
                    }

                    PostToPool(
                            mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(),
                            mysql::kMysqlQueueBudgetMs, connection,
                            [send]() {
                                send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1,
                                     {});
                            },
                            [send]() {
                                send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, 0, {});
                            },
                            {},
                            [normalized = std::move(normalized), send, connection]() mutable {
                                service::LogicService::Logic(
                                        normalized, connection, [send](const service::LogicResponse& response) {
                                            send(response.ec, service::HttpStatusForError(response.ec), response.retry_after,response);
                                        }
                                );
                            }
                    );
                }
        );
    });

    RegPost("/avatar_upload", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int ec, boost::beast::http::status status,
                                        const service::AvatarTicketResponse& data) {
            boost::asio::post(
                    connection->GetSocket().get_executor(),
                    [connection, state, ec, status, data]() {
                        if (!TryClaim(connection, state)) {
                            return;
                        }

                        json response;
                        response["ec"] = ec;
                        response["object_key"] = data.objectKey;
                        response["upload_url"] = data.uploadUrl;
                        response["expires_in"] = data.expiresIn;
                        response["msg"] = data.message;

                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();
        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, {});
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, {}); },
                [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {}); }, {},
                [body = std::move(body), client_ip, send]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }

                    service::AvatarTicketRequest request;
                    try {
                        request.uid = request_json.value("uid", 0ULL);
                        request.token = request_json.value("token", std::string{});
                        request.requestId = request_json.value("request_id", std::string{});
                        request.fileSize = request_json.value("file_size", 0ULL);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }

                    const auto result = service::AvatarService::CreateTicket(request, client_ip);
                    send(result.ec, service::HttpStatusForError(result.ec), result);
                }
        );
    });

    RegPost("/avatar_complete", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(boost::beast::http::field::content_type, "application/json");

        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int ec, boost::beast::http::status status,
                                        const service::AvatarCompleteResponse& data) {
            boost::asio::post(
                    connection->GetSocket().get_executor(),
                    [connection, state, ec, status, data]() {
                        if (!TryClaim(connection, state)) {
                            return;
                        }

                        json response;
                        response["ec"] = ec;
                        response["avatar"] = data.avatarKey;
                        response["avatar_url"] = data.avatarUrl;
                        response["msg"] = data.message;

                        connection->_response.result(status);
                        connection->_response.set(boost::beast::http::field::content_type, "application/json");
                        boost::beast::ostream(connection->_response.body()) << response.dump();
                        connection->WriteResponse();
                    }
            );
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();
        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, {});
            return;
        }

        PostToPool(
                admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs,
                connection,
                [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, {}); },
                [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {}); }, {},
                [body = std::move(body), client_ip, send]() mutable {
                    json request_json;
                    try {
                        request_json = json::parse(body);
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }
                    if (!request_json.is_object()) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }

                    unsigned long long uid = 0;
                    std::string token;
                    std::string object_key;
                    try {
                        uid = request_json.value("uid", 0ULL);
                        token = request_json.value("token", std::string{});
                        object_key = request_json.value("object_key", std::string{});
                    } catch (const json::exception&) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }

                    const auto result = service::AvatarService::Complete(uid, token, object_key, client_ip);
                    send(result.ec, service::HttpStatusForError(result.ec), result);
                }
        );
    });
    RegPost("/video_upload", [](std::shared_ptr<HttpConnection> connection) {
        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int ec, boost::beast::http::status status, int retry_after, service::ResponseVideoData data) {
            SendVideoResponse(connection, state, ec, status, retry_after, std::move(data));
        };
        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        auto client_ip = ClientIp(connection);
        connection->DeferResponse();

        if (client_ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
            return;
        }
        PostToPool(admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs, connection,
            [send]() {send(ErrorCodes::TooManyRequests,boost::beast::http::status::service_unavailable, 1, service::ResponseVideoData());},
            [send](){send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());},{},
            [body = std::move(body), client_ip, send, connection]() {
                json request_json;
                try {
                    request_json = json::parse(body);
                }catch (const std::exception& e) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }
                if (!request_json.is_object()) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }
                auto request_id = request_json["request_id"].get<std::string>();
                if (!request_id.empty() && (request_id.size() > 64 || !std::all_of(request_id.begin(), request_id.end(), [](unsigned char c) {
                                 return std::isalnum(c) || c == '-' || c == '_';
                }))) {send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }
                service::RequestVideoData v_request{};
                try {
                    v_request.uid = request_json["uid"].get<unsigned long long>();
                    v_request.token = request_json.value("token", std::string{});
                    v_request.title = request_json["title"].get<std::string>();
                    v_request.description = request_json.value("description", std::string{});
                    v_request.fileName = request_json["file_name"].get<std::string>();
                    v_request.fileSize = request_json["file_size"].get<double>();
                    v_request.requestId = request_id;
                    for (const auto& tag : request_json["tags"]) {
                        v_request.tags.push_back(tag.get<std::string>());
                    }
                } catch (const std::exception& e) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }

                const auto admitted = service::UpLoadVideoServer::Admit(v_request, client_ip);
                if (admitted.ec != ErrorCodes::Success) {
                    auto fail_data = service::ResponseVideoData();
                    fail_data.requestId = v_request.requestId;
                    send(admitted.ec, service::HttpStatusForError(admitted.ec), 0, fail_data);
                    return;
                }
                PostToPool(mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(), mysql::kMysqlQueueBudgetMs, connection,
                    [send](){send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1, service::ResponseVideoData());},
                    [send](){send(ErrorCodes::StorageError, boost::beast::http::status::service_unavailable, 0, service::ResponseVideoData());}, {},
                    [v_request, send]() {
                        const auto session = service::CreateUploadSession(v_request);
                        if (!session) {
                            auto fail_data = service::ResponseVideoData();
                            fail_data.requestId = v_request.requestId;
                            fail_data.message = "failed to create upload session";
                            send(ErrorCodes::StorageError, boost::beast::http::status::service_unavailable, 0, fail_data);
                            return;
                        }

                        service::ResponseVideoData data{};
                        data.videoId = session->videoId;
                        data.objectKey = session->objectKey;
                        data.uploadUrl = session->uploadUrl;
                        data.requestId = session->requestId;
                        data.expiresIn = session->expiresIn;
                        send(ErrorCodes::Success, boost::beast::http::status::ok, 0, data);
                    });
            });
    });

    RegPost("/video_upload_complete", [](std::shared_ptr<HttpConnection> connection) {
        auto state = std::make_shared<ReplyState>();
        auto send = [connection, state](int ec, boost::beast::http::status status, int retry_after, service::ResponseVideoData data) {
            SendVideoResponse(connection, state, ec, status, retry_after, std::move(data));
        };
        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        connection->DeferResponse();

        PostToPool(admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs, connection,
            [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1, service::ResponseVideoData()); },
            [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData()); },
            {},
            [body = std::move(body), send, connection]() {
                json request_json;
                try {
                    request_json = json::parse(body);
                } catch (const std::exception&) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }
                if (!request_json.is_object()) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }

                service::CompleteUploadRequest request{};
                try {
                    request.uid = request_json.value("uid", 0ULL);
                    request.videoId = request_json.value("video_id", 0ULL);
                    request.requestId = request_json.value("request_id", std::string{});
                    request.objectKey = request_json.value("object_key", std::string{});
                } catch (const std::exception&) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }

                if (request.uid == 0 || request.objectKey.empty() ||
                    request.requestId.empty() || request.requestId.size() > 64 ||
                    !std::all_of(request.requestId.begin(), request.requestId.end(), [](unsigned char ch) {
                        return std::isalnum(ch) || ch == '-' || ch == '_';
                    })) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, 0, service::ResponseVideoData());
                    return;
                }

                PostToPool(mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(), mysql::kMysqlQueueBudgetMs, connection,
                    [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, 1, service::ResponseVideoData()); },
                    [send]() { send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, 0, service::ResponseVideoData()); },
                    {},
                    [request, send]() {
                        const auto result = service::CompleteUpload(request);
                        service::ResponseVideoData data{};
                        data.videoId = request.videoId;
                        data.requestId = request.requestId;
                        data.objectKey = request.objectKey;
                        data.message = result.message;
                        send(result.ec, service::HttpStatusForError(result.ec), 0, data);
                    });
            });
    });

    RegPost("/video_status", [](std::shared_ptr<HttpConnection> connection) {
        auto state = std::make_shared<ReplyState>();

        auto send = [connection, state](int error, boost::beast::http::status status, const service::ResponseData& datas) {
            if (!TryClaim(connection, state)) {
                return;
            }
            json send_json;
            send_json["error"] = error;
            if (datas.success) {
                send_json["user_id"] = datas.userId;
                for (const auto& data : datas.videos) {
                    json array;
                    array["video_id"] = data.videoId;
                    array["status"] = data.status;
                    array["cover_key"] = data.coverKey;
                    array["cover_url"] = data.coverUrl;
                    send_json["videos"].push_back(array);
                }
            }
            connection->SendJsonResponse(status, send_json.dump());
        };
        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        connection->DeferResponse();
        auto ip = ClientIp(connection);
        if (ip.empty()) {
            send(ErrorCodes::IPUnknown, boost::beast::http::status::bad_request, {});
            return;
        }

        PostToPool(admission::WorkerPool(), admission::queued, admission::QueueLimit(), admission::kQueueBudgetMs, connection,
        [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, {}); },
        [send]() { send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {}); }, {},
        [connection, send, body, ip]() {
            json req_json;
            try {
                req_json = json::parse(body);
            }catch (const std::exception&) {
                send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                return;
            }
            if (!req_json.is_object()) {
                send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                return;
            }
            try {
                auto user_id = req_json["user_id"].get<unsigned long long>();
                auto token = req_json["token"].get<std::string>();
                service::RequestData data{user_id, token, ip};
                std::vector<unsigned long long> videos{};
                const auto ids_it = req_json.find("video_ids");
                if (ids_it == req_json.end() || !ids_it->is_array()) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                    return;
                }
                for (const auto& item : *ids_it) {
                    if (!item.is_number_integer() || item.get<long long>() <= 0) {
                        send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                        return;
                    }
                    videos.push_back(item.get<unsigned long long>());
                }
                if (videos.empty() || videos.size() > kCheckVideoStatusMaxIds) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                    return;
                }

                if (user_id <= 0 || token.empty()) {
                    send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                    return;
                }
                auto result = service::CheckStatusServer::Admit(data);
                if (result != ErrorCodes::Success) {
                    send(result, service::HttpStatusForError(result), {});
                    return;
                }

                PostToPool(mysql::MysqlPool(), mysql::mysqlQueued, mysql::MysqlPoolQueueLimit(), mysql::kMysqlQueueBudgetMs, connection,
                    [send]() { send(ErrorCodes::TooManyRequests, boost::beast::http::status::service_unavailable, {}); },
                    [send]() { send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, {}); },{},
                    [user_id, videos, send]() {
                        const auto statuses = MySqlMgr::GetInstance()->GetVideoStatuses(user_id, videos);
                        if (!statuses) {
                            send(ErrorCodes::DBError, boost::beast::http::status::internal_server_error, {});
                            return;
                        }
                        service::ResponseData response{};
                        response.userId = user_id;
                        response.success = true;
                        response.videos.reserve(statuses->size());
                        for (const auto& item : *statuses) {
                            std::string cover_url;
                            if (!item.cover_key.empty()) {
                                cover_url = MinioClient::Instance().PresignGet(item.cover_key, kCoverUrlTtlSec);
                            }
                            response.videos.push_back({item.video_id, item.status, item.cover_key, std::move(cover_url)});
                        }
                        send(ErrorCodes::Success, boost::beast::http::status::ok, response);
                    });

            }catch (const std::exception&) {
                send(ErrorCodes::Error_Json, boost::beast::http::status::bad_request, {});
                return;
            }
        });

    });

    RegPost("/video_detail", [](std::shared_ptr<HttpConnection> connection) {
        auto state = std::make_shared<ReplyState>();

        auto status_for_error = [](int ec) -> boost::beast::http::status {
            switch (ec) {
                case ErrorCodes::Success:
                    return boost::beast::http::status::ok;
                case ErrorCodes::TooManyRequests:
                    return boost::beast::http::status::too_many_requests;
                case ErrorCodes::Error_Token:
                    return boost::beast::http::status::unauthorized;
                case ErrorCodes::VideoNotFound:
                    return boost::beast::http::status::not_found;
                case ErrorCodes::DBError:
                case ErrorCodes::MySqlError:
                    return boost::beast::http::status::internal_server_error;
                case ErrorCodes::StorageError:
                case ErrorCodes::VarifyBackErr:
                    return boost::beast::http::status::service_unavailable;
                default:
                    return boost::beast::http::status::bad_request;
            }
        };

        auto send = [connection, state, status_for_error](
                int error, const service::PlayVideoData* data) {
            if (!TryClaim(connection, state)) {
                return;
            }

            json send_json;
            send_json["error"] = error;
            if (error != ErrorCodes::Success || data == nullptr) {
                send_json["msg"] = service::PlayVideoService::ErrorMessage(error);
                connection->SendJsonResponse(status_for_error(error), send_json.dump());
                return;
            }

            send_json["video_id"] = data->videoId;
            send_json["title"] = data->title;
            send_json["description"] = data->description;
            send_json["cover_url"] = data->coverUrl;
            send_json["master_url"] = data->masterUrl;
            send_json["expires_in"] = data->expiresIn;
            send_json["like_count"] = data->likeCount;
            send_json["favorite_count"] = data->favoriteCount;
            send_json["play_count"] = data->playCount;
            send_json["comment_count"] = data->commentCount;
            connection->SendJsonResponse(status_for_error(error), send_json.dump());
        };

        auto body = boost::beast::buffers_to_string(connection->_request.body().data());
        connection->DeferResponse();

        const auto ip = ClientIp(connection);
        if (ip.empty()) {
            send(ErrorCodes::IPUnknown, nullptr);
            return;
        }

        PostToPool(admission::WorkerPool(), admission::queued, admission::QueueLimit(),
                   admission::kQueueBudgetMs, connection,
                [send]() { send(ErrorCodes::TooManyRequests, nullptr); },
                [send]() { send(ErrorCodes::Error_Json, nullptr); }, {},
                [body, ip, send, connection]() {
                    json req_json;
                    try {
                        req_json = json::parse(body);
                    } catch (const std::exception&) {
                        send(ErrorCodes::Error_Json, nullptr);
                        return;
                    }
                    if (!req_json.is_object()) {
                        send(ErrorCodes::Error_Json, nullptr);
                        return;
                    }

                    try {
                        const auto user_id = req_json.at("user_id").get<unsigned long long>();
                        const auto video_id = req_json.at("video_id").get<unsigned long long>();
                        const auto token = req_json.at("token").get<std::string>();
                        if (user_id == 0 || video_id == 0 || token.empty()) {
                            send(ErrorCodes::Error_Json, nullptr);
                            return;
                        }

                        service::PlayVideoRequest request{user_id, video_id, token};
                        const int admitted = service::PlayVideoService::Admit(request, ip);
                        if (admitted != ErrorCodes::Success) {
                            send(admitted, nullptr);
                            return;
                        }

                        PostToPool(mysql::MysqlPool(), mysql::mysqlQueued,
                                   mysql::MysqlPoolQueueLimit(), mysql::kMysqlQueueBudgetMs, connection,
                            [send]() { send(ErrorCodes::TooManyRequests, nullptr); },
                            [send]() { send(ErrorCodes::DBError, nullptr); }, {},
                            [video_id, send]() {
                                auto detail = MySqlMgr::GetInstance()->GetPlayVideoDetail(video_id);
                                if (!detail) {
                                    send(ErrorCodes::VideoNotFound, nullptr);
                                    return;
                                }

                                const std::string master_url = MinioClient::Instance().PresignGet(
                                        detail->master_key, kPlayUrlTtlSec);
                                if (master_url.empty()) {
                                    send(ErrorCodes::StorageError, nullptr);
                                    return;
                                }

                                service::PlayVideoData data{};
                                data.videoId = detail->video_id;
                                data.title = detail->title;
                                data.description = detail->description;
                                data.masterUrl = master_url;
                                data.expiresIn = kPlayUrlTtlSec;
                                data.likeCount = detail->like_count;
                                data.favoriteCount = detail->favorite_count;
                                data.playCount = detail->play_count;
                                data.commentCount = detail->comment_count;
                                if (!detail->cover_key.empty()) {
                                    data.coverUrl = MinioClient::Instance().PresignGet(
                                            detail->cover_key, kCoverUrlTtlSec);
                                }
                                send(ErrorCodes::Success, &data);
                            });
                    } catch (const std::exception&) {
                        send(ErrorCodes::Error_Json, nullptr);
                    }
                });
    });
}

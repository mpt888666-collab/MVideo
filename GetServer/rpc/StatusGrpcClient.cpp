//
// Created by mpt on 2026/10/19.
//

#include "StatusGrpcClient.h"

#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>

#include <stdexcept>
#include <utility>

#include "const.h"

StatusGrpcPool::StatusGrpcPool(const std::string& host, const std::string& port, std::size_t size)
    : _host(host),
      _port(port),
      _grpc_pool_size(size) {
    std::shared_ptr<grpc::Channel> channel =
            grpc::CreateChannel(_host + ":" + _port, grpc::InsecureChannelCredentials());
    std::size_t currentPoolSize = 0;
    for (std::size_t i = 0; i < _grpc_pool_size; ++i) {
        std::unique_ptr<message::StatusService::Stub> stub = message::StatusService::NewStub(channel);
        if (stub != nullptr) {
            ++currentPoolSize;
            _grpc_pools.push(std::move(stub));
        }
    }
    if (currentPoolSize == 0) {
        throw std::runtime_error("No statusServer grpc pools available");
    }
}

std::unique_ptr<message::StatusService::Stub> StatusGrpcPool::GetGrpcService(int timeout) {
    std::unique_lock<std::mutex> lock(_mtx);
    const bool ready = _cv.wait_for(lock, std::chrono::milliseconds(timeout), [this]() {
        return _b_stop || !_grpc_pools.empty();
    });

    if (_b_stop || !ready || _grpc_pools.empty()) {
        return nullptr;
    }

    std::unique_ptr<message::StatusService::Stub> stub = std::move(_grpc_pools.front());
    _grpc_pools.pop();
    lock.unlock();
    return stub;
}

void StatusGrpcPool::ReturnGrpcService(std::unique_ptr<message::StatusService::Stub> stub) {
    if (stub == nullptr) {
        return;
    }

    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop) {
        return;
    }

    _grpc_pools.push(std::move(stub));
    lock.unlock();
    _cv.notify_one();
}

StatusGrpcClient::StatusGrpcClient() {
    auto& cfg = ConfigMgr::Inst();
    auto host = cfg["StatusServer"]["Host"];
    auto port = cfg["StatusServer"]["Port"];
    std::size_t stub_pool_size = 0;
    try {
        const auto configured = ConfigMgr::Inst()["StatusServer"]["StubPoolSize"];
        if (!configured.empty()) {
            stub_pool_size = static_cast<std::size_t>(std::stoi(configured));
        }
    } catch (...) {
    }
    if (stub_pool_size == 0) {
        const unsigned int hardware = std::thread::hardware_concurrency();
        stub_pool_size = hardware > 0 ? hardware / 2 : 8;
        if (stub_pool_size < 4)
            stub_pool_size = 4;
        if (stub_pool_size > 16)
            stub_pool_size = 16;
    }
    if (stub_pool_size < 2)
        stub_pool_size = 2;
    if (stub_pool_size > 64)
        stub_pool_size = 64;

    _pool = std::make_unique<StatusGrpcPool>(host, port, stub_pool_size);
    if (_pool == nullptr) {
        throw std::runtime_error("StatusServer Pool create failed");
    }
}

message::GetChatServerRsp StatusGrpcClient::GetChatServer(unsigned long long user_id, std::string request_id) {
    message::GetChatServerReq req;
    message::GetChatServerRsp rsp;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(kRpcDeadlineMs));
    req.set_request_id(std::move(request_id));
    req.set_user_id(user_id);

    auto stub = _pool->GetGrpcService();
    if (stub == nullptr) {
        rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }

    const grpc::Status state = stub->GetChatServer(&context, req, &rsp);
    _pool->ReturnGrpcService(std::move(stub));

    if (!state.ok()) {
        rsp.set_error(ErrorCodes::RPCFailed);
        rsp.clear_host();
        rsp.clear_port();
        rsp.clear_token();
        rsp.set_expires_in(0);
        return rsp;
    }

    // The StatusServer returns business errors inside the response with a
    // successful gRPC status. Preserve that value; do not overwrite it with
    // Success, otherwise an empty route looks valid to the caller.
    return rsp;
}

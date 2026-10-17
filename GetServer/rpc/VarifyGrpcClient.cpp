//
// Created by mpt on 2026/10/15.
//

#include "rpc/VarifyGrpcClient.h"
#include <chrono>
#include <grpcpp/channel.h>
#include <grpcpp/grpcpp.h>
#include "const.h"
#include "config/ConfigMgr.h"
VarifyGrpcClientPool::VarifyGrpcClientPool(const std::string &host, const std::string& port, std::size_t size) :
_host(host), _port(port), _grpc_pool_size(size){
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(_host + ":" + _port,
                grpc::InsecureChannelCredentials());
    int currentPoolSize = 0;
    for (std::size_t i = 0; i < _grpc_pool_size; i++) {
        std::unique_ptr<message::VarifyService::Stub> stub = message::VarifyService::NewStub(channel);
        _client_pool.push(std::move(stub));
        currentPoolSize++;
    }
    std::cout << "Current VarifyGrpcClient Pool Size: " << currentPoolSize << std::endl;
}

std::unique_ptr<message::VarifyService::Stub> VarifyGrpcClientPool::GetGrpcService(int timeoutMs) {
    std::unique_lock<std::mutex> lock(_mtx);
    bool hasIdle = _cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),[this]() {
        return !_client_pool.empty() || _b_stop;
    });

    if (!hasIdle) {
        std::cout << "Waiting for a new client" << std::endl;
        return nullptr;
    }
    if (_b_stop) {
        std::cout << "Client is shutting down" << std::endl;
        return nullptr;
    }

    auto stub = std::move(_client_pool.front());
    _client_pool.pop();
    lock.unlock();
    return stub;
}

void VarifyGrpcClientPool::ReturnGrpcService(std::unique_ptr<message::VarifyService::Stub> stub) {
    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop) {
        return;
    }
    _client_pool.push(std::move(stub));
    lock.unlock();
    _cv.notify_one();
}

void VarifyGrpcClientPool::Stop() {
    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop) return;
    _b_stop = true;
    lock.unlock();
    _cv.notify_all();
}

VarifyGrpcClientPool::~VarifyGrpcClientPool() {
    Stop();
    while (!_client_pool.empty()) {
        _client_pool.pop();
    }
}

VarifyGrpcClient::VarifyGrpcClient() {
    auto host = ConfigMgr::Inst()["VarifyServer"]["Host"];
    auto port = ConfigMgr::Inst()["VarifyServer"]["Port"];
    _pool = std::make_unique<VarifyGrpcClientPool>(host, port);
    if (_pool == nullptr) {
        throw std::runtime_error("VarifyGrpcClient Pool create failed");
    }

}

message::GetVarifyRsp VarifyGrpcClient::GetVarityCode(const std::string &email) {
    message::GetVarifyRsp rsp;
    message::GetVarifyReq req;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(kRpcDeadlineMs));
    req.set_email(email);
    rsp.set_error(ErrorCodes::Success);
    auto stub = _pool->GetGrpcService();
    if (stub == nullptr) {
        rsp.set_error(ErrorCodes::VarifyBackErr);
        return rsp;
    }
    grpc::Status status = stub->GetVarifyCode(&context, req, &rsp);
    _pool->ReturnGrpcService(std::move(stub));
    if (!status.ok()) {
        std::cerr << "gRPC GetVarifyCode failed: code=" << static_cast<int>(status.error_code())
                  << " msg=" << status.error_message() << std::endl;
        rsp.set_code("");
        rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }
    return rsp;
}





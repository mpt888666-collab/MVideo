//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_VARIFYGRPCCLIENT_H
#define GETSERVER_VARIFYGRPCCLIENT_H
#include "message.grpc.pb.h"
#include "common/Singleton.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
class VarifyGrpcClientPool {
public:
    VarifyGrpcClientPool(const std::string& host, const std::string& port, std::size_t size = std::thread::hardware_concurrency());

    std::unique_ptr<message::VarifyService::Stub> GetGrpcService(int timeoutMs = 3000);
    void ReturnGrpcService(std::unique_ptr<message::VarifyService::Stub> stub);

    void Stop();

    ~VarifyGrpcClientPool();
private:
    std::queue<std::unique_ptr<message::VarifyService::Stub>> _client_pool;
    std::mutex _mtx;
    std::condition_variable _cv;
    std::string _host;
    std::string _port;
    std::size_t _grpc_pool_size;
    std::atomic<bool> _b_stop{false};
};
class VarifyGrpcClient : public Singleton<VarifyGrpcClient> {
    friend Singleton<VarifyGrpcClient>;
public:
    static constexpr int kRpcDeadlineMs = 3000;

    message::GetVarifyRsp GetVarityCode(const std::string& email);
private:
    VarifyGrpcClient();

    std::unique_ptr<VarifyGrpcClientPool> _pool;
};


#endif //GETSERVER_VARIFYGRPCCLIENT_H
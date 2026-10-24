//
// Created by mpt on 2026/10/19.
//

#ifndef GETSERVER_STATUSGRPCCLIENT_H
#define GETSERVER_STATUSGRPCCLIENT_H
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include "common/Singleton.h"
#include "message.grpc.pb.h"
#include <thread>
#include "common.h"

class StatusGrpcPool {
public:
    StatusGrpcPool(
            const std::string& host, const std::string& port, std::size_t size = std::thread::hardware_concurrency()
    );

    std::unique_ptr<message::StatusService::Stub> GetGrpcService(int timeout = kRpcDeadlineMs);

    void ReturnGrpcService(std::unique_ptr<message::StatusService::Stub> stub);

private:
    std::queue<std::unique_ptr<message::StatusService::Stub>> _grpc_pools;
    std::mutex _mtx;
    std::condition_variable _cv;
    std::string _host;
    std::string _port;
    std::size_t _grpc_pool_size;
    std::atomic<bool> _b_stop{false};
};

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;

public:
    message::GetChatServerRsp GetChatServer(unsigned long long, std::string);

private:
    StatusGrpcClient();

    std::unique_ptr<StatusGrpcPool> _pool;
};
#endif  // GETSERVER_STATUSGRPCCLIENT_H

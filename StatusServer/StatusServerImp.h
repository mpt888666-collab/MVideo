//
// Created by mpt on 2026/10/19.
//

#ifndef STATUSSERVER_STATUSSERVERIMP_H
#define STATUSSERVER_STATUSSERVERIMP_H

#include "message.grpc.pb.h"

#include <boost/asio/thread_pool.hpp>
#include <grpcpp/grpcpp.h>

#include <string>
#include <unordered_map>
#include <vector>

struct ChatServer {
    std::string host;
    std::string port;
    std::string name;
};

class StatusServerImp final : public message::StatusService::CallbackService {
public:
    explicit StatusServerImp(boost::asio::thread_pool& pool);

    grpc::ServerUnaryReactor* GetChatServer(
            grpc::CallbackServerContext* context,
            const message::GetChatServerReq* request,
            message::GetChatServerRsp* response) override;

private:
    boost::asio::thread_pool& _pool;
    std::vector<ChatServer> _chatServers;
    std::unordered_map<std::string, ChatServer> _chatServerMap;
};

#endif // STATUSSERVER_STATUSSERVERIMP_H

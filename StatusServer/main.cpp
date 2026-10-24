#include <iostream>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <boost/asio.hpp>
#include "StatusServerImp.h"
#include "ConfigMgr.h"
#include <memory>
int main() {
    auto& cfg = ConfigMgr::Inst();
    boost::asio::thread_pool pool(std::stoi(cfg["StatusServer"]["StubPoolSize"]));
    auto server_address = cfg["StatusServer"]["Host"] + ":" + cfg["StatusServer"]["GRPCPort"];
    StatusServerImp service(pool);
    grpc::ServerBuilder builder;
    int selected_port = 0;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &selected_port);
    builder.RegisterService(&service);

    builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);
    builder.SetMaxSendMessageSize(4 * 1024 * 1024);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server || selected_port == 0) {
        std::cerr << "listen failed" << std::endl;
        return 1;
    }

    std::cout << "listening on " << selected_port << std::endl;
    server->Wait();

    pool.stop();
    pool.join();
    return 0;
}
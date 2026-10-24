#include <boost/asio/io_context.hpp>

#include <exception>
#include <iostream>
#include <string>

#include "config/ConfigMgr.h"
#include "logic/LogicSystem.h"
#include "net/CServer.h"
#include "redis/ChatRedis.h"
#include "redis/RedisCleanup.h"
#include "redis/RedisMgr.h"

int main() {
    try {
        auto& cfg = ConfigMgr::Inst();

        const std::string port_text = cfg["SelfServer"]["Port"];
        if (port_text.empty()) {
            std::cerr << "SelfServer.Port is missing in config.ini" << std::endl;
            return 1;
        }
        const unsigned int port = static_cast<unsigned int>(std::stoi(port_text));

        auto redis = RedisMgr::GetInstance();
        if (!redis->IsValid()) {
            std::cerr << "Redis is not available" << std::endl;
            return 1;
        }
        redis_cleanup::ResetServerState();

        LogicSystem::GetInstance();

        boost::asio::io_context ioc;
        CServer server(ioc, port);
        server.Start();

        std::cout << "ChatServer " << chat_redis::ServerName() << " listening on " << port << std::endl;
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "ChatServer startup failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

#include <iostream>
#include <csignal>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include "net/Server.h"
#include "config/ConfigMgr.h"
#include "redis/RedisMgr.h"
#include "db/MySqlMgr.h"

int main() {
    try {
        if (!RedisMgr::GetInstance()->IsValid()) {
            std::cerr << "Redis连接失败！" << std::endl;
            return -1;
        }
        if (!MySqlMgr::GetInstance()->IsValid()) {
            std::cerr << "Mysql连接失败！" << std::endl;
            return -1;
        }

        boost::asio::io_context ioc{1};
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& ec, int) {
            if (ec) {
                return;
            }
            ioc.stop();
        });
        unsigned short port = std::stoi(ConfigMgr::Inst()["GetServer"]["Port"]);
        std::shared_ptr<Server> server = std::make_shared<Server>(ioc, port);
        server->Start();
        ioc.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}

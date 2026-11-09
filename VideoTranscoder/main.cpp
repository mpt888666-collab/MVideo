#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <csignal>
#include <exception>
#include <iostream>
#include <thread>

#include "EncodeMgr.h"
#include "EncodePoller.h"
#include "MySqlMgr.h"
#include "ThreadPools.h"

class TranscodeRuntime {
public:
    TranscodeRuntime() {
        MySqlMgr::GetInstance();
        EncodeWorkPool::GetInstance();
        EncodeMgr::GetInstance();
    }

    ~TranscodeRuntime() {
        EncodeMgr::GetInstance()->Stop();
        MySqlMgr::GetInstance()->Stop();
    }

    TranscodeRuntime(const TranscodeRuntime&) = delete;
    TranscodeRuntime& operator=(const TranscodeRuntime&) = delete;
};

int main() {
    try {
        TranscodeRuntime runtime;

        boost::asio::io_context ioc;
        EncodePoller poller(ioc);

        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& ec, int signo) {
            if (ec) {
                return;
            }
            std::cout << "[main] signal " << signo << ", shutting down" << std::endl;
            poller.Stop();
            ioc.stop();
        });

        poller.Run();

        std::thread io_thread([&ioc] { ioc.run(); });
        io_thread.join();
        ioc.stop();
    } catch (const std::exception& e) {
        std::cerr << "[main] fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_SERVER_H
#define GETSERVER_SERVER_H
#include <atomic>
#include <chrono>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
class HttpConnection;

class Server {
public:
    static constexpr std::size_t kMaxConnections = 4096;                // 最大连接数
    static constexpr std::chrono::milliseconds kAcceptRetryDelay{100};  // accept() 失败后等待多久再重试
    Server(boost::asio::io_context& ioc, const unsigned short& port);

    void Start();

private:
    void StartAccept();
    unsigned short _port;
    boost::asio::io_context& _ioc;
    boost::asio::ip::tcp::acceptor _acceptor;
    boost::asio::steady_timer _acceptRetry{_ioc};
    std::shared_ptr<std::atomic<std::size_t>> _liveConnections = std::make_shared<std::atomic<std::size_t>>(0);
    std::chrono::steady_clock::time_point _lastOverloadLog{};
};
#endif  // GETSERVER_SERVER_H

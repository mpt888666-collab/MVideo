//
// Created by mpt on 2026/10/15.
//

#include "net/Server.h"
#include "const.h"
#include "net/AsioIOServicePool.h"
#include "net/HttpConnection.h"

Server::Server(boost::asio::io_context& ioc, const unsigned short& port)
    : _port(port),
      _ioc(ioc),
      _acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    std::cout << "Server initialized on port: " << port << std::endl;
}

void Server::Start() {
    StartAccept();
}

void Server::StartAccept() {
    auto& io = AsioIOServicePool::GetInstance()->GetIOService();
    auto new_con = std::make_shared<HttpConnection>(io);
    _acceptor.async_accept(new_con->GetSocket(), [this, new_con](boost::beast::error_code ec) {
        try {
            if (ec) {
                LogThrottled("Accept error", ec.message());
                _acceptRetry.expires_after(kAcceptRetryDelay);
                _acceptRetry.async_wait([this](const boost::system::error_code& timer_ec) {
                    if (timer_ec == boost::asio::error::operation_aborted) {
                        return;
                    }
                    StartAccept();
                });
                return;
            }

            if (_liveConnections->load(std::memory_order_relaxed) >= kMaxConnections) {
                const auto now = std::chrono::steady_clock::now();
                if (now - _lastOverloadLog >= std::chrono::seconds(5)) {
                    _lastOverloadLog = now;
                    std::cerr << "Connection limit reached (" << kMaxConnections << "), refusing new connections"
                              << std::endl;
                }
                new_con->SendOverloadResponse();
                StartAccept();
                return;
            }

            _liveConnections->fetch_add(1, std::memory_order_relaxed);
            auto live = _liveConnections;
            new_con->SetOnClose([live]() { live->fetch_sub(1, std::memory_order_relaxed); });

            new_con->Start();
            StartAccept();
        } catch (const std::exception& e) {
            LogThrottled("Exception in accept handler", e.what());
            _acceptRetry.expires_after(kAcceptRetryDelay);
            _acceptRetry.async_wait([this](const boost::system::error_code& timer_ec) {
                if (timer_ec == boost::asio::error::operation_aborted) {
                    return;
                }
                StartAccept();
            });
        }
    });
}

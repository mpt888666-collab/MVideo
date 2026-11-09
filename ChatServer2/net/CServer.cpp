//
// Created by mpt on 2026/10/20.
//

#include "net/CServer.h"

#include <iostream>
#include <memory>

#include "net/AsioIOServicePool.h"
#include "net/CSession.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned int port)
    : _ioc(ioc),
      _acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4(), port)) {}

void CServer::Start() {
    auto& ioc = AsioIOServicePool::GetInstance()->GetIOService();
    auto new_session = std::make_shared<CSession>(ioc, this);
    _acceptor.async_accept(new_session->GetSocket(), [this, new_session](boost::system::error_code ec) {
        if (ec) {
            std::cout << "Error while accepting connection: " << ec << std::endl;
            Start();
            return;
        }
        std::unique_lock<std::mutex> lock(_sessions_mutex);
        _sessions.insert(std::make_pair(new_session->GetSessionId(), new_session));

        lock.unlock();
        new_session->Start();
        Start();
    });
}

void CServer::ClearSession(std::string id) {
    std::unique_lock<std::mutex> lock(_sessions_mutex);
    if (_sessions.find(id) != _sessions.end()) {
        _sessions.erase(id);
    }
}

//
// Created by mpt on 2026/10/20.
//

#ifndef CHATSERVER1_CSERVER_H
#define CHATSERVER1_CSERVER_H

#include <boost/asio.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class CSession;

class CServer {
public:
    explicit CServer(boost::asio::io_context& ioc, unsigned int port);

    void Start();

    void ClearSession(std::string);

private:
    boost::asio::io_context& _ioc;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::unordered_map<std::string, std::shared_ptr<CSession>> _sessions;
    std::mutex _sessions_mutex;
};

#endif  // CHATSERVER1_CSERVER_H

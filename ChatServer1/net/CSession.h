//
// Created by mpt on 2026/10/20.
//

#ifndef CHATSERVER1_CSESSION_H
#define CHATSERVER1_CSESSION_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "const.h"

class CServer;
class RecvNode;
class MsgNode;
class SendNode;

class CSession : public std::enable_shared_from_this<CSession> {
public:
    CSession(boost::asio::io_context& ioc, CServer* server);

    boost::asio::ip::tcp::socket& GetSocket();

    std::string GetSessionId();

    void Start();

    void AsyncReadHead(int head_total_len);

    void AsyncReadBody(int max_len);

    void Send(const std::string& msg, std::uint16_t msg_id);

    // Returns false when the session was closed before the login completed.
    bool SetLoginState(unsigned long long user_id, std::string token);

    // Returns a non-zero attempt id when this session may start a login.
    std::uint64_t TryBeginLogin();
    void EndLogin(std::uint64_t attempt);

    bool IsLoggedIn();

    bool IsOpen() const;

private:
    boost::asio::ip::tcp::socket _socket;

    std::string _session_id;

    CServer* _server;

    std::shared_ptr<MsgNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;
    std::queue<std::shared_ptr<SendNode>> _send_queue;
    char _data[MAX_LENGTH];
    unsigned long long _user_id;
    std::string _token;
    std::atomic<bool> _b_stop{false};
    bool _sending{false};
    bool _logged_in{false};
    bool _login_in_flight{false};
    std::uint64_t _login_attempt{0};
    std::mutex _mtx;

    void asyncReadFull(
            int total_len, const std::function<void(boost::system::error_code, std::size_t)>& callback
    );

    void asyncReadLen(
            int cur_len, int total_len, const std::function<void(boost::system::error_code, std::size_t)>& callback
    );

    void AsyncWrite();

    void Close();
};

#endif  // CHATSERVER1_CSESSION_H

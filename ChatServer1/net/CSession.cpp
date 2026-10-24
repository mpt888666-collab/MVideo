//
// Created by mpt on 2026/10/20.
//

#include "net/CSession.h"

#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/uuid.hpp>

#include <cstring>
#include <functional>
#include <iostream>
#include <utility>

#include "logic/LogicNode.h"
#include "logic/LogicSystem.h"
#include "net/CServer.h"
#include "net/MsgNode.h"
#include "redis/RedisCleanup.h"

namespace {

std::string MakeSessionId() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

}  // namespace

CSession::CSession(boost::asio::io_context& ioc, CServer* server)
    : _socket(ioc),
      _session_id(MakeSessionId()),
      _server(server),
      _user_id(0) {
    _recv_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

std::string CSession::GetSessionId() {
    return _session_id;
}

bool CSession::IsOpen() const {
    return !_b_stop.load(std::memory_order_acquire);
}

void CSession::AsyncReadHead(int head_total_len) {
    auto self = shared_from_this();
    asyncReadFull(head_total_len, [self, this](boost::system::error_code ec, std::size_t bytes_transfered) {
        if (ec) {
            if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset) {
                std::cerr << "asyncReadFull error: " << ec << std::endl;
            }
            Close();
            return;
        }

        if (bytes_transfered < HEAD_TOTAL_LEN) {
            std::cout << "read length not match, read [" << bytes_transfered << "] , total [" << HEAD_TOTAL_LEN << "]"
                      << std::endl;
            Close();
            return;
        }

        _recv_head_node->Clear();
        std::memcpy(_recv_head_node->_data, _data, bytes_transfered);

        uint16_t id = 0;
        uint16_t len = 0;
        std::memcpy(&id, _recv_head_node->_data, HEAD_ID_LEN);
        std::memcpy(&len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
        id = ntohs(id);
        len = ntohs(len);

        if (id == 0) {
            std::cout << "invalid msg_id is " << id << std::endl;
            Close();
            return;
        }
        if (len > MAX_LENGTH) {
            std::cout << "invalid data length is " << len << std::endl;
            Close();
            return;
        }

        _recv_msg_node = std::make_shared<RecvNode>(id, len);
        AsyncReadBody(len);
    });
}

void CSession::AsyncReadBody(int len) {
    auto self = shared_from_this();
    asyncReadFull(len, [self, this](boost::system::error_code ec, std::size_t bytes_transfered) {
        if (ec) {
            Close();
            return;
        }

        if (bytes_transfered < _recv_msg_node->_total_len) {
            Close();
            return;
        }

        std::memcpy(_recv_msg_node->_data, _data, bytes_transfered);
        _recv_msg_node->_cur_len = static_cast<uint16_t>(bytes_transfered);

        auto logic_node = std::make_shared<LogicNode>(self, _recv_msg_node);
        if (!LogicSystem::GetInstance()->PostLogicQueue(std::move(logic_node))) {
            std::cerr << "logic queue is full, close session" << std::endl;
            Close();
            return;
        }

        AsyncReadHead(HEAD_TOTAL_LEN);
    });
}

void CSession::Start() {
    AsyncReadHead(HEAD_TOTAL_LEN);
}

boost::asio::ip::tcp::socket& CSession::GetSocket() {
    return _socket;
}

void CSession::Send(const std::string& msg, uint16_t msg_id) {
    if (msg.size() > MAX_LENGTH) {
        std::cerr << "send message too large: " << msg.size() << std::endl;
        return;
    }

    auto self = shared_from_this();
    auto node = std::make_shared<SendNode>(msg.data(), msg_id, static_cast<uint16_t>(msg.size()));

    boost::asio::post(_socket.get_executor(), [self, node]() {
        bool start_write = false;
        {
            std::lock_guard<std::mutex> lock(self->_mtx);
            if (self->_b_stop) {
                return;
            }
            self->_send_queue.push(node);
            if (!self->_sending) {
                self->_sending = true;
                start_write = true;
            }
        }
        if (start_write) {
            self->AsyncWrite();
        }
    });
}

void CSession::AsyncWrite() {
    auto self = shared_from_this();
    std::shared_ptr<SendNode> node;
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_b_stop || _send_queue.empty()) {
            _sending = false;
            return;
        }
        node = _send_queue.front();
    }

    boost::asio::async_write(
            _socket, boost::asio::buffer(node->_data, node->_total_len),
            [self, node](boost::system::error_code ec, std::size_t) {
                bool start_next = false;
                {
                    std::lock_guard<std::mutex> lock(self->_mtx);
                    if (!self->_send_queue.empty()) {
                        self->_send_queue.pop();
                    }
                    if (ec || self->_b_stop) {
                        self->_sending = false;
                    } else if (!self->_send_queue.empty()) {
                        start_next = true;
                    } else {
                        self->_sending = false;
                    }
                }
                if (ec) {
                    self->Close();
                    return;
                }
                if (start_next) {
                    self->AsyncWrite();
                }
            }
    );
}

bool CSession::SetLoginState(unsigned long long user_id, std::string token) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_b_stop.load(std::memory_order_acquire) || _logged_in) {
        return false;
    }
    _logged_in = true;
    _user_id = user_id;
    _token = std::move(token);
    return true;
}

bool CSession::IsLoggedIn() {
    std::lock_guard<std::mutex> lock(_mtx);
    return _logged_in;
}

std::uint64_t CSession::TryBeginLogin() {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_b_stop.load(std::memory_order_acquire) || _logged_in || _login_in_flight) {
        return 0;
    }

    ++_login_attempt;
    if (_login_attempt == 0) {
        ++_login_attempt;
    }
    _login_in_flight = true;
    return _login_attempt;
}

void CSession::EndLogin(std::uint64_t attempt) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_login_attempt == attempt) {
        _login_in_flight = false;
    }
}

void CSession::asyncReadFull(
        int total_len, const std::function<void(boost::system::error_code, std::size_t)>& callback
) {
    if (total_len <= 0) {
        callback(boost::system::error_code{}, 0);
        return;
    }
    std::memset(_data, 0, MAX_LENGTH);
    asyncReadLen(0, total_len, callback);
}

void CSession::asyncReadLen(
        int cur_len, int total_len, const std::function<void(boost::system::error_code, std::size_t)>& callback
) {
    auto self = shared_from_this();
    _socket.async_read_some(
            boost::asio::buffer(_data + cur_len, total_len - cur_len),
            [total_len, cur_len, callback, this, self](boost::system::error_code ec, std::size_t bytes_transfered) {
                if (ec) {
                    if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset) {
                        std::cerr << "asyncReadFull error: " << ec << std::endl;
                    }
                    callback(ec, cur_len + bytes_transfered);
                    return;
                }

                if (bytes_transfered + cur_len < total_len) {
                    asyncReadLen(bytes_transfered + cur_len, total_len, callback);
                    return;
                }
                callback(ec, bytes_transfered + cur_len);
            }
    );
}

void CSession::Close() {
    bool was_logged_in = false;
    std::string token;
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_b_stop.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        was_logged_in = _logged_in;
        _logged_in = false;
        _login_in_flight = false;
        ++_login_attempt;
        token = std::move(_token);
        _token.clear();
        while (!_send_queue.empty()) {
            _send_queue.pop();
        }
        _sending = false;
    }

    boost::system::error_code ec;
    _socket.close(ec);

    if (was_logged_in && !token.empty() && !redis_cleanup::QueueRelease(std::move(token))) {
        std::cerr << "redis cleanup queue is full, token release dropped" << std::endl;
    }

    if (_server) {
        _server->ClearSession(_session_id);
    }
}

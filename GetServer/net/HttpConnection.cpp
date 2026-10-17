//
// Created by mpt on 2026/10/15.
//

#include "net/HttpConnection.h"
#include "const.h"
#include "http/LogicSystem.h"
HttpConnection::HttpConnection(boost::asio::io_context& ioc) : _socket(ioc){

}

HttpConnection::~HttpConnection() {
    if (_onClose) {
        _onClose();
    }
}

void HttpConnection::SetOnClose(std::function<void()> onClose) {
    _onClose = std::move(onClose);
}

void HttpConnection::DeferResponse() {
    _deferred = true;
}

void HttpConnection::Start() {
    auto self = shared_from_this();

    _parser.emplace();
    _parser->header_limit(kHeaderLimit);
    _parser->body_limit(kBodyLimit);

    const bool idle = _requestCount > 0 && _buffer.size() == 0;
    CheckDeadline(_requestCount == 0 ? kFirstRequestTimeout
                                 : (idle ? kIdleTimeout : kTimeout));

    boost::beast::http::async_read(_socket, _buffer, *_parser, [self](const boost::system::error_code &ec, std::size_t bytes_transfer) {
        try {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            if (ec) {
                if (ec == boost::beast::http::error::header_limit) {
                    self->SendErrorResponse(boost::beast::http::status::request_header_fields_too_large,
                                            "request header too large\r\n"); //431
                } else if (ec == boost::beast::http::error::body_limit) {
                    self->SendErrorResponse(boost::beast::http::status::payload_too_large, //413
                                            "request body too large\r\n");
                } else if (ec == boost::beast::http::error::buffer_overflow) {
                    self->SendErrorResponse(boost::beast::http::status::payload_too_large,
                                            "request too large\r\n");                 // 413
                } else if (ec == boost::beast::http::error::end_of_stream
                        || ec == boost::asio::error::eof
                        || ec == boost::asio::error::connection_reset) {
                    self->Close();
                } else {
                    LogThrottled("HTTP read error", ec.message());
                    self->Close();
                }
                return;
            }

            self->_request = self->_parser->release();
            self->_responded = false;
            self->CheckDeadline(kTimeout);

            boost::ignore_unused(bytes_transfer);
            self->handleReq();

        }catch (std::exception& e) {
            LogThrottled("HttpConnection::Start handler exception", e.what());
        }
    });
}


void HttpConnection::CheckDeadline(std::chrono::seconds timeout) {
    auto self = shared_from_this();
    _deadline.expires_after(timeout);
    _deadline.async_wait([self](boost::system::error_code ec) {
        try {
            if (!ec) {
                self->Close();
            }

        }catch (std::exception& e) {
            LogThrottled("CheckDeadline exception", e.what());
            self->Close();
        }
    });
}

void HttpConnection::handleReq() {
    ++_requestCount;

    _response = {};
    _deferred = false;

    _response.version(_request.version());
    _response.set(boost::beast::http::field::server, "GateServer");

    const bool keepAlive = _request.keep_alive() && _requestCount < kMaxRequestsPerConn;
    _response.keep_alive(keepAlive);

    if (_request.method() == boost::beast::http::verb::get) {
        bool success = LogicSystem::GetInstance()->HandleGet(_request.target(), shared_from_this());
        if (!success) {
            _response.result(boost::beast::http::status::not_found);
            _response.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(_response.body()) << "url not found\r\n";
            WriteResponse();
            return ;

        }
        if (!_deferred) {
            WriteResponse();
        }
        return;
    }

    if (_request.method() == boost::beast::http::verb::post) {
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
        if (!success) {
            _response.result(boost::beast::http::status::not_found);
            _response.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(_response.body()) << "url not found\r\n";
            WriteResponse();
            return;
        }
        if (!_deferred) {
            WriteResponse();
        }
        return;
    }

    _response.result(boost::beast::http::status::method_not_allowed);
    _response.set(boost::beast::http::field::content_type, "text/plain");
    boost::beast::ostream(_response.body()) << "method not allowed\r\n";
    WriteResponse();
}

void HttpConnection::WriteResponse() {
    if (_responded) {
        return;
    }
    _responded = true;
    auto self = shared_from_this();
    _response.prepare_payload();

    boost::beast::http::async_write(_socket, _response,[self](boost::system::error_code ec, std::size_t bytes_transfer){
        boost::ignore_unused(bytes_transfer);
        if (ec) {
            self->Close();
            return;
        }
        if (!self->_response.keep_alive()) {
           if (self->_drainOnClose) {
               self->BeginDrainClose();
           } else {
               self->Close();
           }
           return;
       }
        self->Start();
    });
}

void HttpConnection::SendOverloadResponse() {
    _response = {};
    _response.result(boost::beast::http::status::service_unavailable);
    _response.version(11);
    _response.keep_alive(false);
    _response.set(boost::beast::http::field::server, "GateServer");
    _response.set(boost::beast::http::field::content_type, "text/plain");
    boost::beast::ostream(_response.body()) << "server overloaded\r\n";
    WriteResponse();
}

boost::asio::ip::tcp::socket& HttpConnection::GetSocket() {
    return _socket;
}

void HttpConnection::SendErrorResponse(boost::beast::http::status status, std::string body) {
    _response = {};
    _response.result(status);
    _response.version(_request.version());
    _response.keep_alive(false);
    _response.set(boost::beast::http::field::content_type, "text/plain");
    _response.set(boost::beast::http::field::server, "GateServer");
    boost::beast::ostream(_response.body()) << body;
    _drainOnClose = true;

    WriteResponse();
}

void HttpConnection::Close() {
    if (_closed.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    _deadline.cancel();
    boost::system::error_code ignored;
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    _socket.close(ignored);
    if (_onClose) {
        auto onClose = std::move(_onClose);
        _onClose = nullptr;
        onClose();
    }
}

void HttpConnection::BeginDrainClose() {
    if (_closed.load(std::memory_order_acquire)) {
        return;
    }
    if (!_discard) {
        _discard = std::make_unique<char[]>(kDiscardChunk);
    }
    _drained = 0;
    boost::system::error_code ignored;
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);

    CheckDeadline(kCloseReadTimeout);
    DrainSome();
}

void HttpConnection::DrainSome() {
    if (_closed.load(std::memory_order_acquire)) {
        return;
    }
    auto self = shared_from_this();
    _socket.async_read_some(boost::asio::buffer(_discard.get(), kDiscardChunk),
        [self](const boost::system::error_code &ec, std::size_t n) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            if (ec) {
                self->Close();
                return;
            }
            self->_drained += n;
            if (self->_drained >= self->kCloseReadLimit) {
                self->Close();
                return;
            }
            self->DrainSome();
        });
}



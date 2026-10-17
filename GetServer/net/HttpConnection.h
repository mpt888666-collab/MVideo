//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_HTTPCONNECTION_H
#define GETSERVER_HTTPCONNECTION_H
#include <chrono>
#include <functional>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/optional.hpp>
class LogicSystem;
class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
    friend class LogicSystem;
public:
    static constexpr std::uint32_t kHeaderLimit = 8 * 1024;                   // 头部限制8KB
    static constexpr std::uint64_t kBodyLimit   = 1024ull * 1024;             // 身体限制1MB
    static constexpr std::size_t   kBufferLimit = kBodyLimit + 64 * 1024;     // 缓冲区限制1MB+64KB
    static constexpr std::chrono::seconds kTimeout{60};                   // 一次请求的总超时，包含读取请求和写入响应
    static constexpr std::chrono::seconds kFirstRequestTimeout{10};       // 首请求读取上限，防止慢速占槽
    static constexpr std::chrono::seconds kIdleTimeout{30};               // 长连接时间隔超过30秒没有请求就关闭连接
    static constexpr std::uint32_t kMaxRequestsPerConn = 1000;                // 每个连接最多处理1000个请求，防止客户端恶意占用连接
    static constexpr std::uint32_t kCloseReadLimit = 256 * 1024;              // socket关闭前读取剩余接收缓冲区无用数据大小，防止触发RST
    static constexpr std::chrono::seconds kCloseReadTimeout{2};           // socket关闭前读取剩余接收缓冲区无用数据的超时时间
    static constexpr std::size_t kDiscardChunk = 8 * 1024;                    // 每次读多少
public:
    explicit HttpConnection(boost::asio::io_context &ioc);

    ~HttpConnection();

    void Start();

     boost::asio::ip::tcp::socket& GetSocket();

    void SetOnClose(std::function<void()> onClose);

    void SendOverloadResponse();

    void DeferResponse();

    bool IsOpen() const { return !_closed.load(std::memory_order_acquire); }
private:
    using RequestParser = boost::beast::http::request_parser<boost::beast::http::dynamic_body>;

    void handleReq();
    void CheckDeadline(std::chrono::seconds timeout);
    void WriteResponse();

    void SendErrorResponse(boost::beast::http::status status, std::string body);

    void Close();

    void BeginDrainClose();

    void DrainSome();

    boost::asio::ip::tcp::socket _socket;
    boost::optional<RequestParser> _parser;
    boost::beast::flat_buffer _buffer{kBufferLimit};
    boost::beast::http::request<boost::beast::http::dynamic_body> _request;
    boost::beast::http::response<boost::beast::http::dynamic_body> _response;
    bool _responded = false;
    bool _deferred = false;
    std::uint32_t _requestCount = 0;
    std::function<void()> _onClose;
    std::atomic<bool> _closed{false};
    boost::asio::steady_timer _deadline{_socket.get_executor()};
    std::unique_ptr<char[]> _discard;
    bool _drainOnClose = false;
    std::size_t _drained = 0;
};


#endif //GETSERVER_HTTPCONNECTION_H
#include "MinioClient.h"

#include "ConfigMgr.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <boost/beast/http/basic_file_body.hpp>
#include <boost/none.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string HexEncode(const unsigned char* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result[i * 2] = kHex[(data[i] >> 4) & 0x0F];
        result[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return result;
}

std::string Sha256Hex(const std::string& value) {
    unsigned char digest[SHA256_DIGEST_LENGTH]{};
    unsigned int digest_length = 0;
    EVP_Digest(value.data(), value.size(), digest, &digest_length, EVP_sha256(), nullptr);
    return HexEncode(digest, digest_length);
}

std::string HmacSha256(const std::string& key, const std::string& value) {
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digest_length = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(value.data()), value.size(),
         digest, &digest_length);
    return std::string(reinterpret_cast<const char*>(digest), digest_length);
}

bool IsUnreserved(unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

std::string UriEncode(const std::string& value, bool encode_slash) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        if (IsUnreserved(ch) || (!encode_slash && ch == '/')) {
            result.push_back(static_cast<char>(ch));
            continue;
        }
        result.push_back('%');
        result.push_back(kHex[(ch >> 4) & 0x0F]);
        result.push_back(kHex[ch & 0x0F]);
    }
    return result;
}

std::string UtcTime(std::time_t now, const char* format) {
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char buffer[64]{};
    std::strftime(buffer, sizeof(buffer), format, &utc);
    return buffer;
}
}  // namespace

MinioClient& MinioClient::Instance() {
    static MinioClient client;
    return client;
}

MinioClient::MinioClient() {
    auto config = ConfigMgr::Inst()["MinIO"];
    _endpoint = config["Endpoint"];
    _bucket = config["Bucket"];
    _access_key = config["AccessKey"];
    _secret_key = config["SecretKey"];
    _region = config["Region"];
    if (_region.empty()) {
        _region = "us-east-1";
    }

    if (!ParseEndpoint()) {
        std::cerr << "[MinioClient] invalid MinIO endpoint: " << _endpoint << std::endl;
    }
    if (!IsConfigured()) {
        std::cerr << "[MinioClient] MinIO configuration is incomplete" << std::endl;
    }
}

bool MinioClient::IsConfigured() const {
    return !_scheme.empty() && !_host.empty() && !_bucket.empty() &&
           !_access_key.empty() && !_secret_key.empty();
}

bool MinioClient::ParseEndpoint() {
    const auto scheme_end = _endpoint.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }

    _scheme = ToLower(_endpoint.substr(0, scheme_end));
    if (_scheme != "http" && _scheme != "https") {
        return false;
    }

    const std::string rest = _endpoint.substr(scheme_end + 3);
    const auto path_start = rest.find('/');
    const std::string authority = rest.substr(0, path_start);
    _base_path = path_start == std::string::npos ? "" : rest.substr(path_start);
    if (authority.empty()) {
        return false;
    }

    if (authority.front() == '[') {
        const auto bracket = authority.find(']');
        if (bracket == std::string::npos) {
            return false;
        }
        _host = authority.substr(1, bracket - 1);
        if (bracket + 1 < authority.size() && authority[bracket + 1] == ':') {
            _port = authority.substr(bracket + 2);
        }
    } else {
        const auto colon = authority.rfind(':');
        if (colon != std::string::npos) {
            _host = authority.substr(0, colon);
            _port = authority.substr(colon + 1);
        } else {
            _host = authority;
        }
    }

    if (_host.empty()) {
        return false;
    }
    if (_port.empty()) {
        _port = _scheme == "https" ? "443" : "80";
    }
    return true;
}

std::string MinioClient::HostHeader() const {
    const bool default_port = (_scheme == "http" && _port == "80") ||
                              (_scheme == "https" && _port == "443");
    if (default_port) {
        return _host;
    }
    if (_host.find(':') != std::string::npos) {
        return "[" + _host + "]:" + _port;
    }
    return _host + ":" + _port;
}

std::string MinioClient::ObjectPath(const std::string& object_key) const {
    std::string base = _base_path;
    while (base.size() > 1 && base.back() == '/') {
        base.pop_back();
    }
    if (base.empty() || base.back() != '/') {
        base.push_back('/');
    }
    return base + UriEncode(_bucket, true) + "/" + object_key;
}

MinioClient::SignedRequest MinioClient::SignRequest(const std::string& method, const std::string& object_key, int expires_seconds)
const {
    if (!IsConfigured() || object_key.empty() || expires_seconds <= 0) {
        return {};
    }

    const std::time_t now = std::time(nullptr);
    const std::string amz_date = UtcTime(now, "%Y%m%dT%H%M%SZ");
    const std::string short_date = amz_date.substr(0, 8);  //只要年月日
    const std::string scope = short_date + "/" + _region + "/s3/aws4_request"; //SigV4 的 credential scope（凭据作用域）格式固定：{YYYYMMDD}/{region}/{service}/aws4_request  s3是s3协议
    const std::string canonical_uri = UriEncode(ObjectPath(object_key), false);
    const std::string canonical_query =
            "X-Amz-Algorithm=AWS4-HMAC-SHA256" +
            std::string("&X-Amz-Credential=") + UriEncode(_access_key + "/" + scope, true) +
            "&X-Amz-Date=" + UriEncode(amz_date, true) +
            "&X-Amz-Expires=" + std::to_string(expires_seconds) +
            "&X-Amz-SignedHeaders=host";
    const std::string canonical_headers = "host:" + HostHeader() + "\n";
    const std::string signed_headers = "host";
    const std::string payload_hash = "UNSIGNED-PAYLOAD";
    const std::string canonical_request =
            method + "\n" + canonical_uri + "\n" + canonical_query + "\n" +
            canonical_headers + "\n" + signed_headers + "\n" + payload_hash;
    const std::string string_to_sign =
            "AWS4-HMAC-SHA256\n" + amz_date + "\n" + scope + "\n" + Sha256Hex(canonical_request);

    const std::string k_date = HmacSha256("AWS4" + _secret_key, short_date);
    const std::string k_region = HmacSha256(k_date, _region);
    const std::string k_service = HmacSha256(k_region, "s3");
    const std::string k_signing = HmacSha256(k_service, "aws4_request");
    const std::string signature_bytes = HmacSha256(k_signing, string_to_sign);
    const std::string signature = HexEncode(
            reinterpret_cast<const unsigned char*>(signature_bytes.data()), signature_bytes.size()
    );

    const std::string final_query = canonical_query + "&X-Amz-Signature=" + signature;
    SignedRequest request;
    request.url = _scheme + "://" + HostHeader() + canonical_uri + "?" + final_query;
    request.target = canonical_uri + "?" + final_query;
    return request;
}

std::string MinioClient::PresignPut(const std::string& object_key, int expires_seconds) const {
    return SignRequest("PUT", object_key, expires_seconds).url;
}

std::optional<unsigned long long> MinioClient::HeadObjectSize(
        const std::string& object_key, int timeout_seconds
) const {
    const auto signed_request = SignRequest("HEAD", object_key, 60);
    if (signed_request.url.empty()) {
        return std::nullopt;
    }

    try {
        net::io_context io;
        tcp::resolver resolver(io);
        const auto endpoints = resolver.resolve(_host, _port);
        http::response<http::empty_body> response;
        beast::flat_buffer buffer;

        if (_scheme == "https") {
            ssl::context context(ssl::context::tls_client);
            context.set_default_verify_paths();
            beast::ssl_stream<beast::tcp_stream> stream(io, context);
            stream.set_verify_mode(ssl::verify_peer);
            stream.set_verify_callback(ssl::host_name_verification(_host));

            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(timeout_seconds));
            beast::get_lowest_layer(stream).connect(endpoints);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), _host.c_str())) {
                return std::nullopt;
            }
            stream.handshake(ssl::stream_base::client);

            http::request<http::empty_body> request{http::verb::head, signed_request.target, 11};
            request.set(http::field::host, HostHeader());
            request.set(http::field::user_agent, "MVideoGetServer/1.0");
            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(timeout_seconds));
            http::write(stream, request);
            http::response_parser<http::empty_body> parser;
            parser.skip(true);
            http::read(stream, buffer, parser);
            response = parser.release();

            boost::system::error_code ignored;
            stream.shutdown(ignored);
        } else {
            beast::tcp_stream stream(io);
            stream.expires_after(std::chrono::seconds(timeout_seconds));
            stream.connect(endpoints);

            http::request<http::empty_body> request{http::verb::head, signed_request.target, 11};
            request.set(http::field::host, HostHeader());
            request.set(http::field::user_agent, "MVideoGetServer/1.0");
            stream.expires_after(std::chrono::seconds(timeout_seconds));
            http::write(stream, request);
            http::response_parser<http::empty_body> parser;
            parser.skip(true);
            http::read(stream, buffer, parser);
            response = parser.release();

            boost::system::error_code ignored;
            stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
        }

        if (response.result() != http::status::ok) {
            return std::nullopt;
        }
        const auto content_length = response.find(http::field::content_length);
        if (content_length == response.end()) {
            return std::nullopt;
        }
        return std::stoull(std::string(content_length->value()));
    } catch (const std::exception& error) {
        std::cerr << "[MinioClient] HEAD object failed: " << error.what() << std::endl;
        return std::nullopt;
    }
}

namespace {

bool EnsureParentDirectory(const std::string& local_path) {
    std::error_code ec;
    const std::filesystem::path target(local_path);
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ec);
    }
    if (ec) {
        std::cerr << "[MinioClient] create directory failed: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

}  // namespace

std::string MinioClient::PresignGet(const std::string& object_key, int expires_seconds) const {
    return SignRequest("GET", object_key, expires_seconds).url;
}

bool MinioClient::Download(
        const std::string& object_key, const std::string& local_path, int timeout_seconds
) const {
    if (!IsConfigured() || object_key.empty() || local_path.empty() || timeout_seconds <= 0) {
        std::cerr << "[MinioClient] Download: invalid argument, key=" << object_key << std::endl;
        return false;
    }

    const SignedRequest signed_request = SignRequest("GET", object_key, 3600);
    if (signed_request.url.empty()) {
        std::cerr << "[MinioClient] Download: presign failed, key=" << object_key << std::endl;
        return false;
    }

    if (!EnsureParentDirectory(local_path)) {
        return false;
    }

    std::error_code fs_ec;
    const std::filesystem::path target(local_path);
    const std::string part_path = local_path + ".part";
    std::filesystem::remove(part_path, fs_ec);

    auto transfer = [&](auto& stream) -> bool {
        const auto deadline = std::chrono::seconds(timeout_seconds);
        beast::get_lowest_layer(stream).expires_after(deadline);

        http::request<http::empty_body> request{http::verb::get, signed_request.target, 11};
        request.set(http::field::host, HostHeader());
        request.set(http::field::user_agent, "MVideoTranscoder/1.0");
        http::write(stream, request);

        beast::flat_buffer buffer;
        http::response_parser<http::file_body> parser;
        parser.body_limit(boost::none);
        http::read_header(stream, buffer, parser);

        if (parser.get().result() != http::status::ok) {
            std::cerr << "[MinioClient] Download: GET " << object_key
                      << " returned " << parser.get().result_int() << std::endl;
            return false;
        }

        beast::error_code body_ec;
        parser.get().body().open(part_path.c_str(), beast::file_mode::write, body_ec);
        if (body_ec) {
            std::cerr << "[MinioClient] Download: open " << part_path << " failed: "
                      << body_ec.message() << std::endl;
            return false;
        }

        beast::get_lowest_layer(stream).expires_after(deadline);
        http::read(stream, buffer, parser);
        return true;
    };

    try {
        net::io_context io;
        tcp::resolver resolver(io);
        const auto endpoints = resolver.resolve(_host, _port);

        if (_scheme == "https") {
            ssl::context context(ssl::context::tls_client);
            context.set_default_verify_paths();
            beast::ssl_stream<beast::tcp_stream> stream(io, context);
            stream.set_verify_mode(ssl::verify_peer);
            stream.set_verify_callback(ssl::host_name_verification(_host));
            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(timeout_seconds));
            beast::get_lowest_layer(stream).connect(endpoints);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), _host.c_str())) {
                return false;
            }
            stream.handshake(ssl::stream_base::client);

            const bool ok = transfer(stream);
            boost::system::error_code ignored;
            stream.shutdown(ignored);
            if (!ok) {
                return false;
            }
        } else {
            beast::tcp_stream stream(io);
            stream.expires_after(std::chrono::seconds(timeout_seconds));
            stream.connect(endpoints);

            const bool ok = transfer(stream);
            boost::system::error_code ignored;
            stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
            if (!ok) {
                return false;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[MinioClient] Download failed: key=" << object_key
                  << ", error=" << error.what() << std::endl;
        std::filesystem::remove(part_path, fs_ec);
        return false;
    }

    std::filesystem::remove(target, fs_ec);
    std::filesystem::rename(part_path, target, fs_ec);
    if (fs_ec) {
        std::cerr << "[MinioClient] Download: rename to " << local_path
                  << " failed: " << fs_ec.message() << std::endl;
        std::filesystem::remove(part_path, fs_ec);
        return false;
    }

    std::cout << "[MinioClient] downloaded " << object_key << " -> " << local_path
              << " (" << std::filesystem::file_size(target, fs_ec) << " bytes)" << std::endl;
    return true;
}

bool MinioClient::Upload(
        const std::string& local_path, const std::string& object_key,
        const std::string& content_type, int timeout_seconds
) const {
    if (!IsConfigured() || local_path.empty() || object_key.empty() || timeout_seconds <= 0) {
        std::cerr << "[MinioClient] Upload: invalid argument, key=" << object_key << std::endl;
        return false;
    }

    std::error_code fs_ec;
    const std::uintmax_t local_size = std::filesystem::file_size(local_path, fs_ec);
    if (fs_ec) {
        std::cerr << "[MinioClient] Upload: cannot stat " << local_path << std::endl;
        return false;
    }

    const SignedRequest signed_request = SignRequest("PUT", object_key, 3600);
    if (signed_request.url.empty()) {
        std::cerr << "[MinioClient] Upload: presign failed, key=" << object_key << std::endl;
        return false;
    }

    http::file_body::value_type body;
    beast::error_code open_ec;
    body.open(local_path.c_str(), beast::file_mode::scan, open_ec);
    if (open_ec) {
        std::cerr << "[MinioClient] Upload: open " << local_path << " failed: "
                  << open_ec.message() << std::endl;
        return false;
    }

    auto transfer = [&](auto& stream) -> bool {
        const auto deadline = std::chrono::seconds(timeout_seconds);
        beast::get_lowest_layer(stream).expires_after(deadline);

        http::request<http::file_body> request{http::verb::put, signed_request.target, 11};
        request.set(http::field::host, HostHeader());
        request.set(http::field::user_agent, "MVideoTranscoder/1.0");
        if (!content_type.empty()) {
            request.set(http::field::content_type, content_type);
        }
        request.body() = std::move(body);
        request.prepare_payload();

        http::write(stream, request);

        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        beast::get_lowest_layer(stream).expires_after(deadline);
        http::read(stream, buffer, response);

        const auto status = response.result();
        if (status != http::status::ok && status != http::status::created &&
            status != http::status::no_content) {
            std::cerr << "[MinioClient] Upload: PUT " << object_key << " returned "
                      << response.result_int() << " " << response.body() << std::endl;
            return false;
        }
        return true;
    };

    try {
        net::io_context io;
        tcp::resolver resolver(io);
        const auto endpoints = resolver.resolve(_host, _port);

        if (_scheme == "https") {
            ssl::context context(ssl::context::tls_client);
            context.set_default_verify_paths();
            beast::ssl_stream<beast::tcp_stream> stream(io, context);
            stream.set_verify_mode(ssl::verify_peer);
            stream.set_verify_callback(ssl::host_name_verification(_host));

            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(timeout_seconds));
            beast::get_lowest_layer(stream).connect(endpoints);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), _host.c_str())) {
                return false;
            }
            stream.handshake(ssl::stream_base::client);

            const bool ok = transfer(stream);
            boost::system::error_code ignored;
            stream.shutdown(ignored);
            if (!ok) {
                return false;
            }
        } else {
            beast::tcp_stream stream(io);
            stream.expires_after(std::chrono::seconds(timeout_seconds));
            stream.connect(endpoints);

            const bool ok = transfer(stream);
            boost::system::error_code ignored;
            stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
            if (!ok) {
                return false;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[MinioClient] Upload failed: key=" << object_key
                  << ", error=" << error.what() << std::endl;
        return false;
    }

    std::cout << "[MinioClient] uploaded " << local_path << " -> " << object_key
              << " (" << local_size << " bytes)" << std::endl;
    return true;
}
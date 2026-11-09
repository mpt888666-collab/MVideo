//
// Created by mpt on 2026/11/6.
// VideoTranscoder 本地副本：不做上传预签名，直接用预签名 URL 收发对象字节。
// 线程安全：所有方法只读配置、每次调用各自建连接，可在多个 worker 线程并发使用。
//

#ifndef VIDEOTRANSCODER_MINIOCLIENT_H
#define VIDEOTRANSCODER_MINIOCLIENT_H

#include <optional>
#include <string>

class MinioClient {
public:
    static MinioClient& Instance();

    [[nodiscard]] bool IsConfigured() const;

    // 预签名 URL（给客户端直传用，本服务内部一般用不到）
    [[nodiscard]] std::string PresignPut(const std::string& object_key, int expires_seconds) const;
    [[nodiscard]] std::string PresignGet(const std::string& object_key, int expires_seconds) const;

    // 对象 -> 本地文件。先写 .part 再改名，失败不会留下半截文件。
    [[nodiscard]] bool Download(const std::string& object_key,
                  const std::string& local_path,
                  int timeout_seconds = 1800) const;

    // 本地文件 -> 对象。流式发送，不会把文件读进内存。
    [[nodiscard]] bool Upload(const std::string& local_path,
                const std::string& object_key,
                const std::string& content_type = "application/octet-stream",
                int timeout_seconds = 1800) const;

    std::optional<unsigned long long> HeadObjectSize(
            const std::string& object_key, int timeout_seconds = 5
    ) const;

private:
    struct SignedRequest {
        std::string url;
        std::string target;
    };

    MinioClient();

    bool ParseEndpoint();
    [[nodiscard]] std::string HostHeader() const;
    [[nodiscard]] std::string ObjectPath(const std::string& object_key) const;
    [[nodiscard]] SignedRequest SignRequest(
            const std::string& method, const std::string& object_key, int expires_seconds
    ) const;

    std::string _endpoint;
    std::string _scheme;
    std::string _host;
    std::string _port;
    std::string _base_path;
    std::string _bucket;
    std::string _access_key;
    std::string _secret_key;
    std::string _region;
};

#endif  // VIDEOTRANSCODER_MINIOCLIENT_H
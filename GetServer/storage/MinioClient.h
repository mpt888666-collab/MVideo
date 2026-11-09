#ifndef GETSERVER_MINIOClient_H
#define GETSERVER_MINIOClient_H

#include <optional>
#include <string>

class MinioClient {
public:
    static MinioClient& Instance();

    bool IsConfigured() const;

    std::string PresignPut(const std::string& object_key, int expires_seconds) const;

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
    std::string HostHeader() const;
    std::string ObjectPath(const std::string& object_key) const;
    SignedRequest SignRequest(
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

#endif  // GETSERVER_MINIOClient_H

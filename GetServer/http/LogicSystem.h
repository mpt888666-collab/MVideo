//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_LOGICSYSTEM_H
#define GETSERVER_LOGICSYSTEM_H
#include <functional>
#include <unordered_map>

#include "common/Singleton.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
class HttpConnection;
class LogicSystem : public Singleton<LogicSystem>{
    friend class Singleton<LogicSystem>;
public:
    bool HandleGet(const std::string &url, std::shared_ptr<HttpConnection> conn);

    bool HandlePost(const std::string &url, std::shared_ptr<HttpConnection> conn);
private:
    LogicSystem();

    void RegGet(std::string url, std::function<void(std::shared_ptr<HttpConnection>)> handler);

    void RegPost(std::string url, std::function<void(std::shared_ptr<HttpConnection>)> handler);

    void initGetHandlers();

    void initPostHandlers();

    std::unordered_map<std::string, std::function<void(std::shared_ptr<HttpConnection>)>> _get_handlers;
    std::unordered_map<std::string, std::function<void(std::shared_ptr<HttpConnection>)>> _post_handlers;
};


#endif //GETSERVER_LOGICSYSTEM_H
//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_REDISMGR_H
#define GETSERVER_REDISMGR_H

#include "Singleton.h"
#include "const.h"

#include <sw/redis++/redis++.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

class RedisMgr : public Singleton<RedisMgr> {
    friend class Singleton<RedisMgr>;
public:
    bool IsValid() const { return _redis != nullptr; }

    // Execute a Lua script returning a flat array of strings.
    std::vector<std::string> EvalStrings(
            const std::string& script,
            const std::vector<std::string>& keys,
            const std::vector<std::string>& args);

private:
    RedisMgr();

    std::unique_ptr<sw::redis::Redis> _redis;
};

#endif // GETSERVER_REDISMGR_H

//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_REDISMGR_H
#define GETSERVER_REDISMGR_H

#include <sw/redis++/redis++.h>
#include "common/Singleton.h"
#include "const.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

class RedisMgr : public Singleton<RedisMgr> {
    friend class Singleton<RedisMgr>;

public:
    bool IsValid() const {
        return _redis != nullptr;
    }


    std::vector<long long> EvalInts(
            const std::string& script, const std::vector<std::string>& keys, const std::vector<std::string>& args
    );

private:
    RedisMgr();

    std::unique_ptr<sw::redis::Redis> _redis;
};

#endif  // GETSERVER_REDISMGR_H

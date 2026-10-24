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
    // 写入 key-value，不带过期时间
    bool Set(const std::string& key, const std::string& value);

    // 写入 key-value 并设置过期时间，ttl <= 0 时等价于 Set
    bool SetEx(const std::string& key, const std::string& value, std::chrono::seconds ttl);

    // SET key value EX ttl NX 的原子占位：只有真抢到名额才算通过
    // 返回 LimiterAllowed=写入成功(放行) / LimiterDenied=key 已存在(冷却中) / LimiterError=Redis 异常
    LimiterResult SetNx(const std::string& key, const std::string& value, std::chrono::seconds ttl);

    // 读取 key，key 不存在或出错返回 false（此时 value 保持不变）
    bool Get(const std::string& key, std::string& value);

    // 构造函数里已经 ping 过，这里只表示是否连上 Redis
    bool IsValid() const {
        return _redis != nullptr;
    }

    // 原子自增 key 的值，并设置过期时间，返回自增后的值，失败返回 -1
    long long IncrWithTtl(const std::string& key, std::chrono::seconds ttl);

    long long EvalInt(
            const std::string& script, const std::vector<std::string>& keys, const std::vector<std::string>& args
    );

    // 删除 key，成功返回 true，失败或 key 不存在返回 false
    bool Del(const std::string& key);

private:
    RedisMgr();

    std::unique_ptr<sw::redis::Redis> _redis;
};
#endif  // GETSERVER_REDISMGR_H

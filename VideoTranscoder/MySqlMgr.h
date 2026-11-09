//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_MYSQLMGR_H
#define VIDEOTRANSCODER_MYSQLMGR_H

#include <mysql/mysql.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "Singleton.h"

struct EncodeResult;
struct SourceInfo;

struct MysqlConnDeleter {
    void operator()(MYSQL* conn) const {
        if (conn) {
            mysql_close(conn);
        }
    }
};

using MysqlConnUPtr = std::unique_ptr<MYSQL, MysqlConnDeleter>;

class MySqlMgr;

class MysqlConnLease {
public:
    explicit MysqlConnLease(MySqlMgr& mgr);
    ~MysqlConnLease();

    MysqlConnLease(const MysqlConnLease&) = delete;
    MysqlConnLease& operator=(const MysqlConnLease&) = delete;

    MYSQL* Raw() const { return _conn.get(); }
    explicit operator bool() const { return static_cast<bool>(_conn); }

private:
    MySqlMgr& _mgr;
    MysqlConnUPtr _conn;
};

class MySqlPool {
    constexpr static int kAcquireTimeoutMs = 3000;

public:
    MySqlPool(const std::string& host, unsigned int port, const std::string& user,
              const std::string& password, const std::string& db, std::size_t size);
    ~MySqlPool();

    MySqlPool(const MySqlPool&) = delete;
    MySqlPool& operator=(const MySqlPool&) = delete;

    MysqlConnUPtr GetConnection(int timeout_ms = kAcquireTimeoutMs);
    void ReturnConnection(MysqlConnUPtr conn);
    void Stop();

private:
    MysqlConnUPtr createdConn() const;

    std::queue<MysqlConnUPtr> _mysql_conns;
    std::mutex _mtx;
    std::condition_variable _cv;
    std::size_t _pool_size{0};
    std::string _host;
    unsigned int _port{3306};
    std::string _user;
    std::string _password;
    std::string _dbname;
    bool _b_stop{false};
};

class MySqlMgr : public Singleton<MySqlMgr> {
    friend class Singleton<MySqlMgr>;

public:
    // 待转码候选：只查不改，重复和过期都无害
    std::vector<unsigned long long> PickPendingVideoIds(std::size_t limit);

    // 原子认领，affected_rows == 1 才算拿到
    bool ClaimVideo(unsigned long long video_id);

    // 认领成功后一次取齐：源文件 key + 作者 uid
    std::optional<SourceInfo> GetSourceInfo(unsigned long long video_id);

    // 成功：单事务写 video_asset 并更新 video
    bool CommitTranscode(const EncodeResult& result);

    // 失败：未超过次数回到待办(2)，超过则置为失败(4)
    bool FailTranscode(unsigned long long video_id, const std::string& reason);

    bool IsValid() const { return _pool != nullptr; }
    void Stop();

    MysqlConnUPtr GetConn();
    void ReturnConn(MysqlConnUPtr conn);

private:
    MySqlMgr();

    std::unique_ptr<MySqlPool> _pool;
};

#endif  // VIDEOTRANSCODER_MYSQLMGR_H
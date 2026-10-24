//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_MYSQLMGR_H
#define GETSERVER_MYSQLMGR_H
#include "common/Singleton.h"
#include "config/ConfigMgr.h"

#include <mysql/mysql.h>

#include <boost/asio/thread_pool.hpp>
#include <boost/range/size.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

struct MysqlConnDeleter {
    void operator()(MYSQL* conn) const {
        if (conn) {
            mysql_close(conn);
        }
    }
};

using MysqlConnUPtr = std::unique_ptr<MYSQL, MysqlConnDeleter>;

class MySqlPool {
public:
    MySqlPool(
            const std::string& host, const unsigned int& port, const std::string& user, const std::string& pass,
            const std::string& db, size_t size = 2
    );

    MysqlConnUPtr GetConnection(int timeoutMs = 3000);

    void ReturnConnection(MysqlConnUPtr conn);

    void Stop() {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_b_stop)
            return;
        _b_stop = true;
        lock.unlock();
        _cv.notify_all();
    }

    ~MySqlPool() {
        Stop();
        ClearAllConn();
    }

private:
    MysqlConnUPtr createdConn() const;
    std::queue<MysqlConnUPtr> _mysql_conns;
    std::mutex _mtx;
    std::condition_variable _cv;
    size_t _pool_size;
    std::string _host;
    const unsigned int _port;
    std::string _user;
    std::string _password;
    std::string _dbname;
    std::atomic<bool> _b_stop = false;

    void ClearAllConn() {
        std::unique_lock<std::mutex> lock(_mtx);
        while (!_mysql_conns.empty()) {
            _mysql_conns.pop();
        }
    }
};

class MySqlMgr : public Singleton<MySqlMgr> {
    friend class Singleton<MySqlMgr>;

public:
    struct NewUser {
        std::string account;
        std::string username;
        std::string email;
        std::string phone;
        std::string password_hash;
    };

    enum class CreateUserResult { Success, Duplicate, Error };

    CreateUserResult CreateUser(const NewUser& user, long long& user_id);

    // -1: database error, 0: updated, 1: user not found
    int ResetPwd(const std::string& email, const std::string& password_hash);

    // -1: database error, 0: success, 1: user not found, 2: wrong password
    int UserLogin(const std::string& email, const std::string& password, unsigned long long& user_id);

    MysqlConnUPtr GetConn() const {
        if (!_pool)
            return nullptr;
        return _pool->GetConnection();
    }

    void ReturnConn(MysqlConnUPtr conn) {
        if (_pool)
            _pool->ReturnConnection(std::move(conn));
    }

    void StopPool() {
        if (_pool) {
            _pool->Stop();
            _pool.reset();
        }
    }

    bool IsValid() const {
        return _pool != nullptr;
    }

private:
    MySqlMgr();

    std::unique_ptr<MySqlPool> _pool;
};

struct MysqlConnGuard {
    explicit MysqlConnGuard(MysqlConnUPtr c)
        : _conn(std::move(c)) {}

    ~MysqlConnGuard() {
        if (_conn)
            MySqlMgr::GetInstance()->ReturnConn(std::move(_conn));
    }

    MysqlConnUPtr& Get() {
        return _conn;
    }

    MYSQL* Raw() {
        return _conn.get();
    }

    MysqlConnUPtr _conn;
};

namespace mysql {
inline static size_t MysqlThreadCount() {
    static const size_t count = [] {
        size_t size = 0;
        try {
            size = stoi(ConfigMgr::Inst()["MySQLServer"]["MySQL_Threads"]);
        } catch (std::exception& e) {
        }
        if (size == 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            size = hardware > 0 ? hardware / 2 : 8;
            if (size < 8)
                size = 8;
            if (size > 16)
                size = 16;
        }
        if (size > 32)
            size = 32;
        return size;
    }();

    return count;
}

inline static size_t MysqlPoolQueueLimit() {
    static const size_t limit = [] {
        size_t size = 0;
        try {
            size = stoi(ConfigMgr::Inst()["MySQLServer"]["MySQLQueueLimit"]);
        } catch (...) {
        }
        if (size < 32)
            size = 32;
        if (size > 2048)
            size = 2048;
        return static_cast<std::size_t>(size);
    }();
    return limit;
}

inline static boost::asio::thread_pool& MysqlPool() {
    static boost::asio::thread_pool pool(MysqlThreadCount());
    return pool;
}

inline std::atomic<std::size_t> mysqlQueued{0};
constexpr int kMysqlQueueBudgetMs = 2500;
}  // namespace mysql
#endif  // GETSERVER_MYSQLMGR_H

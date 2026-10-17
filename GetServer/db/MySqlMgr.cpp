//
// Created by mpt on 2026/10/15.
//

#include "db/MySqlMgr.h"
#include "config/ConfigMgr.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
namespace {

struct StmtDeleter {
    void operator()(MYSQL_STMT* stmt) const {
        if (stmt) {
            mysql_stmt_close(stmt);
        }
    }
};

using StmtPtr = std::unique_ptr<MYSQL_STMT, StmtDeleter>;

void LogMysqlError(const char* where, MYSQL* mysql) {
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_errno(mysql)
              << " " << mysql_error(mysql) << std::endl;
}

void LogStmtError(const char* where, MYSQL_STMT* stmt) {
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_stmt_errno(stmt)
              << " " << mysql_stmt_error(stmt) << std::endl;
}

StmtPtr Prepare(MYSQL* mysql, const char* sql) {
    MYSQL_STMT* raw = mysql_stmt_init(mysql);
    if (!raw) {
        LogMysqlError("mysql_stmt_init", mysql);
        return {};
    }
    if (mysql_stmt_prepare(raw, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        LogStmtError("mysql_stmt_prepare", raw);
        mysql_stmt_close(raw);
        return {};
    }
    return StmtPtr(raw);
}

void BindString(MYSQL_BIND& bind, const std::string& value, unsigned long& length) {
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(value.data());
    bind.buffer_length = static_cast<unsigned long>(value.size());
    bind.length = &length;
}

void BindNull(MYSQL_BIND& bind) {
    bind.buffer_type = MYSQL_TYPE_NULL;
}

bool Rollback(MYSQL* mysql) {
    return mysql_query(mysql, "ROLLBACK") == 0;
}

enum class LookupResult {
    NotFound,
    Found,
    Error
};

LookupResult FindExistingUser(MYSQL* mysql, const std::string& account,
                              const std::string& email, const std::string& phone) {
    static const char* kSql =
            "SELECT user_id FROM user WHERE account=? OR email=? "
            "OR (phone IS NOT NULL AND phone=?) LIMIT 1";
    auto stmt = Prepare(mysql, kSql);
    if (!stmt) {
        return LookupResult::Error;
    }

    std::string phone_value = phone;
    unsigned long account_length = static_cast<unsigned long>(account.size());
    unsigned long email_length = static_cast<unsigned long>(email.size());
    unsigned long phone_length = static_cast<unsigned long>(phone_value.size());
    MYSQL_BIND params[3]{};
    BindString(params[0], account, account_length);
    BindString(params[1], email, email_length);
    if (phone_value.empty()) {
        BindNull(params[2]);
    } else {
        BindString(params[2], phone_value, phone_length);
    }

    if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
        LogStmtError("mysql_stmt_bind_param(user lookup)", stmt.get());
        return LookupResult::Error;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(user lookup)", stmt.get());
        return LookupResult::Error;
    }

    unsigned long long existing_id = 0;
    bool existing_is_null = false;
    MYSQL_BIND result{};
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = &existing_id;
    result.is_null = &existing_is_null;
    result.is_unsigned = true;
    if (mysql_stmt_bind_result(stmt.get(), &result) != 0) {
        LogStmtError("mysql_stmt_bind_result(user lookup)", stmt.get());
        return LookupResult::Error;
    }
    if (mysql_stmt_store_result(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_store_result(user lookup)", stmt.get());
        return LookupResult::Error;
    }

    const int fetch_result = mysql_stmt_fetch(stmt.get());
    if (fetch_result == 0) {
        return LookupResult::Found;
    }
    if (fetch_result == MYSQL_NO_DATA) {
        return LookupResult::NotFound;
    }
    LogStmtError("mysql_stmt_fetch(user lookup)", stmt.get());
    return LookupResult::Error;
}

} // namespace
MySqlPool::MySqlPool(const std::string &host, const unsigned int &port,
    const std::string &user, const std::string &pass, const std::string &db, std::size_t size) : _host(host),
_port(port),_user(user),_password(pass),_dbname(db), _pool_size(size){
    int success_start_conn = 0;
    for (size_t i = 0; i < _pool_size; i++) {
        if (auto conn = createdConn()) {
            _mysql_conns.push(std::move(conn));
            success_start_conn++;
        }else {
            std::cerr << "Create is " << i << "sql failed!" << std::endl;
        }
    }
    size_t valid = _mysql_conns.size();
    if (valid == 0)  throw std::runtime_error("mysql pool create zero valid connections");
    else std::cout << "Pool init success!" << "number is " << success_start_conn << std::endl;

}

MysqlConnUPtr MySqlPool::createdConn() const {
    MYSQL *raw = mysql_init(nullptr);
    if (!raw) {
        std::cerr << "MySQL init error\n";
        return nullptr;
    }
    MysqlConnUPtr conn(raw, MysqlConnDeleter{});
    bool ret = mysql_real_connect(
       raw,
       _host.c_str(),
       _user.c_str(),
       _password.c_str(),
       _dbname.c_str(),
       _port,
       nullptr,
       0
   );
    if (!ret) {
        std::cerr << "set sql failed!" << mysql_error(raw) << std::endl;
        return nullptr;
    }

    mysql_set_character_set(raw, "utf8mb4");
    return conn;
}

MysqlConnUPtr MySqlPool::GetConnection(int timeoutMs) {
    std::unique_lock<std::mutex> lock(_mtx);
    bool hasIdle = _cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
        return !_mysql_conns.empty() || _b_stop;
    });

    if (_b_stop || !hasIdle) {
        return nullptr;
    }

    MysqlConnUPtr conn = std::move(_mysql_conns.front());
    _mysql_conns.pop();
    lock.unlock();

    if (mysql_ping(conn.get()) != 0) {
        std::cerr << "sql conn is down , plase reset connection" << std::endl;
        conn = createdConn();
        if (!conn) {
            std::cerr << "sql reset connection failed " << std::endl;
            return nullptr;
        }
    }
    return conn;
}
void MySqlPool::ReturnConnection(MysqlConnUPtr conn) {
    if (!conn) return;
    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop) return;

    mysql_reset_connection(conn.get());
    _mysql_conns.push(std::move(conn));
    _cv.notify_one();
}

MySqlMgr::MySqlMgr() {
    try {
        auto& cfgMgr = ConfigMgr::Inst();
        std::string host = cfgMgr["MySQLServer"]["Host"];
        std::string user = cfgMgr["MySQLServer"]["User"];
        std::string pwd = cfgMgr["MySQLServer"]["Password"];
        std::string db = cfgMgr["MySQLServer"]["Db"];
        unsigned int port = static_cast<unsigned int>(std::stoi(cfgMgr["MySQLServer"]["Port"]));
        size_t pool_size = static_cast<size_t>(std::stoi(cfgMgr["MySQLServer"]["Pool_size"]));
        _pool = std::make_unique<MySqlPool>(host, port, user, pwd, db, pool_size);
    } catch (std::exception& e) {
        std::cerr << "[MysqlMgr] init failed:" << e.what() << std::endl;
    }
}
MySqlMgr::CreateUserResult MySqlMgr::CreateUser(const NewUser& user, long long& user_id) {
    user_id = 0;
    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        std::cerr << "[MySqlMgr] MySQL is not available" << std::endl;
        return CreateUserResult::Error;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql) {
        std::cerr << "[MySqlMgr] no MySQL connection available" << std::endl;
        return CreateUserResult::Error;
    }

    if (mysql_query(mysql, "START TRANSACTION") != 0) {
        LogMysqlError("START TRANSACTION", mysql);
        return CreateUserResult::Error;
    }

    const LookupResult lookup = FindExistingUser(mysql, user.account, user.email, user.phone);
    if (lookup == LookupResult::Error) {
        Rollback(mysql);
        return CreateUserResult::Error;
    }
    if (lookup == LookupResult::Found) {
        Rollback(mysql);
        return CreateUserResult::Duplicate;
    }

    static const char* kInsertUserSql =
            "INSERT INTO user (account, phone, email, password_hash) VALUES (?, ?, ?, ?)";
    auto insert_user = Prepare(mysql, kInsertUserSql);
    if (!insert_user) {
        Rollback(mysql);
        return CreateUserResult::Error;
    }

    std::string account = user.account;
    std::string phone = user.phone;
    std::string email = user.email;
    std::string password_hash = user.password_hash;
    unsigned long account_length = static_cast<unsigned long>(account.size());
    unsigned long phone_length = static_cast<unsigned long>(phone.size());
    unsigned long email_length = static_cast<unsigned long>(email.size());
    unsigned long password_hash_length = static_cast<unsigned long>(password_hash.size());
    MYSQL_BIND user_params[4]{};
    BindString(user_params[0], account, account_length);
    if (phone.empty()) {
        BindNull(user_params[1]);
    } else {
        BindString(user_params[1], phone, phone_length);
    }
    BindString(user_params[2], email, email_length);
    BindString(user_params[3], password_hash, password_hash_length);

    if (mysql_stmt_bind_param(insert_user.get(), user_params) != 0) {
        LogStmtError("mysql_stmt_bind_param(insert user)", insert_user.get());
        Rollback(mysql);
        return CreateUserResult::Error;
    }
    if (mysql_stmt_execute(insert_user.get()) != 0) {
        const unsigned int error = mysql_stmt_errno(insert_user.get());
        LogStmtError("mysql_stmt_execute(insert user)", insert_user.get());
        Rollback(mysql);
        return error == 1062 ? CreateUserResult::Duplicate : CreateUserResult::Error;
    }

    user_id = static_cast<long long>(mysql_stmt_insert_id(insert_user.get()));
    insert_user.reset();
    if (user_id <= 0) {
        std::cerr << "[MySqlMgr] mysql_stmt_insert_id returned 0" << std::endl;
        Rollback(mysql);
        return CreateUserResult::Error;
    }

    static const char* kInsertInfoSql =
            "INSERT INTO user_info (user_id, username) VALUES (?, ?)";
    auto insert_info = Prepare(mysql, kInsertInfoSql);
    if (!insert_info) {
        Rollback(mysql);
        return CreateUserResult::Error;
    }

    unsigned long long uid = static_cast<unsigned long long>(user_id);
    std::string username = user.username;
    unsigned long username_length = static_cast<unsigned long>(username.size());
    MYSQL_BIND info_params[2]{};
    info_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    info_params[0].buffer = &uid;
    info_params[0].is_unsigned = true;
    BindString(info_params[1], username, username_length);

    if (mysql_stmt_bind_param(insert_info.get(), info_params) != 0) {
        LogStmtError("mysql_stmt_bind_param(insert user_info)", insert_info.get());
        Rollback(mysql);
        return CreateUserResult::Error;
    }
    if (mysql_stmt_execute(insert_info.get()) != 0) {
        LogStmtError("mysql_stmt_execute(insert user_info)", insert_info.get());
        Rollback(mysql);
        return CreateUserResult::Error;
    }

    if (mysql_query(mysql, "COMMIT") != 0) {
        LogMysqlError("COMMIT", mysql);
        Rollback(mysql);
        return CreateUserResult::Error;
    }
    return CreateUserResult::Success;
}

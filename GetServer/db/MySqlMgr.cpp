//
// Created by mpt on 2026/10/15.
//

#include "db/MySqlMgr.h"

#include "config/ConfigMgr.h"
#include "security/PasswordUtil.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

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
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_errno(mysql) << " " << mysql_error(mysql) << std::endl;
}

void LogStmtError(const char* where, MYSQL_STMT* stmt) {
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_stmt_errno(stmt) << " " << mysql_stmt_error(stmt)
              << std::endl;
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

std::optional<unsigned int> GetOrCreateTagId(MYSQL* mysql, const std::string& name) {
    static const char* kSql =
            "INSERT INTO tag (name) VALUES (?) ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id)";
    auto stmt = Prepare(mysql, kSql);
    if (!stmt) {
        return std::nullopt;
    }

    unsigned long name_length = static_cast<unsigned long>(name.size());
    MYSQL_BIND param{};
    BindString(param, name, name_length);
    if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
        LogStmtError("mysql_stmt_bind_param(tag upsert)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(tag upsert)", stmt.get());
        return std::nullopt;
    }

    const auto tag_id = mysql_stmt_insert_id(stmt.get());
    if (tag_id == 0) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(tag_id);
}

enum class LookupResult { NotFound, Found, Error };

LookupResult FindExistingUser(
        MYSQL* mysql, const std::string& account, const std::string& email, const std::string& phone
) {
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
    MYSQL_BIND result{};
    using MysqlNullFlag = std::remove_pointer_t<decltype(result.is_null)>;
    MysqlNullFlag existing_is_null = 0;
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
}  // namespace

MySqlPool::MySqlPool(
        const std::string& host, const unsigned int& port, const std::string& user, const std::string& pass,
        const std::string& db, std::size_t size
)
    : _host(host),
      _port(port),
      _user(user),
      _password(pass),
      _dbname(db),
      _pool_size(size) {
    int success_start_conn = 0;
    for (size_t i = 0; i < _pool_size; i++) {
        if (auto conn = createdConn()) {
            _mysql_conns.push(std::move(conn));
            success_start_conn++;
        } else {
            std::cerr << "Create is " << i << "sql failed!" << std::endl;
        }
    }
    size_t valid = _mysql_conns.size();
    if (valid == 0)
        throw std::runtime_error("mysql pool create zero valid connections");
    else
        std::cout << "Pool init success!" << "number is " << success_start_conn << std::endl;
}

MysqlConnUPtr MySqlPool::createdConn() const {
    MYSQL* raw = mysql_init(nullptr);
    if (!raw) {
        std::cerr << "MySQL init error\n";
        return nullptr;
    }
    MysqlConnUPtr conn(raw, MysqlConnDeleter{});
    bool ret = mysql_real_connect(
            raw, _host.c_str(), _user.c_str(), _password.c_str(), _dbname.c_str(), _port, nullptr, 0
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
    if (!conn)
        return;
    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop)
        return;

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
        size_t pool_size = 0;
        try {
            const auto configured = cfgMgr["MySQLServer"]["Pool_size"];
            if (!configured.empty()) {
                pool_size = static_cast<size_t>(std::stoi(configured));
            }
        } catch (...) {
        }
        if (pool_size == 0) {
            const unsigned int hardware = std::thread::hardware_concurrency();
            pool_size = hardware > 0 ? hardware / 2 : 8;
            if (pool_size < 8)
                pool_size = 8;
            if (pool_size > 16)
                pool_size = 16;
        }
        if (pool_size < 2)
            pool_size = 2;
        if (pool_size > 64)
            pool_size = 64;
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

    static const char* kInsertUserSql = "INSERT INTO user (account, phone, email, password_hash) VALUES (?, ?, ?, ?)";
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

    static const char* kInsertInfoSql = "INSERT INTO user_info (user_id, username) VALUES (?, ?)";
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

std::optional<unsigned long long> MySqlMgr::CreateUploadingVideo(const NewVideo& video) {
    if (video.author_id == 0 || video.request_id.empty() || video.object_key.empty() ||
        video.title.empty() || video.file_size == 0) {
        std::cerr << "[MySqlMgr] invalid uploading video data" << std::endl;
        return std::nullopt;
    }

    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        std::cerr << "[MySqlMgr] MySQL is not available" << std::endl;
        return std::nullopt;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql) {
        std::cerr << "[MySqlMgr] no MySQL connection available" << std::endl;
        return std::nullopt;
    }

    if (mysql_query(mysql, "START TRANSACTION") != 0) {
        LogMysqlError("START TRANSACTION", mysql);
        return std::nullopt;
    }

    static const char* kInsertVideoSql =
            "INSERT INTO video "
            "(author_id, request_id, title, description, category_id, cover_key, status, review_status, visibility) "
            "VALUES (?, ?, ?, ?, 0, '', 1, 0, 0) "
            "ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id)";
    auto insert_video = Prepare(mysql, kInsertVideoSql);
    if (!insert_video) {
        Rollback(mysql);
        return std::nullopt;
    }

    unsigned long long author_id = video.author_id;
    std::string request_id = video.request_id;
    std::string title = video.title;
    std::string description = video.description;
    unsigned long request_id_length = static_cast<unsigned long>(request_id.size());
    unsigned long title_length = static_cast<unsigned long>(title.size());
    unsigned long description_length = static_cast<unsigned long>(description.size());
    MYSQL_BIND video_params[4]{};
    video_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    video_params[0].buffer = &author_id;
    video_params[0].is_unsigned = true;
    BindString(video_params[1], request_id, request_id_length);
    BindString(video_params[2], title, title_length);
    BindString(video_params[3], description, description_length);

    if (mysql_stmt_bind_param(insert_video.get(), video_params) != 0) {
        LogStmtError("mysql_stmt_bind_param(insert uploading video)", insert_video.get());
        Rollback(mysql);
        return std::nullopt;
    }
    if (mysql_stmt_execute(insert_video.get()) != 0) {
        LogStmtError("mysql_stmt_execute(insert uploading video)", insert_video.get());
        Rollback(mysql);
        return std::nullopt;
    }

    unsigned long long video_id = mysql_stmt_insert_id(insert_video.get());
    insert_video.reset();
    if (video_id == 0) {
        std::cerr << "[MySqlMgr] mysql_stmt_insert_id(video) returned 0" << std::endl;
        Rollback(mysql);
        return std::nullopt;
    }

    static const char* kInsertAssetSql =
            "INSERT INTO video_asset (video_id, asset_type, quality, storage_key, file_size) "
            "VALUES (?, 'original', 'default', ?, ?) "
            "ON DUPLICATE KEY UPDATE storage_key=storage_key";
    auto insert_asset = Prepare(mysql, kInsertAssetSql);
    if (!insert_asset) {
        Rollback(mysql);
        return std::nullopt;
    }

    std::string object_key = video.object_key;
    unsigned long object_key_length = static_cast<unsigned long>(object_key.size());
    unsigned long long file_size = video.file_size;
    MYSQL_BIND asset_params[3]{};
    asset_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    asset_params[0].buffer = &video_id;
    asset_params[0].is_unsigned = true;
    BindString(asset_params[1], object_key, object_key_length);
    asset_params[2].buffer_type = MYSQL_TYPE_LONGLONG;
    asset_params[2].buffer = &file_size;
    asset_params[2].is_unsigned = true;

    if (mysql_stmt_bind_param(insert_asset.get(), asset_params) != 0) {
        LogStmtError("mysql_stmt_bind_param(insert uploading video asset)", insert_asset.get());
        Rollback(mysql);
        return std::nullopt;
    }
    if (mysql_stmt_execute(insert_asset.get()) != 0) {
        LogStmtError("mysql_stmt_execute(insert uploading video asset)", insert_asset.get());
        Rollback(mysql);
        return std::nullopt;
    }
    insert_asset.reset();

    static const char* kInsertStatSql =
            "INSERT INTO video_stat (video_id) VALUES (?) "
            "ON DUPLICATE KEY UPDATE video_id=video_id";
    auto insert_stat = Prepare(mysql, kInsertStatSql);
    if (!insert_stat) {
        Rollback(mysql);
        return std::nullopt;
    }

    MYSQL_BIND stat_param{};
    stat_param.buffer_type = MYSQL_TYPE_LONGLONG;
    stat_param.buffer = &video_id;
    stat_param.is_unsigned = true;
    if (mysql_stmt_bind_param(insert_stat.get(), &stat_param) != 0) {
        LogStmtError("mysql_stmt_bind_param(insert uploading video stat)", insert_stat.get());
        Rollback(mysql);
        return std::nullopt;
    }
    if (mysql_stmt_execute(insert_stat.get()) != 0) {
        LogStmtError("mysql_stmt_execute(insert uploading video stat)", insert_stat.get());
        Rollback(mysql);
        return std::nullopt;
    }
    insert_stat.reset();

    static const char* kInsertVideoTagSql = "INSERT IGNORE INTO video_tag (video_id, tag_id) VALUES (?, ?)";
    for (const auto& tag : video.tags) {
        if (tag.empty()) {
            continue;
        }

        const auto tag_id = GetOrCreateTagId(mysql, tag);
        if (!tag_id) {
            Rollback(mysql);
            return std::nullopt;
        }

        auto insert_video_tag = Prepare(mysql, kInsertVideoTagSql);
        if (!insert_video_tag) {
            Rollback(mysql);
            return std::nullopt;
        }

        unsigned long long video_tag_video_id = video_id;
        unsigned long long video_tag_tag_id = *tag_id;
        MYSQL_BIND tag_params[2]{};
        tag_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
        tag_params[0].buffer = &video_tag_video_id;
        tag_params[0].is_unsigned = true;
        tag_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
        tag_params[1].buffer = &video_tag_tag_id;
        tag_params[1].is_unsigned = true;

        if (mysql_stmt_bind_param(insert_video_tag.get(), tag_params) != 0) {
            LogStmtError("mysql_stmt_bind_param(insert uploading video tag)", insert_video_tag.get());
            Rollback(mysql);
            return std::nullopt;
        }
        if (mysql_stmt_execute(insert_video_tag.get()) != 0) {
            LogStmtError("mysql_stmt_execute(insert uploading video tag)", insert_video_tag.get());
            Rollback(mysql);
            return std::nullopt;
        }
    }

    if (mysql_query(mysql, "COMMIT") != 0) {
        LogMysqlError("COMMIT", mysql);
        Rollback(mysql);
        return std::nullopt;
    }
    return video_id;
}

std::optional<MySqlMgr::UploadingVideo> MySqlMgr::GetUploadingVideo(
        unsigned long long author_id, const std::string& request_id
) {
    if (author_id == 0 || request_id.empty()) {
        return std::nullopt;
    }

    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        return std::nullopt;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql) {
        return std::nullopt;
    }

    static const char* kSql =
            "SELECT v.id, v.status, a.storage_key, COALESCE(a.file_size, 0) "
            "FROM video v "
            "JOIN video_asset a ON a.video_id = v.id "
            "AND a.asset_type = 'original' AND a.quality = 'default' "
            "WHERE v.author_id = ? AND v.request_id = ? LIMIT 1";
    auto stmt = Prepare(mysql, kSql);
    if (!stmt) {
        return std::nullopt;
    }

    std::string request_id_value = request_id;
    unsigned long request_id_length = static_cast<unsigned long>(request_id_value.size());
    MYSQL_BIND params[2]{};
    params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    params[0].buffer = &author_id;
    params[0].is_unsigned = true;
    BindString(params[1], request_id_value, request_id_length);
    if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
        LogStmtError("mysql_stmt_bind_param(get uploading video)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(get uploading video)", stmt.get());
        return std::nullopt;
    }

    unsigned long long video_id = 0;
    unsigned char status = 0;
    char object_key[513]{};
    unsigned long object_key_length = 0;
    unsigned long long file_size = 0;
    MYSQL_BIND result[4]{};
    using MysqlNullFlag = std::remove_pointer_t<decltype(result[0].is_null)>;
    MysqlNullFlag null_flags[4]{};

    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].buffer = &video_id;
    result[0].is_unsigned = true;
    result[0].is_null = &null_flags[0];

    result[1].buffer_type = MYSQL_TYPE_TINY;
    result[1].buffer = &status;
    result[1].is_unsigned = true;
    result[1].is_null = &null_flags[1];

    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = object_key;
    result[2].buffer_length = static_cast<unsigned long>(sizeof(object_key));
    result[2].length = &object_key_length;
    result[2].is_null = &null_flags[2];

    result[3].buffer_type = MYSQL_TYPE_LONGLONG;
    result[3].buffer = &file_size;
    result[3].is_unsigned = true;
    result[3].is_null = &null_flags[3];

    if (mysql_stmt_bind_result(stmt.get(), result) != 0) {
        LogStmtError("mysql_stmt_bind_result(get uploading video)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_store_result(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_store_result(get uploading video)", stmt.get());
        return std::nullopt;
    }

    const int fetched = mysql_stmt_fetch(stmt.get());
    if (fetched == MYSQL_NO_DATA) {
        return std::nullopt;
    }
    if (fetched != 0 || null_flags[0] || null_flags[2] ||
        object_key_length >= sizeof(object_key)) {
        return std::nullopt;
    }

    UploadingVideo video{};
    video.video_id = video_id;
    video.status = status;
    video.object_key.assign(object_key, object_key_length);
    video.file_size = null_flags[3] ? 0 : file_size;
    return video;
}

bool MySqlMgr::MarkVideoTranscoding(
        unsigned long long video_id, unsigned long long author_id,
        unsigned long long actual_file_size
) {
    if (video_id == 0 || author_id == 0) {
        return false;
    }

    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        return false;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql || mysql_query(mysql, "START TRANSACTION") != 0) {
        return false;
    }

    static const char* kUpdateAssetSql =
            "UPDATE video_asset SET file_size = ? "
            "WHERE video_id = ? AND asset_type = 'original' AND quality = 'default'";
    auto update_asset = Prepare(mysql, kUpdateAssetSql);
    if (!update_asset) {
        Rollback(mysql);
        return false;
    }

    MYSQL_BIND asset_params[2]{};
    asset_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    asset_params[0].buffer = &actual_file_size;
    asset_params[0].is_unsigned = true;
    asset_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    asset_params[1].buffer = &video_id;
    asset_params[1].is_unsigned = true;
    if (mysql_stmt_bind_param(update_asset.get(), asset_params) != 0 ||
        mysql_stmt_execute(update_asset.get()) != 0) {
        LogStmtError("update uploaded video asset", update_asset.get());
        Rollback(mysql);
        return false;
    }
    update_asset.reset();

    static const char* kUpdateVideoSql =
            "UPDATE video SET status = 2 WHERE id = ? AND author_id = ? AND status = 1";
    auto update_video = Prepare(mysql, kUpdateVideoSql);
    if (!update_video) {
        Rollback(mysql);
        return false;
    }

    unsigned long long bound_video_id = video_id;
    unsigned long long bound_author_id = author_id;
    MYSQL_BIND video_params[2]{};
    video_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    video_params[0].buffer = &bound_video_id;
    video_params[0].is_unsigned = true;
    video_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    video_params[1].buffer = &bound_author_id;
    video_params[1].is_unsigned = true;
    if (mysql_stmt_bind_param(update_video.get(), video_params) != 0 ||
        mysql_stmt_execute(update_video.get()) != 0) {
        LogStmtError("mark video transcoding", update_video.get());
        Rollback(mysql);
        return false;
    }
    if (mysql_stmt_affected_rows(update_video.get()) != 1) {
        Rollback(mysql);
        return false;
    }
    update_video.reset();

    if (mysql_query(mysql, "COMMIT") != 0) {
        LogMysqlError("COMMIT", mysql);
        Rollback(mysql);
        return false;
    }
    return true;
}

int MySqlMgr::ResetPwd(const std::string& email, const std::string& password_hash) {
    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        std::cerr << "[MySqlMgr] MySQL is not available" << std::endl;
        return -1;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql) {
        std::cerr << "[MySqlMgr] no MySQL connection available" << std::endl;
        return -1;
    }

    static const char* kSql = "UPDATE user SET password_hash=? WHERE email=?";
    auto stmt = Prepare(mysql, kSql);
    if (!stmt) {
        return -1;
    }

    std::string hash = password_hash;
    std::string mail = email;
    unsigned long hash_length = static_cast<unsigned long>(hash.size());
    unsigned long email_length = static_cast<unsigned long>(mail.size());
    MYSQL_BIND params[2]{};
    BindString(params[0], hash, hash_length);
    BindString(params[1], mail, email_length);

    if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
        LogStmtError("mysql_stmt_bind_param(reset password)", stmt.get());
        return -1;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(reset password)", stmt.get());
        return -1;
    }

    const my_ulonglong affected = mysql_stmt_affected_rows(stmt.get());
    if (affected == static_cast<my_ulonglong>(-1)) {
        LogStmtError("mysql_stmt_affected_rows(reset password)", stmt.get());
        return -1;
    }
    return affected == 0 ? 1 : 0;
}

int MySqlMgr::UserLogin(const std::string& email, const std::string& password, UserProfile& user) {
    user = UserProfile{};

    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        std::cerr << "[MySqlMgr] MySQL is not available" << std::endl;
        return -1;
    }

    std::string stored_password_hash;
    unsigned long long db_user_id = 0;
    std::string account;
    std::string stored_email;
    std::string phone;
    std::string username;
    bool user_not_found = false;
    {
        MysqlConnGuard guard(mysql_mgr->GetConn());
        MYSQL* mysql = guard.Raw();
        if (!mysql) {
            std::cerr << "[MySqlMgr] no MySQL connection available" << std::endl;
            return -1;
        }

        static const char* kSql =
                "SELECT u.user_id, u.account, u.email, COALESCE(u.phone, ''), u.password_hash, "
                "COALESCE(i.username, '') FROM user u "
                "LEFT JOIN user_info i ON i.user_id = u.user_id WHERE u.email=? LIMIT 1";
        auto stmt = Prepare(mysql, kSql);
        if (!stmt) {
            return -1;
        }

        std::string mail = email;
        unsigned long email_length = static_cast<unsigned long>(mail.size());
        MYSQL_BIND param{};
        BindString(param, mail, email_length);
        if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
            LogStmtError("mysql_stmt_bind_param(user login)", stmt.get());
            return -1;
        }
        if (mysql_stmt_execute(stmt.get()) != 0) {
            LogStmtError("mysql_stmt_execute(user login)", stmt.get());
            return -1;
        }

        char account_buffer[65]{};
        char email_buffer[256]{};
        char phone_buffer[33]{};
        char hash_buffer[256]{};
        char username_buffer[257]{};
        unsigned long account_length = 0;
        unsigned long email_length_value = 0;
        unsigned long phone_length = 0;
        unsigned long hash_length = 0;
        unsigned long username_length = 0;
        MYSQL_BIND result[6]{};
        using MysqlNullFlag = std::remove_pointer_t<decltype(result[0].is_null)>;
        MysqlNullFlag null_flags[6]{};

        result[0].buffer_type = MYSQL_TYPE_LONGLONG;
        result[0].buffer = &db_user_id;
        result[0].is_null = &null_flags[0];
        result[0].is_unsigned = true;

        result[1].buffer_type = MYSQL_TYPE_STRING;
        result[1].buffer = account_buffer;
        result[1].buffer_length = static_cast<unsigned long>(sizeof(account_buffer));
        result[1].length = &account_length;
        result[1].is_null = &null_flags[1];

        result[2].buffer_type = MYSQL_TYPE_STRING;
        result[2].buffer = email_buffer;
        result[2].buffer_length = static_cast<unsigned long>(sizeof(email_buffer));
        result[2].length = &email_length_value;
        result[2].is_null = &null_flags[2];

        result[3].buffer_type = MYSQL_TYPE_STRING;
        result[3].buffer = phone_buffer;
        result[3].buffer_length = static_cast<unsigned long>(sizeof(phone_buffer));
        result[3].length = &phone_length;
        result[3].is_null = &null_flags[3];

        result[4].buffer_type = MYSQL_TYPE_STRING;
        result[4].buffer = hash_buffer;
        result[4].buffer_length = static_cast<unsigned long>(sizeof(hash_buffer));
        result[4].length = &hash_length;
        result[4].is_null = &null_flags[4];

        result[5].buffer_type = MYSQL_TYPE_STRING;
        result[5].buffer = username_buffer;
        result[5].buffer_length = static_cast<unsigned long>(sizeof(username_buffer));
        result[5].length = &username_length;
        result[5].is_null = &null_flags[5];

        if (mysql_stmt_bind_result(stmt.get(), result) != 0) {
            LogStmtError("mysql_stmt_bind_result(user login)", stmt.get());
            return -1;
        }
        if (mysql_stmt_store_result(stmt.get()) != 0) {
            LogStmtError("mysql_stmt_store_result(user login)", stmt.get());
            return -1;
        }

        const int fetch_result = mysql_stmt_fetch(stmt.get());
        if (fetch_result == MYSQL_NO_DATA) {
            user_not_found = true;
        } else if (fetch_result != 0) {
            LogStmtError("mysql_stmt_fetch(user login)", stmt.get());
            return -1;
        } else if (null_flags[0] || null_flags[4] || hash_length == 0 ||
                   hash_length >= sizeof(hash_buffer)) {
            return -1;
        } else {
            stored_password_hash.assign(hash_buffer, hash_length);
            account.assign(account_buffer, account_length);
            stored_email.assign(email_buffer, email_length_value);
            if (!null_flags[3])
                phone.assign(phone_buffer, phone_length);
            if (!null_flags[5])
                username.assign(username_buffer, username_length);
        }
    }

    if (user_not_found) {
        static const std::string kDummyHash =
                "pbkdf2_sha256$120000$Pb80mue4Gif2W/xfgIMMRA==$8Z5ZV2UYg5GChRXGq5deKSnFz/NMw+vHFvzSzb9o1kQ=";
        security::PasswordUtil::Verify(password, kDummyHash);
        return 1;
    }
    if (!security::PasswordUtil::Verify(password, stored_password_hash)) {
        return 2;
    }
    if (db_user_id == 0) {
        return -1;
    }

    user.user_id = db_user_id;
    user.account = std::move(account);
    user.email = std::move(stored_email);
    user.phone = std::move(phone);
    user.username = std::move(username);
    return 0;
}
std::optional<std::vector<MySqlMgr::VideoStatus>> MySqlMgr::GetVideoStatuses(
        unsigned long long author_id, const std::vector<unsigned long long>& video_ids
) {
    if (author_id == 0) {
        return std::nullopt;
    }
    if (video_ids.empty()) {
        return std::vector<VideoStatus>{};
    }

    auto mysql_mgr = MySqlMgr::GetInstance();
    if (!mysql_mgr->IsValid()) {
        return std::nullopt;
    }

    MysqlConnGuard guard(mysql_mgr->GetConn());
    MYSQL* mysql = guard.Raw();
    if (!mysql) {
        return std::nullopt;
    }

    std::string sql =
            "SELECT id, status FROM video "
            "WHERE author_id = ? AND deleted_at IS NULL AND id IN (";
    for (std::size_t i = 0; i < video_ids.size(); ++i) {
        if (i > 0) {
            sql.push_back(',');
        }
        sql.push_back('?');
    }
    sql.push_back(')');

    auto stmt = Prepare(mysql, sql.c_str());
    if (!stmt) {
        return std::nullopt;
    }

    std::vector<unsigned long long> ids = video_ids;
    std::vector<MYSQL_BIND> params(ids.size() + 1);
    params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    params[0].buffer = &author_id;
    params[0].is_unsigned = true;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        params[i + 1].buffer_type = MYSQL_TYPE_LONGLONG;
        params[i + 1].buffer = &ids[i];
        params[i + 1].is_unsigned = true;
    }
    if (mysql_stmt_bind_param(stmt.get(), params.data()) != 0) {
        LogStmtError("mysql_stmt_bind_param(get video statuses)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(get video statuses)", stmt.get());
        return std::nullopt;
    }

    unsigned long long video_id = 0;
    unsigned char status = 0;
    MYSQL_BIND result[2]{};
    using MysqlNullFlag = std::remove_pointer_t<decltype(result[0].is_null)>;
    MysqlNullFlag null_flags[2]{};

    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].buffer = &video_id;
    result[0].is_unsigned = true;
    result[0].is_null = &null_flags[0];

    result[1].buffer_type = MYSQL_TYPE_TINY;
    result[1].buffer = &status;
    result[1].is_unsigned = true;
    result[1].is_null = &null_flags[1];

    if (mysql_stmt_bind_result(stmt.get(), result) != 0) {
        LogStmtError("mysql_stmt_bind_result(get video statuses)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_store_result(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_store_result(get video statuses)", stmt.get());
        return std::nullopt;
    }

    std::vector<VideoStatus> videos;
    while (true) {
        const int fetched = mysql_stmt_fetch(stmt.get());
        if (fetched == MYSQL_NO_DATA) {
            break;
        }
        if (fetched != 0) {
            LogStmtError("mysql_stmt_fetch(get video statuses)", stmt.get());
            return std::nullopt;
        }
        if (null_flags[0]) {
            continue;
        }
        videos.push_back(VideoStatus{video_id, static_cast<int>(status)});
    }
    return videos;
}
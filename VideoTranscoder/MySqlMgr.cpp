//
// Created by mpt on 2026/11/2.
//

#include "MySqlMgr.h"

#include "ConfigMgr.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "Types.h"
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
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_errno(mysql) << " "
              << mysql_error(mysql) << std::endl;
}

void LogStmtError(const char* where, MYSQL_STMT* stmt) {
    std::cerr << "[MySqlMgr] " << where << " failed: " << mysql_stmt_errno(stmt) << " "
              << mysql_stmt_error(stmt) << std::endl;
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

void BindULL(MYSQL_BIND& bind, unsigned long long& value) {
    bind.buffer_type = MYSQL_TYPE_LONGLONG;
    bind.buffer = &value;
    bind.is_unsigned = true;
}

void BindUInt(MYSQL_BIND& bind, unsigned int& value) {
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = &value;
    bind.is_unsigned = true;
}

void BindDouble(MYSQL_BIND& bind, double& value) {
    bind.buffer_type = MYSQL_TYPE_DOUBLE;
    bind.buffer = &value;
}

void BindString(MYSQL_BIND& bind, const std::string& value, unsigned long& length) {
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(value.data());
    bind.buffer_length = static_cast<unsigned long>(value.size());
    bind.length = &length;
}

bool ExecSimple(MYSQL* mysql, const char* sql) {
    if (mysql_query(mysql, sql) != 0) {
        LogMysqlError(sql, mysql);
        return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- 连接租借

MysqlConnLease::MysqlConnLease(MySqlMgr& mgr) : _mgr(mgr), _conn(mgr.GetConn()) {}

MysqlConnLease::~MysqlConnLease() {
    if (_conn) {
        _mgr.ReturnConn(std::move(_conn));
    }
}

// ---------------------------------------------------------------- 连接池

MySqlPool::MySqlPool(const std::string& host, unsigned int port, const std::string& user,
                     const std::string& password, const std::string& db, std::size_t size)
    : _pool_size(size), _host(host), _port(port), _user(user), _password(password), _dbname(db) {
    if (_pool_size == 0) {
        _pool_size = 1;
    }

    std::size_t created = 0;
    for (std::size_t i = 0; i < _pool_size; ++i) {
        if (auto conn = createdConn()) {
            _mysql_conns.push(std::move(conn));
            ++created;
        } else {
            std::cerr << "[MySqlMgr] create connection " << i << " failed" << std::endl;
        }
    }

    if (created == 0) {
        throw std::runtime_error("mysql pool created zero valid connections");
    }
    std::cout << "[MySqlMgr] pool ready, connections = " << created << std::endl;
}

MysqlConnUPtr MySqlPool::createdConn() const {
    MYSQL* raw = mysql_init(nullptr);
    if (!raw) {
        std::cerr << "[MySqlMgr] mysql_init failed" << std::endl;
        return nullptr;
    }

    MysqlConnUPtr conn(raw, MysqlConnDeleter{});
    if (!mysql_real_connect(raw, _host.c_str(), _user.c_str(), _password.c_str(), _dbname.c_str(),
                            _port, nullptr, 0)) {
        std::cerr << "[MySqlMgr] connect failed: " << mysql_error(raw) << std::endl;
        return nullptr;
    }

    mysql_set_character_set(raw, "utf8mb4");
    return conn;
}

MysqlConnUPtr MySqlPool::GetConnection(int timeout_ms) {
    std::unique_lock<std::mutex> lock(_mtx);
    const bool ready = _cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return !_mysql_conns.empty() || _b_stop;
    });
    if (_b_stop || !ready) {
        return nullptr;
    }

    MysqlConnUPtr conn = std::move(_mysql_conns.front());
    _mysql_conns.pop();
    lock.unlock();

    if (mysql_ping(conn.get()) != 0) {
        std::cerr << "[MySqlMgr] stale connection, reconnecting" << std::endl;
        conn = createdConn();
    }
    return conn;
}

void MySqlPool::ReturnConnection(MysqlConnUPtr conn) {
    if (!conn) {
        return;
    }

    std::unique_lock<std::mutex> lock(_mtx);
    if (_b_stop) {
        return;
    }
    mysql_reset_connection(conn.get());
    _mysql_conns.push(std::move(conn));
    lock.unlock();
    _cv.notify_one();
}

void MySqlPool::Stop() {
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_b_stop) {
            return;
        }
        _b_stop = true;
    }
    _cv.notify_all();

    std::lock_guard<std::mutex> lock(_mtx);
    while (!_mysql_conns.empty()) {
        _mysql_conns.pop();
    }
}

MySqlPool::~MySqlPool() {
    Stop();
}

// ---------------------------------------------------------------- MySqlMgr

MySqlMgr::MySqlMgr() {
    auto& cfg = ConfigMgr::Inst();
    const std::string host = cfg["MySQLServer"]["Host"];
    const std::string user = cfg["MySQLServer"]["User"];
    const std::string password = cfg["MySQLServer"]["Password"];
    const std::string db = cfg["MySQLServer"]["Db"];

    unsigned int port = 3306;
    std::size_t pool_size = 8;
    try {
        const std::string port_text = cfg["MySQLServer"]["Port"];
        if (!port_text.empty()) {
            port = static_cast<unsigned int>(std::stoi(port_text));
        }
        const std::string size_text = cfg["MySQLServer"]["Pool_size"];
        if (!size_text.empty()) {
            pool_size = static_cast<std::size_t>(std::stoi(size_text));
        }
    } catch (const std::exception& e) {
        std::cerr << "[MySqlMgr] bad config: " << e.what() << std::endl;
    }
    if (pool_size < 1) {
        pool_size = 1;
    }
    if (pool_size > 32) {
        pool_size = 32;
    }

    try {
        _pool = std::make_unique<MySqlPool>(host, port, user, password, db, pool_size);
    } catch (const std::exception& e) {
        std::cerr << "[MySqlMgr] init failed: " << e.what() << std::endl;
        _pool.reset();
    }
}

MysqlConnUPtr MySqlMgr::GetConn() {
    if (!_pool) {
        return nullptr;
    }
    return _pool->GetConnection();
}

void MySqlMgr::ReturnConn(MysqlConnUPtr conn) {
    if (_pool) {
        _pool->ReturnConnection(std::move(conn));
    }
}

void MySqlMgr::Stop() {
    if (_pool) {
        _pool->Stop();
        _pool.reset();
    }
}

std::vector<unsigned long long> MySqlMgr::PickPendingVideoIds(std::size_t limit) {
    std::vector<unsigned long long> ids;
    MysqlConnLease lease(*this);
    if (!lease) {
        std::cerr << "[MySqlMgr] PickPendingVideoIds: no connection" << std::endl;
        return ids;
    }

    static const char* kSql =
            "SELECT v.id FROM video v "
            "WHERE v.status = 2 AND v.deleted_at IS NULL "
            "  AND (v.transcode_claimed_at IS NULL "
            "       OR v.transcode_claimed_at < NOW(3) - INTERVAL 10 MINUTE) "
            "ORDER BY v.updated_at "
            "LIMIT ?";

    auto stmt = Prepare(lease.Raw(), kSql);
    if (!stmt) {
        return ids;
    }

    unsigned long long limit_value = static_cast<unsigned long long>(limit);
    MYSQL_BIND param{};
    BindULL(param, limit_value);
    if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
        LogStmtError("mysql_stmt_bind_param(pick pending)", stmt.get());
        return ids;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(pick pending)", stmt.get());
        return ids;
    }

    unsigned long long video_id = 0;
    MYSQL_BIND result{};
    BindULL(result, video_id);
    if (mysql_stmt_bind_result(stmt.get(), &result) != 0) {
        LogStmtError("mysql_stmt_bind_result(pick pending)", stmt.get());
        return ids;
    }

    int rc = 0;
    while ((rc = mysql_stmt_fetch(stmt.get())) == 0) {
        ids.push_back(video_id);
    }
    if (rc != MYSQL_NO_DATA) {
        LogStmtError("mysql_stmt_fetch(pick pending)", stmt.get());
        ids.clear();
    }
    return ids;
}

bool MySqlMgr::ClaimVideo(unsigned long long video_id) {
    MysqlConnLease lease(*this);
    if (!lease) {
        std::cerr << "[MySqlMgr] ClaimVideo: no connection" << std::endl;
        return false;
    }

    static const char* kSql =
            "UPDATE video SET "
            "  transcode_claimed_at = NOW(3), "
            "  transcode_started_at = NOW(3), "
            "  transcode_attempts   = transcode_attempts + 1, "
            "  transcode_error      = NULL "
            "WHERE id = ? AND status = 2 "
            "  AND (transcode_claimed_at IS NULL "
            "       OR transcode_claimed_at < NOW(3) - INTERVAL 10 MINUTE)";

    auto stmt = Prepare(lease.Raw(), kSql);
    if (!stmt) {
        return false;
    }

    unsigned long long id_value = video_id;
    MYSQL_BIND param{};
    BindULL(param, id_value);
    if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
        LogStmtError("mysql_stmt_bind_param(claim)", stmt.get());
        return false;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(claim)", stmt.get());
        return false;
    }

    return mysql_stmt_affected_rows(stmt.get()) == 1;
}

std::optional<SourceInfo> MySqlMgr::GetSourceInfo(unsigned long long video_id) {
    MysqlConnLease lease(*this);
    if (!lease) {
        std::cerr << "[MySqlMgr] GetSourceInfo: no connection" << std::endl;
        return std::nullopt;
    }

    static const char* kSql =
            "SELECT va.storage_key, v.author_id "
            "FROM video_asset va "
            "JOIN video v ON v.id = va.video_id "
            "WHERE va.video_id = ? AND va.asset_type = 'original' "
            "ORDER BY va.id LIMIT 1";

    auto stmt = Prepare(lease.Raw(), kSql);
    if (!stmt) {
        return std::nullopt;
    }

    unsigned long long id_value = video_id;
    MYSQL_BIND param{};
    BindULL(param, id_value);
    if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
        LogStmtError("mysql_stmt_bind_param(source info)", stmt.get());
        return std::nullopt;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(source info)", stmt.get());
        return std::nullopt;
    }

    char key_buffer[513]{};
    unsigned long key_length = 0;
    unsigned long long uid_value = 0;

    MYSQL_BIND results[2]{};
    results[0].buffer_type = MYSQL_TYPE_STRING;
    results[0].buffer = key_buffer;
    results[0].buffer_length = static_cast<unsigned long>(sizeof(key_buffer) - 1);
    results[0].length = &key_length;
    BindULL(results[1], uid_value);
    results[1].buffer_length = static_cast<unsigned long>(sizeof(uid_value));

    if (mysql_stmt_bind_result(stmt.get(), results) != 0) {
        LogStmtError("mysql_stmt_bind_result(source info)", stmt.get());
        return std::nullopt;
    }

    const int rc = mysql_stmt_fetch(stmt.get());
    if (rc == MYSQL_NO_DATA) {
        return std::nullopt;
    }
    if (rc != 0) {
        LogStmtError("mysql_stmt_fetch(source info)", stmt.get());
        return std::nullopt;
    }
    if (key_length == 0 || key_length >= sizeof(key_buffer)) {
        return std::nullopt;
    }

    SourceInfo info;
    info.object_key.assign(key_buffer, key_length);
    info.uid = uid_value;
    return info;
}

bool MySqlMgr::CommitTranscode(const EncodeResult& result) {
    MysqlConnLease lease(*this);
    if (!lease) {
        std::cerr << "[MySqlMgr] CommitTranscode: no connection" << std::endl;
        return false;
    }
    MYSQL* mysql = lease.Raw();

    if (!ExecSimple(mysql, "START TRANSACTION")) {
        return false;
    }

    static const char* kAssetSql =
            "INSERT INTO video_asset "
            "  (video_id, asset_type, quality, width, height, bitrate, codec, storage_key, file_size) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) AS new "
            "ON DUPLICATE KEY UPDATE "
            "  width = new.width, height = new.height, bitrate = new.bitrate, "
            "  codec = new.codec, storage_key = new.storage_key, file_size = new.file_size";

    for (const auto& asset : result.assets) {
        auto stmt = Prepare(mysql, kAssetSql);
        if (!stmt) {
            ExecSimple(mysql, "ROLLBACK");
            return false;
        }

        unsigned long long video_id_value = result.video_id;
        std::string type_value = asset.asset_type;
        std::string quality_value = asset.quality;
        unsigned int width_value = asset.width;
        unsigned int height_value = asset.height;
        unsigned int bitrate_value = asset.bitrate;
        std::string codec_value = asset.codec;
        std::string key_value = asset.storage_key;
        unsigned long long size_value = asset.file_size;

        unsigned long type_length = static_cast<unsigned long>(type_value.size());
        unsigned long quality_length = static_cast<unsigned long>(quality_value.size());
        unsigned long codec_length = static_cast<unsigned long>(codec_value.size());
        unsigned long key_length = static_cast<unsigned long>(key_value.size());

        MYSQL_BIND params[9]{};
        BindULL(params[0], video_id_value);
        BindString(params[1], type_value, type_length);
        BindString(params[2], quality_value, quality_length);
        BindUInt(params[3], width_value);
        BindUInt(params[4], height_value);
        BindUInt(params[5], bitrate_value);
        BindString(params[6], codec_value, codec_length);
        BindString(params[7], key_value, key_length);
        BindULL(params[8], size_value);

        if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
            LogStmtError("mysql_stmt_bind_param(commit asset)", stmt.get());
            ExecSimple(mysql, "ROLLBACK");
            return false;
        }
        if (mysql_stmt_execute(stmt.get()) != 0) {
            LogStmtError("mysql_stmt_execute(commit asset)", stmt.get());
            ExecSimple(mysql, "ROLLBACK");
            return false;
        }
    }

    static const char* kVideoSql =
            "UPDATE video SET "
            "  status = 3, "
            "  review_status = 1, "
            "  published_at = COALESCE(published_at, NOW(3)), "
            "  duration_ms = ?, width = ?, height = ?, fps = ?, "
            "  cover_key = COALESCE(NULLIF(?, ''), cover_key), "
            "  transcode_error = NULL, "
            "  transcode_claimed_at = NULL, "
            "  transcode_finished_at = NOW(3) "
            "WHERE id = ?";

    auto video_stmt = Prepare(mysql, kVideoSql);
    if (!video_stmt) {
        ExecSimple(mysql, "ROLLBACK");
        return false;
    }

    unsigned long long video_id_value = result.video_id;
    unsigned int duration_value = result.duration_ms;
    unsigned int width_value = result.width;
    unsigned int height_value = result.height;
    double fps_value = result.fps;
    std::string cover_value = result.cover_key;
    unsigned long cover_length = static_cast<unsigned long>(cover_value.size());

    MYSQL_BIND video_params[6]{};
    BindUInt(video_params[0], duration_value);
    BindUInt(video_params[1], width_value);
    BindUInt(video_params[2], height_value);
    BindDouble(video_params[3], fps_value);
    BindString(video_params[4], cover_value, cover_length);
    BindULL(video_params[5], video_id_value);

    if (mysql_stmt_bind_param(video_stmt.get(), video_params) != 0) {
        LogStmtError("mysql_stmt_bind_param(commit video)", video_stmt.get());
        ExecSimple(mysql, "ROLLBACK");
        return false;
    }
    if (mysql_stmt_execute(video_stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(commit video)", video_stmt.get());
        ExecSimple(mysql, "ROLLBACK");
        return false;
    }

    if (!ExecSimple(mysql, "COMMIT")) {
        ExecSimple(mysql, "ROLLBACK");
        return false;
    }
    return true;
}

bool MySqlMgr::FailTranscode(unsigned long long video_id, const std::string& reason) {
    MysqlConnLease lease(*this);
    if (!lease) {
        std::cerr << "[MySqlMgr] FailTranscode: no connection" << std::endl;
        return false;
    }

    static const char* kSql =
            "UPDATE video SET "
            "  status = IF(transcode_attempts >= 3, 4, 2), "
            "  transcode_error = ?, "
            "  transcode_claimed_at = NULL, "
            "  transcode_finished_at = NOW(3) "
            "WHERE id = ?";

    auto stmt = Prepare(lease.Raw(), kSql);
    if (!stmt) {
        return false;
    }

    std::string reason_value = reason;
    if (reason_value.size() > 1024) {
        reason_value.resize(1024);
    }
    unsigned long long id_value = video_id;
    unsigned long reason_length = static_cast<unsigned long>(reason_value.size());

    MYSQL_BIND params[2]{};
    BindString(params[0], reason_value, reason_length);
    BindULL(params[1], id_value);

    if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
        LogStmtError("mysql_stmt_bind_param(fail)", stmt.get());
        return false;
    }
    if (mysql_stmt_execute(stmt.get()) != 0) {
        LogStmtError("mysql_stmt_execute(fail)", stmt.get());
        return false;
    }

    return mysql_stmt_affected_rows(stmt.get()) >= 1;
}
//
// Created by mpt on 2026/6/30.
//
#ifndef GETSERVER_CONST_H
#define GETSERVER_CONST_H
#include <map>
#include <string>
#include <iostream>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <queue>
#include <chrono>
#include <mutex>
#include <cstddef>
#include <boost/asio.hpp>

enum ErrorCodes {
    Success = 0,
    Error_Json = 1001,         //Json解析错误
    RPCFailed = 1002,          //RPC请求错误
    VarifyBackErr = 1003,      //验证码返回失败
    Error_Email = 1004,        // 邮箱格式非法
    TooManyRequests = 1005,    // 服务器被限流
    IPUnknown = 1006,          // IP未知
    Error_Account = 1007,      // 账号格式非法
    Error_Password = 1008,     // 密码格式非法
    Error_Username = 1009,     // 昵称格式非法
    Error_Phone = 1010,        // 手机号格式非法
    Error_Code = 1011,         // 验证码错误或已过期
    UserExists = 1012,         // 账号/邮箱/手机号已注册
    DBError = 1013,            // 数据库操作失败
    ErrorPwdNotSame = 1014,    // 两次输入的密码不一致
    MySqlError = 1015,         // MySQL 操作失败
    UserUNExists = 1016,       // 用户不存在（保留，外部不再直接暴露）
    ErrorPwdNotSuccess = 1017, // 邮箱或密码错误
};

enum LimiterResult {
    LimiterError   = -1,   // Redis 异常 -> 应答 503
    LimiterDenied  =  0,   // 命中限流   -> 应答 429
    LimiterAllowed =  1,   // 放行
};

constexpr int kGlobalPerSec = 20;    // 全局每秒，保护 SMTP 配额
constexpr int kMailCooldown = 60;    // 同邮箱 60s 一次
constexpr int kMailPerDay   = 10;    // 同邮箱 每天 10 封
constexpr int kIpCooldown   = 3;     // 同 IP 3s 一次
constexpr int kIpPerHour    = 30;    // 同 IP 每小时
constexpr int kIpPerDay     = 100;   // 同 IP 每天

// 注册接口限流：按全局、IP、邮箱三个维度限制，防止撞库和批量注册
constexpr int kRegGlobalPerSec = 20;  // 全站每秒最多 20 次注册尝试
constexpr int kRegIpPerMinute  = 20;  // 同 IP 每分钟 20 次
constexpr int kRegIpPerHour    = 120; // 同 IP 每小时 120 次
constexpr int kRegMailPerHour  = 3;   // 同邮箱每小时 3 次
constexpr int kRegMailPerDay   = 10;  // 同邮箱每天 10 次

// 重置密码接口限流
constexpr int kResetGlobalPerSec = 20;
constexpr int kResetIpPerMinute  = 10;
constexpr int kResetIpPerHour    = 30;
constexpr int kResetMailPerHour  = 10;
constexpr int kResetMailPerDay   = 20;

// 登录接口限流
constexpr int kLogicGlobalPerSec = 100;
constexpr int kLogicIpPerMinute  = 60;
constexpr int kLogicIpPerHour    = 600;
constexpr int kLogicMailPerHour  = 10;
constexpr int kLogicMailPerDay   = 30;


inline void LogThrottled(const std::string& what, const std::string& detail) {
    static std::mutex log_mtx;
    static std::chrono::steady_clock::time_point last{};
    static std::size_t suppressed = 0;
    std::lock_guard<std::mutex> lock(log_mtx);
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(1)) {
        ++suppressed;
        return;
    }
    std::string line = what + ": " + detail;
    if (suppressed > 0) {
        line += " (suppressed " + std::to_string(suppressed) + " in the last second)";
        suppressed = 0;
    }
    last = now;
    std::cout << line << std::endl;
}

#define VarifyCodeKey       "code_"              // 验证码缓存
#define VarifyCodeCdKey     "cd_mail_"           // 同邮箱冷却(60s 一次)
#define VarifyCodeCdKeyDay  "cd_mail_d_"         // 同邮箱每日配额
#define IpCdKey             "cd_ip_"             // 同 IP 冷却(3s 一次)
#define IpCdKeyHour         "cd_ip_h_"           // 同 IP 每小时配额
#define IpCdKeyDay          "cd_ip_d_"           // 同 IP 每日配额
#define ServerCdKey         "quota_global_"      // 全局每秒熔断
#define RegGlobalCdKey      "rl_reg_g_"          // 注册全局每秒
#define RegIpCdKeyMinute    "rl_reg_ip_m_"       // 注册 IP 每分钟
#define RegIpCdKeyHour      "rl_reg_ip_h_"       // 注册 IP 每小时
#define RegMailCdKeyHour    "rl_reg_mail_h_"     // 注册邮箱每小时
#define RegMailCdKeyDay     "rl_reg_mail_d_"     // 注册邮箱每天

#define ResetGlobalCdKey    "rl_reset_g_"
#define ResetIpCdKeyMinute  "rl_reset_ip_m_"
#define ResetIpCdKeyHour    "rl_reset_ip_h_"
#define ResetMailCdKeyHour  "rl_reset_mail_h_"
#define ResetMailCdKeyDay   "rl_reset_mail_d_"

#define LogicGlobalCdKey    "rl_login_g_"
#define LogicIpCdKeyMinute  "rl_login_ip_m_"
#define LogicIpCdKeyHour    "rl_login_ip_h_"
#define LogicMailCdKeyHour  "rl_login_mail_h_"
#define LogicMailCdKeyDay   "rl_login_mail_d_"
#endif //GETSERVER_CONST_H

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
    Error_Json = 1001,          // Json解析错误
    RPCFailed = 1002,           // RPC请求错误
    VarifyBackErr = 1003,       // 验证码返回失败
    Error_Email = 1004,         // 邮箱格式非法
    TooManyRequests = 1005,     // 服务器被限流
    IPUnknown = 1006,           // IP未知
    Error_Account = 1007,       // 账号格式非法
    Error_Password = 1008,      // 密码格式非法
    Error_Username = 1009,      // 昵称格式非法
    Error_Phone = 1010,         // 手机号格式非法
    Error_Code = 1011,          // 验证码错误或已过期
    UserExists = 1012,          // 账号/邮箱/手机号已注册
    DBError = 1013,             // 数据库操作失败
    ErrorPwdNotSame = 1014,     // 两次输入的密码不一致
    MySqlError = 1015,          // MySQL 操作失败
    UserUNExists = 1016,        // 用户不存在（保留，外部不再直接暴露）
    ErrorPwdNotSuccess = 1017,  // 邮箱或密码错误

    ChatTokenInvalid = 1020,     // token missing/wrong server/wrong user
    ChatTokenExpired = 1021,     // pending lease expired
    ChatTokenUsed = 1022,        // token already consumed
    ChatRedisError = 1023,       // redis unavailable or script failed
    ChatAlreadyLoggedIn = 1024,  // this session already logged in
};

enum MSG_ID {

    ID_CHAT_LOGIN_QEQ = 1005,  // 用户登录请求
    ID_CHAT_LOGIN_QSP = 1006,  // 用户登录回包
};

#define HEAD_TOTAL_LEN 4
#define HEAD_ID_LEN 2
#define HEAD_DATA_LEN 2
#define MAX_LENGTH (2 * 1024)
#define MAXMSGDEALQUE 1024
#endif  // GETSERVER_CONST_H

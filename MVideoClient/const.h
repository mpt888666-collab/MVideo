//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_CONST_H
#define MVIDEOCLIENT_CONST_H

#include <QMetaType>

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

    ErrorNetWork = 1025,
};

enum ReqId {
    ID_GET_VARIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002,        //注册
    ID_RESET_PWD = 1003,       //重置密码
    ID_LOGIN_USER = 1004,      //登录
    ID_CHAT_LOGIN_QEQ = 1005,  // 用户登录请求
    ID_CHAT_LOGIN_QSP = 1006,  // 用户登录回包
};

enum Modules {
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
    VARITYMOD = 3,
};

Q_DECLARE_METATYPE(ReqId)
Q_DECLARE_METATYPE(Modules)

#endif //MVIDEOCLIENT_CONST_H

//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_CONST_H
#define MVIDEOCLIENT_CONST_H

#include <QMetaType>
#include <QString>
#include <memory>

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
    NotFoundFile = 1026,
};

enum ReqId {
    ID_GET_VARIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002,        //注册
    ID_RESET_PWD = 1003,       //重置密码
    ID_LOGIN_USER = 1004,      //登录

    ID_CHAT_LOGIN_REQ = 1005,  // 用户登录请求
    ID_CHAT_LOGIN_RSP = 1006,  // 用户登录回包
    ID_UPLOAD_VIDEO = 1007,    //上传视频
    ID_UPLOAD_SERVER_CHECK = 1008, //服务器检查上传是否成功
};

enum Modules {
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
    VARITYMOD = 3,
    HOMEMOD = 4,
};

enum State {
    TicketRequested = 1,
};
struct ServerInfo {
    unsigned long long _uid{0};
    QString _chat_host;
    QString _chat_port;
    QString _token;
};

Q_DECLARE_METATYPE(ReqId)
Q_DECLARE_METATYPE(Modules)
Q_DECLARE_METATYPE(std::shared_ptr<ServerInfo>)

struct RetryTask {
    QString request_key;
    Modules mod;
    ReqId reqId;
    int retry_count{0};
    int max_retry{3};
    int delay_ms = 100;
    qint64 deadlineMs{0};
    std::function<void()> send_func;


};

inline bool isRetryable(int httpStatus, ErrorCodes ec) {
    if (httpStatus == 429)
        return true;                                    // 限流，等一会儿再来
    if (httpStatus >= 500 && httpStatus < 600)
        return true;                                    // 服务端抖动
    if (httpStatus == 0 && ec == ErrorNetWork)
        return true;                                    // 连不上/超时，压根没拿到状态码
    return false;
}

#endif //MVIDEOCLIENT_CONST_H

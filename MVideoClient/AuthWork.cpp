//
// Created by mpt on 2026/10/24.
//

#include "AuthWork.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkReply>

namespace {

qint64 toInteger(const QJsonValue& value)
{
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString())
        return value.toString().toLongLong();
    return 0;
}

int toPort(const QJsonValue& value)
{
    if (value.isDouble())
        return value.toInt();
    if (value.isString())
        return value.toString().toInt();
    return 0;
}

bool isHttpSuccess(int status)
{
    return status >= 200 && status < 300;
}

} // namespace

AuthWork::AuthWork(QObject* parent) : QObject(parent) {}

void AuthWork::parseReply(ReqId id, Modules mod, int httpStatus, int networkError, const QString& body){
    Q_UNUSED(id)

    const auto finishWithError = [this, mod](int error, const QString& message) {
        switch (mod) {
        case VARITYMOD:
            emit sigVerifyCodeParsed(error, message);
            break;
        case REGISTERMOD:
            emit sigRegisterParsed(error, message, 0);
            break;
        case RESETMOD:
            emit sigResetParsed(error, message);
            break;
        case LOGINMOD:
            emit sigLoginParsed(error, message, QString(), QString(), 0, 0);
            break;
        }
    };

    QJsonObject object;
    bool hasJsonObject = false;

    if (!body.trimmed().isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            object = document.object();
            hasJsonObject = true;
        }
    }

    if (!hasJsonObject) {
        if (networkError == QNetworkReply::OperationCanceledError)
            finishWithError(ErrorNetWork, QStringLiteral("请求超时或已取消"));
        else if (networkError != QNetworkReply::NoError)
            finishWithError(ErrorNetWork, QStringLiteral("网络错误，请检查服务器是否运行"));
        else
            finishWithError(Error_Json, defaultMessage(Error_Json));
        return;
    }

    int error = -1;

    if (mod == LOGINMOD && object.contains(QStringLiteral("ec")))
        error = object.value(QStringLiteral("ec")).toInt(-1);
    else if (object.contains(QStringLiteral("error")))
        error = object.value(QStringLiteral("error")).toInt(-1);
    else if (object.contains(QStringLiteral("ec")))
        error = object.value(QStringLiteral("ec")).toInt(-1);

    if (error < 0) {
        if (networkError == QNetworkReply::OperationCanceledError)
            finishWithError(ErrorNetWork, QStringLiteral("请求超时或已取消"));
        else if (networkError != QNetworkReply::NoError)
            finishWithError(ErrorNetWork, QStringLiteral("网络错误，请检查服务器是否运行"));
        else
            finishWithError(Error_Json, defaultMessage(Error_Json));
        return;
    }

    if (error == Success && !isHttpSuccess(httpStatus)) {
        finishWithError(ErrorNetWork,
                        QStringLiteral("服务器返回 HTTP %1").arg(httpStatus));
        return;
    }

    QString message = object.value(QStringLiteral("msg")).toString().trimmed();
    if (message.isEmpty())
        message = defaultMessage(error);

    if (error != Success) {
        finishWithError(error, message);
        return;
    }

    switch (mod) {
    case VARITYMOD:
        emit sigVerifyCodeParsed(Success,
            object.value(QStringLiteral("msg")).toString().isEmpty() ? QStringLiteral("验证码已发送") : message);
        break;
    case REGISTERMOD:
        emit sigRegisterParsed(
            Success,
            object.value(QStringLiteral("msg")).toString().isEmpty()
                ? QStringLiteral("注册成功，请登录")
                : message,
            toInteger(object.value(QStringLiteral("user_id"))));
        break;
    case RESETMOD:
        emit sigResetParsed(Success,
                            object.value(QStringLiteral("msg")).toString().isEmpty()
                                ? QStringLiteral("密码重置成功")
                                : message);
        break;
    case LOGINMOD:
        emit sigLoginParsed(
            Success,
            object.value(QStringLiteral("msg")).toString().isEmpty()
                ? QStringLiteral("登录成功")
                : message,
            object.value(QStringLiteral("token")).toString(),
            object.value(QStringLiteral("host")).toString(),
            toPort(object.value(QStringLiteral("port"))),
            toInteger(object.value(QStringLiteral("user_id"))));
        break;
    }
}

QString AuthWork::defaultMessage(int error)
{
    switch (error) {
    case Success:
        return QStringLiteral("操作成功");
    case Error_Json:
        return QStringLiteral("服务器返回数据格式错误");
    case RPCFailed:
        return QStringLiteral("请求处理失败");
    case VarifyBackErr:
        return QStringLiteral("验证码服务返回失败");
    case Error_Email:
        return QStringLiteral("邮箱格式不正确");
    case TooManyRequests:
        return QStringLiteral("请求过于频繁，请稍后再试");
    case IPUnknown:
        return QStringLiteral("无法识别请求来源");
    case Error_Account:
        return QStringLiteral("账号格式不正确");
    case Error_Password:
        return QStringLiteral("密码格式不正确");
    case Error_Username:
        return QStringLiteral("用户名格式不正确");
    case Error_Phone:
        return QStringLiteral("手机号格式不正确");
    case Error_Code:
        return QStringLiteral("验证码错误或已过期");
    case UserExists:
        return QStringLiteral("账号、邮箱或手机号已注册");
    case DBError:
        return QStringLiteral("数据库操作失败");
    case ErrorPwdNotSame:
        return QStringLiteral("两次输入的密码不一致");
    case MySqlError:
        return QStringLiteral("数据库操作失败");
    case UserUNExists:
        return QStringLiteral("用户不存在");
    case ErrorPwdNotSuccess:
        return QStringLiteral("邮箱或密码错误");
    case ChatTokenInvalid:
        return QStringLiteral("登录凭证无效");
    case ChatTokenExpired:
        return QStringLiteral("登录凭证已过期");
    case ChatTokenUsed:
        return QStringLiteral("登录凭证已被使用");
    case ChatRedisError:
        return QStringLiteral("登录服务暂时不可用");
    case ChatAlreadyLoggedIn:
        return QStringLiteral("该账号已经登录");
    case ErrorNetWork:
        return QStringLiteral("网络连接失败");
    default:
        return QStringLiteral("操作失败（错误码 %1）").arg(error);
    }
}

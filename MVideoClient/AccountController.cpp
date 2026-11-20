//
// Created by mpt on 2026/11/18.
//

#include "AccountController.h"

#include <QJsonObject>

#include "ApiRoutes.h"
#include "AuthWork.h"
#include "HttpMgr.h"
#include "RequestId.h"
#include "UserInfo.h"

AccountController::AccountController(HttpMgr& http, AuthWork* authWork, QObject* parent)
    : QObject(parent), _http(http) {
    connect(authWork, &AuthWork::sigVerifyCodeParsed,
            this, &AccountController::verifyCodeFinished, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigRegisterParsed,
            this, &AccountController::registerFinished, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigResetParsed,
            this, &AccountController::resetPasswordFinished, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigLoginParsed,
            this, &AccountController::onLoginParsed, Qt::QueuedConnection);
}

void AccountController::requestVerifyCode(const QString& email) {
    QJsonObject json;
    json["email"] = email;
    _http.PostRequest(api::VerifyCode(), json, ReqId::ID_GET_VARIFY_CODE, Modules::VARITYMOD);
}

void AccountController::requestRegister(const QString& account, const QString& password,
                                       const QString& username, const QString& email,
                                       const QString& phone, const QString& code) {
    QJsonObject json;
    json["account"] = account;
    json["password"] = password;
    json["username"] = username;
    json["email"] = email;
    json["phone"] = phone;
    json["code"] = code;
    _http.PostRequest(api::Register(), json, ReqId::ID_REG_USER, Modules::REGISTERMOD);
}

void AccountController::requestReset(const QString& email, const QString& code,
                                     const QString& password, const QString& confirm) {
    QJsonObject json;
    json["email"] = email;
    json["code"] = code;
    json["password"] = password;
    json["confirm"] = confirm;
    _http.PostRequest(api::ResetPassword(), json, ReqId::ID_RESET_PWD, Modules::RESETMOD);
}

void AccountController::requestLogin(const QString& email, const QString& password) {
    if (!_pendingLoginRequestId.isEmpty()) {
        return;
    }

    _pendingLoginRequestId = MakeRequestId();
    _pendingLoginEmail = email;

    QJsonObject json;
    json["email"] = email;
    json["password"] = password;
    json["request_id"] = _pendingLoginRequestId;
    _http.PostRequest(api::Login(), json, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}

void AccountController::onLoginParsed(int error, const QString& message, const LoginResult& result) {
    if (error == ErrorCodes::Success) {
        LoginResult info = result;
        if (info.email.isEmpty()) {
            info.email = _pendingLoginEmail;
        }
        UserInfo::GetInstance()->applyLogin(info);
        _pendingLoginRequestId.clear();
        _pendingLoginEmail.clear();
        emit loginSucceeded(info);
        emit loginFinished(error, message);
        return;
    }

    _pendingLoginRequestId.clear();
    _pendingLoginEmail.clear();
    emit loginFinished(error, message);
}

void AccountController::resetPending() {
    _pendingLoginRequestId.clear();
    _pendingLoginEmail.clear();
}
//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_ACCOUNTCONTROLLER_H
#define MVIDEOCLIENT_ACCOUNTCONTROLLER_H

#include <QObject>
#include <QString>

#include "AuthTypes.h"

class HttpMgr;
class AuthWork;

class AccountController : public QObject {
    Q_OBJECT

public:
    AccountController(HttpMgr& http, AuthWork* authWork, QObject* parent = nullptr);

    void requestVerifyCode(const QString& email);
    void requestRegister(const QString& account, const QString& password,
                         const QString& username, const QString& email,
                         const QString& phone, const QString& code);
    void requestReset(const QString& email, const QString& code,
                      const QString& password, const QString& confirm);
    void requestLogin(const QString& email, const QString& password);
    void resetPending();

signals:
    void verifyCodeFinished(int error, const QString& message);
    void registerFinished(int error, const QString& message, qint64 userId);
    void resetPasswordFinished(int error, const QString& message);
    void loginFinished(int error, const QString& message);
    void loginSucceeded(const LoginResult& result);

private:
    void onLoginParsed(int error, const QString& message, const LoginResult& result);

    HttpMgr& _http;
    QString _pendingLoginRequestId;
    QString _pendingLoginEmail;
};

#endif //MVIDEOCLIENT_ACCOUNTCONTROLLER_H
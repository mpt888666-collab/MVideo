//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_AUTHSERVER_H
#define MVIDEOCLIENT_AUTHSERVER_H
#include <QObject>
#include <qqmlintegration.h>

#include "HttpMgr.h"

class AuthWork;
class QThread;

class AuthServer : public QObject{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    AuthServer();
    ~AuthServer() override;

    Q_INVOKABLE void requestVarityCode(const QString& email);
    Q_INVOKABLE void requestRegister(const QString& account, const QString& password,
        const QString& username, const QString& email, const QString& phone, const QString& code);
    Q_INVOKABLE void requestReset(const QString& email, const QString& code, const QString& password, const QString& confirm);
    Q_INVOKABLE void requestLogin(const QString& email, const QString& password, const QString& request_id);

signals:
    void verifyCodeFinished(int error, const QString& message);
    void registerFinished(int error, const QString& message, qint64 userId);
    void resetPasswordFinished(int error, const QString& message);
    void loginFinished(int error, const QString& message, const QString& token,
                       const QString& host, int port, qint64 userId);

private slots:
    void onVerifyCodeParsed(int error, const QString& message);
    void onRegisterParsed(int error, const QString& message, qint64 userId);
    void onResetParsed(int error, const QString& message);
    void onLoginParsed(int error, const QString& message, const QString& token,
                       const QString& host, int port, qint64 userId);

private:
    HttpMgr _httpMgr;
    QThread* _authThread{nullptr};
    AuthWork* _authWork{nullptr};
};


#endif //MVIDEOCLIENT_AUTHSERVER_H

//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_AUTHSERVER_H
#define MVIDEOCLIENT_AUTHSERVER_H
#include <QObject>
#include <qqmlintegration.h>

#include "HttpMgr.h"
#include "AuthTypes.h"

struct PendingUpload {
    QString requestId;
    QString localPath;
    QString title;
    QStringList tags;
    QString objectKey;
    QString uploadUrl;
    qint64  videoId{0};
    qint64  createdAt{0};
    int     percent{0};
    State   state{State::TicketRequested};
};

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
    Q_INVOKABLE void requestLogin(const QString& email, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void uploadVideo(const QString& fileUrl, const QString& title, const QString& description, const QStringList& tags);

signals:
    void verifyCodeFinished(int error, const QString& message);
    void registerFinished(int error, const QString& message, qint64 userId);
    void resetPasswordFinished(int error, const QString& message);
    void loginFinished(int error, const QString& message);
    void uploadFailed(int error, const QString& message);
    void uploadSuccess(int error, const QString& message);
    void serverCheckComplete(bool);

private slots:
    void onVerifyCodeParsed(int error, const QString& message);
    void onRegisterParsed(int error, const QString& message, qint64 userId);
    void onResetParsed(int error, const QString& message);
    void onLoginParsed(int error, const QString& message, const LoginResult& result);
    void onHomeUpLoadParsed(int error, const QString& message, ResponseVideoData);
    void slotUploadConfirm(const ResponseVideoData& data);

private:
    HttpMgr _httpMgr;
    QThread* _authThread{nullptr};
    AuthWork* _authWork{nullptr};

    QString _pendingLoginRequestId{};
    QString _pendingLoginEmail{};

    QHash<QString, std::shared_ptr<PendingUpload>> _uploads{};
};


#endif //MVIDEOCLIENT_AUTHSERVER_H

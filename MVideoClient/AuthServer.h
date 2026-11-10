//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_AUTHSERVER_H
#define MVIDEOCLIENT_AUTHSERVER_H
#include <QObject>
#include <qqmlintegration.h>
#include <QVariantList>
#include <QDateTime>

#include "HttpMgr.h"
#include "AuthTypes.h"
#include "WorksModel.h"

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
class QTimer;

class AuthServer : public QObject{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(WorksModel* works READ works CONSTANT)

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
    Q_INVOKABLE void checkStatus();

    [[nodiscard]] WorksModel* works() const { return _worksModel; }

signals:
    void verifyCodeFinished(int error, const QString& message);
    void registerFinished(int error, const QString& message, qint64 userId);
    void resetPasswordFinished(int error, const QString& message);
    void loginFinished(int error, const QString& message);
    void uploadFailed(unsigned long long, int error, const QString& message);
    void uploadSuccess(unsigned long long, int error, const QString& message);
    void serverCheckComplete(qint64 videoId, bool ok);
    void addSendQueueComplete(ResponseVideoData data,bool success, const QString& message);
    void queueChanged(int pending, int active);

private slots:
    void onVerifyCodeParsed(int error, const QString& message);
    void onRegisterParsed(int error, const QString& message, qint64 userId);
    void onResetParsed(int error, const QString& message);
    void onLoginParsed(int error, const QString& message, const LoginResult& result);
    void onHomeUpLoadParsed(int error, const QString& message, ResponseVideoData);
    void slotUploadConfirm(const ResponseVideoData& data);
    void onVideoStatus(qint64 videoId, int status);
    void onStatusPollFinished();


private:
    HttpMgr _httpMgr;
    QThread* _authThread{nullptr};
    AuthWork* _authWork{nullptr};

    QString _pendingLoginRequestId{};
    QString _pendingLoginEmail{};

    QHash<QString, std::shared_ptr<PendingUpload>> _uploads{};

    void appendWork(const QString& requestId, const QString& title,
                    const QString& description, const QStringList& tags);
    void bindWorkVideoId(const QString& requestId, qint64 videoId);
    void markWorkTranscoding(qint64 videoId);
    void markWorkDone(qint64 videoId);
    void markWorkFailed(qint64 videoId);
    void markWorkFailedByRequest(const QString& requestId);

    WorksModel* _worksModel{nullptr};

    void updateStatusPolling();

    QTimer* _checkStatusTimer{nullptr};
    bool _statusPollInFlight{false};
    QDateTime _statusPollDeadline{};
};


#endif //MVIDEOCLIENT_AUTHSERVER_H

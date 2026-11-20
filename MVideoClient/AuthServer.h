//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_AUTHSERVER_H
#define MVIDEOCLIENT_AUTHSERVER_H

#include <QObject>
#include <qqmlintegration.h>
#include <QString>
#include <QVariantMap>

#include <memory>

#include "AuthTypes.h"
#include "HttpMgr.h"
#include "WorksModel.h"

class AccountController;
class AvatarController;
class VideoUploadController;
class VideoStatusPoller;
class AuthWork;
class QThread;
class VideoPlaybackController;
class AuthServer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(WorksModel* works READ works CONSTANT)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)

public:
    AuthServer();
    ~AuthServer() override;

    Q_INVOKABLE void requestVarityCode(const QString& email);
    Q_INVOKABLE void requestRegister(const QString& account, const QString& password,
                                     const QString& username, const QString& email,
                                     const QString& phone, const QString& code);
    Q_INVOKABLE void requestReset(const QString& email, const QString& code,
                                  const QString& password, const QString& confirm);
    Q_INVOKABLE void requestLogin(const QString& email, const QString& password);
    Q_INVOKABLE void logout();

    Q_INVOKABLE void uploadAvatar(const QString& fileUrl, qreal scaleX, qreal scaleY,
                                  qreal offsetX, qreal offsetY);

    Q_INVOKABLE void uploadVideo(const QString& fileUrl, const QString& title,
                                 const QString& description, const QStringList& tags);

    Q_INVOKABLE void playVideo(qint64 videoId);

    [[nodiscard]] WorksModel* works() const { return _worksModel; }
    [[nodiscard]] QString avatarUrl() const;

signals:
    void verifyCodeFinished(int error, const QString& message);
    void registerFinished(int error, const QString& message, qint64 userId);
    void resetPasswordFinished(int error, const QString& message);
    void loginFinished(int error, const QString& message);
    void avatarUrlChanged();
    void avatarUploadFinished(bool ok, const QString& message);
    void uploadFailed(unsigned long long videoId, int error, const QString& message);
    void uploadSuccess(unsigned long long videoId, int error, const QString& message);
    void serverCheckComplete(qint64 videoId, bool ok);
    void addSendQueueComplete(ResponseVideoData data, bool success, const QString& message);
    void queueChanged(int pending, int active);
    void uploadProgress(unsigned long long videoId, int percent);
    void playReady(qint64 videoId, const QVariantMap& info);
    void playFailed(qint64 videoId, const QString& message);

private:
    void startAuthThread();
    void stopAuthThread();
    void wireControllers();

    HttpMgr _httpMgr;
    QThread* _authThread{nullptr};
    AuthWork* _authWork{nullptr};
    WorksModel* _worksModel{nullptr};

    std::unique_ptr<AccountController> _account;
    std::unique_ptr<AvatarController> _avatar;
    std::unique_ptr<VideoUploadController> _videoUpload;
    std::unique_ptr<VideoStatusPoller> _statusPoller;
    std::unique_ptr<VideoPlaybackController> _videoPlaybackController;
};

#endif //MVIDEOCLIENT_AUTHSERVER_H
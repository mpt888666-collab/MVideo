//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_AVATARCONTROLLER_H
#define MVIDEOCLIENT_AVATARCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>

#include "AuthTypes.h"

class HttpMgr;
class AuthWork;

class AvatarController : public QObject {
    Q_OBJECT

public:
    AvatarController(HttpMgr& http, AuthWork* authWork, QObject* parent = nullptr);

    void loadForLogin(const LoginResult& result);
    void startUpload(const QString& fileUrl, qreal scaleX, qreal scaleY,
                     qreal offsetX, qreal offsetY);
    void cancelPending();

    [[nodiscard]] QString avatarUrl() const { return _avatarUrl; }
    [[nodiscard]] bool uploading() const { return _uploadInFlight; }

signals:
    void avatarUrlChanged();
    void uploadFinished(bool ok, const QString& message);
    void prepareAvatarRequested(const QString& fileUrl, qreal scaleX, qreal scaleY,
                                qreal offsetX, qreal offsetY);

private:
    void onPrepared(const QByteArray& bytes, int error, const QString& message);
    void onTicket(int error, const QString& objectKey, const QString& uploadUrl,
                  const QString& message);
    void onBytesUploaded(bool ok, int status, const QString& error);
    void onComplete(int error, const QString& avatarKey, const QString& avatarUrl,
                    const QString& message);
    void onDownloaded(qint64 userId, bool ok, const QString& savePath, const QString& error);

    void failUpload(const QString& message);
    void clearUploadState();
    void startDownload(const QString& avatarUrl, const QString& avatarPath, qint64 userId);

    HttpMgr& _http;

    QString _avatarUrl;
    QString _avatarPendingPath;

    QByteArray _pendingBytes;
    QString _pendingRequestId;
    QString _pendingObjectKey;
    bool _uploadInFlight{false};
};

#endif //MVIDEOCLIENT_AVATARCONTROLLER_H
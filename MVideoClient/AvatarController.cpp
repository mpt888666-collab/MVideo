//
// Created by mpt on 2026/11/18.
//

#include "AvatarController.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QCryptographicHash>

#include "ApiRoutes.h"
#include "AuthWork.h"
#include "HttpMgr.h"
#include "RequestId.h"
#include "UserInfo.h"

namespace {

QString AvatarCachePath(qint64 uid, const QString& key) {
    if (uid <= 0 || key.isEmpty()) {
        return {};
    }

    const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1)
                    .toHex().left(16));

    QString suffix = QFileInfo(key).suffix().toLower();
    if (suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg") &&
        suffix != QStringLiteral("png") && suffix != QStringLiteral("webp")) {
        suffix = QStringLiteral("jpg");
    }

    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/avatars/")
           + QString::number(uid) + QLatin1Char('_')
           + hash + QLatin1Char('.') + suffix;
}

}

AvatarController::AvatarController(HttpMgr& http, AuthWork* authWork, QObject* parent)
    : QObject(parent), _http(http) {
    connect(this, &AvatarController::prepareAvatarRequested,
            authWork, &AuthWork::prepareAvatar, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigAvatarPrepared,
            this, &AvatarController::onPrepared, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigAvatarTicket,
            this, &AvatarController::onTicket, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigAvatarComplete,
            this, &AvatarController::onComplete, Qt::QueuedConnection);
    connect(&_http, &HttpMgr::sigAvatarBytesUploaded,
            this, &AvatarController::onBytesUploaded);
    connect(&_http, &HttpMgr::sigAvatarDownloaded,
            this, &AvatarController::onDownloaded);
}

void AvatarController::loadForLogin(const LoginResult& result) {
    _avatarUrl.clear();

    if (!result.headPath.isEmpty()) {
        const QString avatarPath = AvatarCachePath(result.userId, result.headPath);
        const QFileInfo avatarInfo(avatarPath);
        if (avatarInfo.exists() && avatarInfo.size() > 0) {
            _avatarUrl = QUrl::fromLocalFile(avatarPath).toString();
        } else if (!result.avatarUrl.isEmpty()) {
            startDownload(result.avatarUrl, avatarPath, result.userId);
        }
    }

    emit avatarUrlChanged();
}

void AvatarController::startUpload(const QString& fileUrl, qreal scaleX, qreal scaleY,
                                   qreal offsetX, qreal offsetY) {
    if (_uploadInFlight) {
        emit uploadFinished(false, QStringLiteral("头像正在上传，请稍候"));
        return;
    }
    if (!UserInfo::GetInstance()->loggedIn()) {
        emit uploadFinished(false, QStringLiteral("请先登录"));
        return;
    }
    if (fileUrl.isEmpty()) {
        emit uploadFinished(false, QStringLiteral("头像文件为空"));
        return;
    }

    _uploadInFlight = true;
    emit prepareAvatarRequested(fileUrl, scaleX, scaleY, offsetX, offsetY);
}

void AvatarController::onPrepared(const QByteArray& bytes, int error, const QString& message) {
    if (!_uploadInFlight) {
        return;
    }
    if (error != ErrorCodes::Success || bytes.isEmpty()) {
        failUpload(message.isEmpty() ? QStringLiteral("头像处理失败") : message);
        return;
    }

    _pendingBytes = bytes;
    _pendingRequestId = MakeRequestId();
    _pendingObjectKey.clear();

    QJsonObject json;
    json["uid"] = UserInfo::GetInstance()->userId();
    json["token"] = UserInfo::GetInstance()->token();
    json["request_id"] = _pendingRequestId;
    json["file_size"] = _pendingBytes.size();
    json["content_type"] = QStringLiteral("image/jpeg");

    _http.PostRequest(api::AvatarUpload(), json, ReqId::ID_AVATAR_UPLOAD, Modules::HOMEMOD);
}

void AvatarController::onTicket(int error, const QString& objectKey, const QString& uploadUrl,
                                const QString& message) {
    if (!_uploadInFlight) {
        return;
    }
    if (error != ErrorCodes::Success || objectKey.isEmpty() || uploadUrl.isEmpty()) {
        failUpload(message.isEmpty() ? QStringLiteral("获取头像上传地址失败") : message);
        return;
    }

    _pendingObjectKey = objectKey;
    _http.UploadAvatarBytes(QUrl(uploadUrl), _pendingBytes);
}

void AvatarController::onBytesUploaded(bool ok, int status, const QString& error) {
    if (!_uploadInFlight) {
        return;
    }
    if (!ok) {
        failUpload(QStringLiteral("头像上传到 MinIO 失败 (HTTP %1) %2").arg(status).arg(error));
        return;
    }

    QJsonObject json;
    json["uid"] = UserInfo::GetInstance()->userId();
    json["token"] = UserInfo::GetInstance()->token();
    json["object_key"] = _pendingObjectKey;

    _http.PostRequest(api::AvatarComplete(), json, ReqId::ID_AVATAR_COMPLETE, Modules::HOMEMOD);
}

void AvatarController::onComplete(int error, const QString& avatarKey, const QString& avatarUrl,
                                  const QString& message) {
    Q_UNUSED(avatarUrl)
    if (!_uploadInFlight) {
        return;
    }
    if (error != ErrorCodes::Success) {
        failUpload(message.isEmpty() ? QStringLiteral("头像保存失败") : message);
        return;
    }

    const QString key = avatarKey.isEmpty() ? _pendingObjectKey : avatarKey;
    const QString path = AvatarCachePath(UserInfo::GetInstance()->userId(), key);
    if (key.isEmpty() || path.isEmpty() || _pendingBytes.isEmpty()) {
        failUpload(QStringLiteral("头像信息不完整"));
        return;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(_pendingBytes) != _pendingBytes.size() ||
        !file.commit()) {
        failUpload(QStringLiteral("头像缓存写入失败"));
        return;
    }

    UserInfo::GetInstance()->setHeadPath(key);
    _avatarUrl = QUrl::fromLocalFile(path).toString();
    clearUploadState();

    emit avatarUrlChanged();
    emit uploadFinished(true, QStringLiteral("头像已更新"));
}

void AvatarController::onDownloaded(qint64 userId, bool ok, const QString& savePath,
                                    const QString& error) {
    if (savePath != _avatarPendingPath) {
        return;
    }
    _avatarPendingPath.clear();
    if (!ok) {
        qWarning() << "avatar download failed:" << error;
        return;
    }
    if (UserInfo::GetInstance()->userId() != userId) {
        return;
    }
    _avatarUrl = QUrl::fromLocalFile(savePath).toString();
    emit avatarUrlChanged();
}

void AvatarController::failUpload(const QString& message) {
    clearUploadState();
    emit uploadFinished(false, message);
}

void AvatarController::clearUploadState() {
    _uploadInFlight = false;
    _pendingBytes.clear();
    _pendingRequestId.clear();
    _pendingObjectKey.clear();
}

void AvatarController::startDownload(const QString& avatarUrl, const QString& avatarPath,
                                     qint64 userId) {
    if (avatarUrl.isEmpty() || avatarPath.isEmpty()) {
        return;
    }
    _avatarPendingPath = avatarPath;
    _http.DownloadAvatar(QUrl(avatarUrl), avatarPath, userId);
}

void AvatarController::cancelPending() {
    _avatarPendingPath.clear();
    _http.CancelAvatarDownload();
    clearUploadState();

    if (!_avatarUrl.isEmpty()) {
        _avatarUrl.clear();
        emit avatarUrlChanged();
    }
}
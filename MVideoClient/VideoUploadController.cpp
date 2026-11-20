//
// Created by mpt on 2026/11/18.
//

#include "VideoUploadController.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include "ApiRoutes.h"
#include "AuthWork.h"
#include "HttpMgr.h"
#include "RequestId.h"
#include "UploadWork.h"
#include "UserInfo.h"
#include "WorksModel.h"

// 作品状态取值：0 上传中 1 等待服务器校验 2 解码中 3 已发布 4 失败
namespace {
constexpr int kStateUploaded = 1;
constexpr int kStateTranscoding = 2;
constexpr int kStateFailed = 4;
}

VideoUploadController::VideoUploadController(HttpMgr& http, AuthWork* authWork, WorksModel* works,
                                             QObject* parent)
    : QObject(parent), _http(http), _works(works) {
    connect(authWork, &AuthWork::sigHomeUpLoadParsed,
            this, &VideoUploadController::onTicketParsed, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigServerCheckComplete,
            this, &VideoUploadController::onServerChecked, Qt::QueuedConnection);

    auto uploadMgr = UploadMgr::GetInstance().get();
    connect(uploadMgr, &UploadMgr::sigUploadDone,
            this, &VideoUploadController::confirmUpload);
    connect(uploadMgr, &UploadMgr::sigUploadSuccess,
            this, &VideoUploadController::onMinioUploaded);
    connect(uploadMgr, &UploadMgr::sigUploadFailed,
            this, &VideoUploadController::onMinioUploadFailed);
    connect(uploadMgr, &UploadMgr::sigAddSendQueueComplete,
            this, &VideoUploadController::addSendQueueComplete);
    connect(uploadMgr, &UploadMgr::sigQueueChanged,
            this, &VideoUploadController::queueChanged);
    connect(uploadMgr, &UploadMgr::sigUploadProgress,
            this, &VideoUploadController::onUploadProgress);
}

void VideoUploadController::uploadVideo(const QString& fileUrl, const QString& title,
                                        const QString& description, const QStringList& tags) {
    qDebug() << "upload video local Url" << fileUrl << " title:" << title
             << " description:" << description;

    const QString filePath = QUrl(fileUrl).toLocalFile();
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        qWarning() << "uploadVideo: file not found" << filePath;
        return;
    }

    QJsonObject json;
    const QString requestId = MakeRequestId();
    json["uid"] = UserInfo::GetInstance()->userId();
    json["request_id"] = requestId;
    json["title"] = title;
    json["description"] = description;
    json["file_name"] = info.fileName();
    json["file_size"] = static_cast<double>(info.size());
    json["token"] = UserInfo::GetInstance()->token();
    QJsonArray tagArray;
    for (const auto& tag : tags) {
        tagArray.append(QJsonValue(tag));
    }
    json["tags"] = tagArray;

    auto pendingUpload = std::make_shared<PendingUpload>();
    pendingUpload->requestId = requestId;
    pendingUpload->title = title;
    pendingUpload->localPath = filePath;
    pendingUpload->tags = tags;
    _uploads.insert(requestId, pendingUpload);

    appendWork(requestId, title, description, tags);
    _http.PostRequest(api::VideoUpload(), json, ReqId::ID_UPLOAD_VIDEO, Modules::HOMEMOD);
}

void VideoUploadController::onTicketParsed(int error, const QString& message,
                                           ResponseVideoData data) {
    qDebug() << "[upload]" << "error" << error << "msg" << message
             << "videoId" << data.videoId << "requestId" << data.requestId;

    if (error != ErrorCodes::Success) {
        markWorkFailedByRequest(data.requestId);
        emit uploadFailed(data.videoId, error, message);
        return;
    }

    const auto it = _uploads.find(data.requestId);
    if (it == _uploads.end()) {
        emit uploadFailed(data.videoId, -1, QStringLiteral("找不到对应的上传任务"));
        return;
    }

    data.localPath = (*it)->localPath;
    bindWorkVideoId(data.requestId, static_cast<qint64>(data.videoId));
    UploadMgr::GetInstance()->submit(std::move(data));
    _uploads.erase(it);
}

void VideoUploadController::confirmUpload(const ResponseVideoData& data) {
    qDebug() << "[upload]" << "confirm videoId" << data.videoId << "requestId" << data.requestId;

    QJsonObject json;
    json["uid"] = UserInfo::GetInstance()->userId();
    json["request_id"] = data.requestId;
    json["video_id"] = static_cast<qint64>(data.videoId);
    json["object_key"] = data.objectKey;
    _http.PostRequest(api::VideoUploadComplete(), json,
                      ReqId::ID_UPLOAD_SERVER_CHECK, Modules::HOMEMOD);
}

void VideoUploadController::onUploadProgress(unsigned long long videoId, int percent) {
    _works->setUploadProgress(static_cast<qint64>(videoId), percent);
    emit uploadProgress(videoId, percent);
}

void VideoUploadController::onMinioUploaded(unsigned long long videoId, int error,
                                            const QString& message) {
    // 结束事件不一定正好是 100%，这里补一下，避免进度环停在 99%。
    _works->setUploadProgress(static_cast<qint64>(videoId), 100);
    markWorkState(static_cast<qint64>(videoId), kStateUploaded);
    emit uploadSuccess(videoId, error, message);
}

void VideoUploadController::onMinioUploadFailed(unsigned long long videoId, int error,
                                                const QString& message) {
    markWorkState(static_cast<qint64>(videoId), kStateFailed);
    emit uploadFailed(videoId, error, message);
}

void VideoUploadController::onServerChecked(qint64 videoId, bool ok) {
    if (ok) {
        markWorkState(videoId, kStateTranscoding);
    } else {
        const qint64 target = videoId > 0 ? videoId : _works->latestPendingVideoId();
        markWorkState(target, kStateFailed);
    }
    emit serverCheckComplete(videoId, ok);
}

void VideoUploadController::appendWork(const QString& requestId, const QString& title,
                                       const QString& description, const QStringList& tags) {
    WorkItem item;
    item.requestId = requestId;
    item.title = title.isEmpty() ? QStringLiteral("未命名作品") : title;
    item.description = description;
    item.tags = tags;
    item.state = 0;
    _works->append(item);
}

void VideoUploadController::bindWorkVideoId(const QString& requestId, qint64 videoId) {
    _works->setVideoId(requestId, videoId);
}

void VideoUploadController::markWorkState(qint64 videoId, int state) {
    _works->setState(videoId, state);
    emit workStateChanged();
}

void VideoUploadController::markWorkFailedByRequest(const QString& requestId) {
    _works->setStateByRequestId(requestId, kStateFailed);
    emit workStateChanged();
}
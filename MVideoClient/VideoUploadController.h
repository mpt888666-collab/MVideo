//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_VIDEOUPLOADCONTROLLER_H
#define MVIDEOCLIENT_VIDEOUPLOADCONTROLLER_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>

#include "AuthTypes.h"
#include "const.h"

class HttpMgr;
class AuthWork;
class WorksModel;

struct PendingUpload {
    QString requestId;
    QString localPath;
    QString title;
    QStringList tags;
    QString objectKey;
    QString uploadUrl;
    qint64 videoId{0};
    qint64 createdAt{0};
    int percent{0};
    State state{State::TicketRequested};
};

class VideoUploadController : public QObject {
    Q_OBJECT

public:
    VideoUploadController(HttpMgr& http, AuthWork* authWork, WorksModel* works,
                          QObject* parent = nullptr);

    void uploadVideo(const QString& fileUrl, const QString& title,
                     const QString& description, const QStringList& tags);

signals:
    void uploadFailed(unsigned long long videoId, int error, const QString& message);
    void uploadSuccess(unsigned long long videoId, int error, const QString& message);
    void serverCheckComplete(qint64 videoId, bool ok);
    void addSendQueueComplete(ResponseVideoData data, bool success, const QString& message);
    void queueChanged(int pending, int active);
    void workStateChanged();
    void uploadProgress(unsigned long long videoId, int percent);

private:
    void onTicketParsed(int error, const QString& message, ResponseVideoData data);
    void onServerChecked(qint64 videoId, bool ok);
    void confirmUpload(const ResponseVideoData& data);
    void onMinioUploaded(unsigned long long videoId, int error, const QString& message);
    void onMinioUploadFailed(unsigned long long videoId, int error, const QString& message);
    void onUploadProgress(unsigned long long videoId, int percent);

    void appendWork(const QString& requestId, const QString& title,
                    const QString& description, const QStringList& tags);
    void bindWorkVideoId(const QString& requestId, qint64 videoId);
    void markWorkState(qint64 videoId, int state);
    void markWorkFailedByRequest(const QString& requestId);

    HttpMgr& _http;
    WorksModel* _works{nullptr};
    QHash<QString, std::shared_ptr<PendingUpload>> _uploads;
};

#endif //MVIDEOCLIENT_VIDEOUPLOADCONTROLLER_H
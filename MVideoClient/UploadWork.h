//
// Created by mpt on 2026/10/29.
//

#ifndef MVIDEOCLIENT_UPLOAD_H
#define MVIDEOCLIENT_UPLOAD_H
#include <QObject>
#include <QFile>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QSet>
#include <memory>

#include "Singleton.h"
#include "AuthTypes.h"

class QNetworkReply;
class UploadWork;
class UploadThread {
public:
    UploadThread(const UploadThread&) = delete;
    UploadThread& operator=(const UploadThread&) = delete;

    UploadThread();
    ~UploadThread();

    UploadWork *work() {
        return _uploadWork;
    }
private:
    QThread* _uploadThread{};
    UploadWork* _uploadWork{};

};

class UploadMgr final : public QObject, public Singleton<UploadMgr>, public std::enable_shared_from_this<UploadMgr>{
    Q_OBJECT
    friend class Singleton<UploadMgr>;
signals:
    void sigUploadFailed(unsigned long long videoId, int error, QString message);
    void sigUploadSuccess(unsigned long long videoId, int error, QString message);
    void sigUploadDone(ResponseVideoData data);
    void sigAddSendQueue(ResponseVideoData data);
    void sigAddSendQueueComplete(ResponseVideoData data, bool success, const QString& message);
    void sigQueueChanged(int pending, int active);
public:
    void submit(ResponseVideoData data);
private:
    UploadMgr();

    UploadThread _work_thread;
};

class UploadWork : public QObject{
    Q_OBJECT
    constexpr static int kMaxUploadCount = 4;
    constexpr static int kStallTimeoutMs = 60000;
public slots:
    void slotAddSendQueue(ResponseVideoData task);
    void slotPump();
    void slotShutdown();
signals:
    void sigUploadFailed(unsigned long long videoId, int error, QString message);
    void sigUploadSuccess(unsigned long long videoId, int error, QString message);
    void sigUploadDone(ResponseVideoData data);
    void sigQueueChanged(int pending, int active);
    void sigAddSendQueueComplete(ResponseVideoData data, bool success, const QString& message);
public:
    UploadWork();
private:
    bool beginUploadVideo(ResponseVideoData task);
    void notifyQueueChanged();

    QNetworkAccessManager *_manager{nullptr};
    bool _cancelled{false};
    QQueue<ResponseVideoData> _sendQueue;
    QSet<QNetworkReply*> _activeReplies;
    int _activeCount{0};
};


#endif //MVIDEOCLIENT_UPLOAD_H
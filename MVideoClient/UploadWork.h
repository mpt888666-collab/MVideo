//
// Created by mpt on 2026/10/29.
//

#ifndef MVIDEOCLIENT_UPLOAD_H
#define MVIDEOCLIENT_UPLOAD_H
#include <QObject>
#include "Singleton.h"
#include <memory>
#include <mutex>
#include <QFile>
#include <QNetworkAccessManager>
#include <QQueue>

#include "AuthTypes.h"
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
    void sigBeginUploadVideo(ResponseVideoData data);
    void sigUploadFailed(int error, QString message);
    void sigUploadSuccess(int error, QString message);
    void sigUploadDone(ResponseVideoData data);
public:
    void submit(ResponseVideoData data);
private:
    UploadMgr();

    UploadThread _work_thread;
};

class UploadWork : public QObject{
    Q_OBJECT
public slots:
    void slotBeginUploadVideo(ResponseVideoData task);

signals:
    void sigUploadFailed(int error, QString message);
    void sigUploadSuccess(int error, QString message);
    void sigUploadDone(ResponseVideoData data);
public:
    UploadWork();
private:
    QNetworkAccessManager *_manager{nullptr};
    QFile *_file{nullptr};
    QNetworkReply* _reply{nullptr};
    bool _cancelled{false};
};


#endif //MVIDEOCLIENT_UPLOAD_H
//
// Created by mpt on 2026/10/29.
//

#include "UploadWork.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>

#include "const.h"

UploadThread::UploadThread() : _uploadThread(new QThread()), _uploadWork(new UploadWork()){
    _uploadThread->setObjectName("uploadThread");
    _uploadWork->moveToThread(_uploadThread);
    QObject::connect(_uploadThread, &QThread::finished, _uploadWork, &QObject::deleteLater);
    _uploadThread->start();
}

UploadThread::~UploadThread() {
    if (!_uploadThread) return;
    _uploadThread->quit();
    _uploadThread->wait();
    delete _uploadThread;
    _uploadWork = nullptr;
}

void UploadWork::slotBeginUploadVideo(ResponseVideoData task) {
    if (_reply) { emit sigUploadFailed(-1, "已有上传在进行"); return; }
    qDebug() << "localPath : " << task.localPath << " uploadUrl: " << task.uploadUrl << " objectkey : " << task.objectKey << " " << task.videoId;
    _file = new QFile(task.localPath, this);
    if (!_file->open(QIODevice::ReadOnly)) {
        emit sigUploadFailed(NotFoundFile, "打不开本地文件");
        _file->deleteLater(); _file = nullptr;
        return;
    }

    QNetworkRequest req{ QUrl(task.uploadUrl) };
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(0);

    _reply = _manager->put(req, _file);

    // connect(_reply, &QNetworkReply::uploadProgress, this,
    //         [this](qint64 sent, qint64 total) { emit sigUploadProgress(sent, total);});

    connect(_reply, &QNetworkReply::finished, this, [this, task] {
        auto* reply = _reply;
        _reply = nullptr;
        _file->close();
        _file->deleteLater();
        _file = nullptr;

        const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (_cancelled) { reply->deleteLater(); return; }
        if (reply->error() != QNetworkReply::NoError || status / 100 != 2) {
            emit sigUploadFailed(status, reply->errorString());
        } else {
            emit sigUploadSuccess(Success, "传输minio成功");
            emit sigUploadDone(task);
        }
        reply->deleteLater();
    });
}
UploadWork::UploadWork() : _manager(new QNetworkAccessManager(this)){

}

UploadMgr::UploadMgr() {
    connect(this, &UploadMgr::sigBeginUploadVideo, _work_thread.work(), &UploadWork::slotBeginUploadVideo);
    connect(_work_thread.work(), &UploadWork::sigUploadFailed, this, &UploadMgr::sigUploadFailed);
    connect(_work_thread.work(), &UploadWork::sigUploadSuccess, this, &UploadMgr::sigUploadSuccess);
    connect(_work_thread.work(), &UploadWork::sigUploadDone, this, &UploadMgr::sigUploadDone);
}

void UploadMgr::submit(ResponseVideoData data) {
    emit sigBeginUploadVideo(std::move(data));
}





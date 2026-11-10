//
// Created by mpt on 2026/10/29.
//

#include "UploadWork.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

#include "const.h"

UploadThread::UploadThread() : _uploadThread(new QThread()), _uploadWork(new UploadWork()){
    _uploadThread->setObjectName("uploadThread");
    _uploadWork->moveToThread(_uploadThread);
    QObject::connect(_uploadThread, &QThread::finished, _uploadWork, &QObject::deleteLater);
    _uploadThread->start();
}

UploadThread::~UploadThread() {
    if (!_uploadThread) return;

    if (_uploadWork && _uploadThread->isRunning()) {
        QMetaObject::invokeMethod(_uploadWork, "slotShutdown", Qt::BlockingQueuedConnection);
    }

    _uploadThread->quit();
    if (!_uploadThread->wait(3000)) {
        qWarning() << "uploadThread: 3 秒内未退出，继续等待收尾";
        _uploadThread->wait();
    }
    delete _uploadThread;
    _uploadThread = nullptr;
    _uploadWork = nullptr;
}

bool UploadWork::beginUploadVideo(ResponseVideoData task) {
    qDebug() << "localPath : " << task.localPath << " uploadUrl: " << task.uploadUrl << " objectkey : " << task.objectKey << " " << task.videoId;
    auto file = new QFile(task.localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit sigUploadFailed(task.videoId, NotFoundFile, "打不开本地文件");
        file->deleteLater();
        return false;
    }

    QNetworkRequest req{ QUrl(task.uploadUrl) };
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(0);

    auto reply = _manager->put(req, file);
    ++_activeCount;
    _activeReplies.insert(reply);
    notifyQueueChanged();

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(kStallTimeoutMs);

    auto stalled = std::make_shared<bool>(false);

    connect(reply, &QNetworkReply::uploadProgress, this, [timer](qint64, qint64) {
        timer->start();
    });
    connect(timer, &QTimer::timeout, this, [reply, stalled]() {
        *stalled = true;
        reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::finished, this, [this, file, task, reply, timer, stalled] {
        timer->stop();
        timer->deleteLater();
        _activeReplies.remove(reply);

        file->close();
        file->deleteLater();

        const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool timedOut = *stalled;
        const QString message = timedOut
                ? QStringLiteral("上传超时：%1 秒没有进度").arg(kStallTimeoutMs / 1000)
                : reply->errorString();
        const bool ok = !timedOut
                && reply->error() == QNetworkReply::NoError
                && status / 100 == 2;

        --_activeCount;

        if (!_cancelled) {
            if (ok) {
                emit sigUploadSuccess(task.videoId, Success, "传输minio成功");
                emit sigUploadDone(task);
            } else {
                emit sigUploadFailed(task.videoId, timedOut ? ErrorNetWork : status, message);
            }
        }

        reply->deleteLater();
        notifyQueueChanged();
    });
    return true;
}

UploadWork::UploadWork() : _manager(new QNetworkAccessManager(this)){
    connect(this, &UploadWork::sigQueueChanged, this, &UploadWork::slotPump);
}

UploadMgr::UploadMgr() {
    connect(this, &UploadMgr::sigAddSendQueue, _work_thread.work(), &UploadWork::slotAddSendQueue);
    connect(_work_thread.work(), &UploadWork::sigUploadFailed, this, &UploadMgr::sigUploadFailed);
    connect(_work_thread.work(), &UploadWork::sigUploadSuccess, this, &UploadMgr::sigUploadSuccess);
    connect(_work_thread.work(), &UploadWork::sigUploadDone, this, &UploadMgr::sigUploadDone);
    connect(_work_thread.work(), &UploadWork::sigAddSendQueueComplete, this, &UploadMgr::sigAddSendQueueComplete);
    connect(_work_thread.work(), &UploadWork::sigQueueChanged, this, &UploadMgr::sigQueueChanged);
}

void UploadMgr::submit(ResponseVideoData data) {
    emit sigAddSendQueue(std::move(data));
}

void UploadWork::notifyQueueChanged() {
    emit sigQueueChanged(static_cast<int>(_sendQueue.size()), _activeCount);
}

void UploadWork::slotAddSendQueue(ResponseVideoData task) {
    if (_cancelled) {
        emit sigAddSendQueueComplete(task, false, "加入上传失败");
        return;
    }
    _sendQueue.append(task);
    emit sigAddSendQueueComplete(task, true, "加入上传成功");
    notifyQueueChanged();
}

void UploadWork::slotPump() {
    if (_cancelled) return;
    while (_activeCount < kMaxUploadCount && !_sendQueue.isEmpty()) {
        ResponseVideoData task = _sendQueue.takeFirst();
        if (!beginUploadVideo(std::move(task))) continue;
    }
}

void UploadWork::slotShutdown() {
    _cancelled = true;
    _sendQueue.clear();

    const auto replies = _activeReplies;
    for (auto *reply : replies) {
        reply->abort();
    }
    notifyQueueChanged();
}
//
// Created by mpt on 2026/10/24.
//

#include "HttpMgr.h"
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QNetworkRequest>

HttpThread::HttpThread() : _http_thread(new QThread), _worker(new HttpWork){
    _http_thread->setObjectName("httpWorkThread");

    _worker->moveToThread(_http_thread);
    QObject::connect(_http_thread, &QThread::finished, _worker, &QObject::deleteLater);
    _http_thread->start();
}

HttpThread::~HttpThread()
{
    if (!_http_thread)
        return;

    _http_thread->quit();
    _http_thread->wait();
    delete _http_thread;
    _http_thread = nullptr;
    _worker = nullptr;
}

HttpWork::HttpWork(QObject* parent) : QObject(parent) {}

void HttpWork::PostRequestWork(const QUrl& url, const QJsonObject& json, ReqId req_id, Modules mod) {
    auto content = QJsonDocument(json).toJson();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(content.length()));

    auto reply = manager()->post(request, content);

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        if(!reply->isFinished()){
            reply->abort();
        }
    });

    timer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, req_id, mod] {
        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit sigHttpReply(req_id, mod, httpStatus, static_cast<int>(reply->error()), QString::fromUtf8(body));
        reply->deleteLater();
    });
}

HttpMgr::HttpMgr(QObject* parent) : QObject(parent) {
    qRegisterMetaType<ReqId>("ReqId");
    qRegisterMetaType<Modules>("Modules");

    connect(this, &HttpMgr::sigPostToWorker, _httpThread.worker(), &HttpWork::PostRequestWork);
    connect(_httpThread.worker(), &HttpWork::sigHttpReply, this, &HttpMgr::sigHttpReply);
    connect(this, &HttpMgr::sigDownloadAvatarToWorker, _httpThread.worker(), &HttpWork::DownloadAvatarWork);
    connect(this, &HttpMgr::sigCancelAvatarDownloadToWorker, _httpThread.worker(), &HttpWork::CancelAvatarDownloadWork);
    connect(_httpThread.worker(), &HttpWork::sigAvatarDownloaded, this, &HttpMgr::sigAvatarDownloaded);
    connect(this, &HttpMgr::sigUploadAvatarBytesToWorker, _httpThread.worker(), &HttpWork::UploadAvatarBytesWork);
    connect(_httpThread.worker(), &HttpWork::sigAvatarBytesUploaded, this, &HttpMgr::sigAvatarBytesUploaded);
}

void HttpMgr::PostRequest(const QUrl& url, const QJsonObject &json, const ReqId reqId, const Modules mod)
{
    emit sigPostToWorker(url, json, reqId, mod);
}

void HttpMgr::DownloadAvatar(const QUrl& url, const QString& savePath, qint64 userId)
{
    emit sigDownloadAvatarToWorker(url, savePath, userId);
}

void HttpMgr::CancelAvatarDownload()
{
    emit sigCancelAvatarDownloadToWorker();
}

void HttpWork::DownloadAvatarWork(const QUrl& url, const QString& savePath, qint64 userId)
{
    if (_avatarReply) {
        _avatarReply->abort();
        _avatarReply->deleteLater();
        _avatarReply = nullptr;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    _avatarReply = manager()->get(request);
    connect(_avatarReply, &QNetworkReply::finished, this, [this, savePath, userId]() {
        auto* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) {
            return;
        }
        if (reply != _avatarReply) {
            reply->deleteLater();
            return;
        }
        _avatarReply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        QString error;
        bool ok = reply->error() == QNetworkReply::NoError && status / 100 == 2 && !data.isEmpty();

        if (!ok) {
            error = reply->errorString();
        } else {
            QDir().mkpath(QFileInfo(savePath).absolutePath());
            QSaveFile file(savePath);
            if (!file.open(QIODevice::WriteOnly) ||
                file.write(data) != data.size() ||
                !file.commit()) {
                error = QStringLiteral("avatar cache write failed");
                ok = false;
            }
        }

        reply->deleteLater();
        emit sigAvatarDownloaded(userId, ok, savePath, error);
    });
}

void HttpWork::CancelAvatarDownloadWork()
{
    if (!_avatarReply) {
        return;
    }
    _avatarReply->abort();
    _avatarReply->deleteLater();
    _avatarReply = nullptr;
}

void HttpMgr::UploadAvatarBytes(const QUrl& url, const QByteArray& data)
{
    emit sigUploadAvatarBytesToWorker(url, data);
}

void HttpWork::UploadAvatarBytesWork(const QUrl& url, const QByteArray& data)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "image/jpeg");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = manager()->put(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && status / 100 == 2;
        const QString error = ok ? QString() : reply->errorString();
        reply->readAll();
        reply->deleteLater();
        emit sigAvatarBytesUploaded(ok, status, error);
    });
}


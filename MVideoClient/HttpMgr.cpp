//
// Created by mpt on 2026/10/24.
//

#include "HttpMgr.h"
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

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
            reply->abort();  //终止网络请求，设置错误码 `OperationCanceledError`，发射 `finished` 信号
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
}

void HttpMgr::PostRequest(const QUrl& url, const QJsonObject &json, const ReqId reqId, const Modules mod)
{
    emit sigPostToWorker(url, json, reqId, mod);
}





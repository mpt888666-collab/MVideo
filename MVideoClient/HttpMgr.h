//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_HTTPMGR_H
#define MVIDEOCLIENT_HTTPMGR_H
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QThread>
#include <QUrl>
#include "const.h"

class HttpWork;
class HttpThread {
public:
    HttpThread();
    ~HttpThread();
    HttpThread(HttpThread const&) = delete;
    HttpThread& operator=(HttpThread const&) = delete;
    [[nodiscard]] HttpWork* worker() const { return _worker; }

private:
    QThread* _http_thread;
    HttpWork* _worker = nullptr;
};

class HttpMgr : public QObject{
    Q_OBJECT

public:
    explicit HttpMgr(QObject* parent = nullptr);
    void PostRequest(const QUrl& url, const QJsonObject &json,ReqId reqId,Modules mod);

signals:
    void sigPostToWorker(const QUrl& url, const QJsonObject& json,ReqId reqId,Modules mod);
    void sigHttpReply(ReqId id, Modules mod, int httpStatus, int networkError,
                      const QString& body);
private:
    HttpThread _httpThread;
};

class HttpWork : public QObject {
    Q_OBJECT
public:
    explicit HttpWork(QObject* parent = nullptr);

public slots:
    void PostRequestWork(const QUrl& url, const QJsonObject& json,
                         ReqId reqId, Modules mod);

signals:
    void sigHttpReply(ReqId id, Modules mod, int httpStatus, int networkError,
                      const QString& body);

private:
    QNetworkAccessManager* _manager{nullptr};

    QNetworkAccessManager *manager(){
        if (!_manager)
            _manager = new QNetworkAccessManager(this);

        return _manager;
    }
};



#endif //MVIDEOCLIENT_HTTPMGR_H

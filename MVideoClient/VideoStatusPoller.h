//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_VIDEOSTATUSPOLLER_H
#define MVIDEOCLIENT_VIDEOSTATUSPOLLER_H

#include <QObject>
#include <QDateTime>

class HttpMgr;
class AuthWork;
class WorksModel;
class QTimer;

class VideoStatusPoller : public QObject {
    Q_OBJECT

public:
    VideoStatusPoller(HttpMgr& http, AuthWork* authWork, WorksModel* works,
                      QObject* parent = nullptr);

    void refresh();
    void reset();

private:
    void checkStatus();
    void onVideoStatus(qint64 videoId, int status, const QString& coverUrl);
    void onPollFinished();

    HttpMgr& _http;
    WorksModel* _works{nullptr};
    QTimer* _timer{nullptr};
    bool _inFlight{false};
    QDateTime _deadline;
};

#endif //MVIDEOCLIENT_VIDEOSTATUSPOLLER_H
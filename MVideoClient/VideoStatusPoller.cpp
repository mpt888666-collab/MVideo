//
// Created by mpt on 2026/11/18.
//

#include "VideoStatusPoller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include "ApiRoutes.h"
#include "AuthWork.h"
#include "HttpMgr.h"
#include "UserInfo.h"
#include "WorksModel.h"
#include "const.h"

namespace {
constexpr int kStateTranscoding = 2;
constexpr int kStateDone = 3;
constexpr int kStateFailed = 4;
}

VideoStatusPoller::VideoStatusPoller(HttpMgr& http, AuthWork* authWork, WorksModel* works,
                                     QObject* parent)
    : QObject(parent), _http(http), _works(works) {
    _timer = new QTimer(this);
    _timer->setInterval(kVideoStatusPollMs);
    connect(_timer, &QTimer::timeout, this, &VideoStatusPoller::checkStatus);

    connect(authWork, &AuthWork::sigVideoStatus,
            this, &VideoStatusPoller::onVideoStatus, Qt::QueuedConnection);
    connect(authWork, &AuthWork::sigVideoStatusDone,
            this, &VideoStatusPoller::onPollFinished, Qt::QueuedConnection);
}

void VideoStatusPoller::checkStatus() {
    if (_inFlight) {
        return;
    }
    if (_deadline.isValid() && QDateTime::currentDateTime() > _deadline) {
        qWarning() << "video status polling timed out";
        _timer->stop();
        return;
    }

    const QList<qint64> ids = _works->videoIdsWithState(kStateTranscoding);
    if (ids.isEmpty()) {
        refresh();
        return;
    }

    QJsonArray array;
    for (const auto id : ids) {
        array.append(id);
    }

    QJsonObject json;
    json["user_id"] = UserInfo::GetInstance()->userId();
    json["token"] = UserInfo::GetInstance()->token();
    json["video_ids"] = array;

    _inFlight = true;
    _http.PostRequest(api::VideoStatus(), json, ReqId::ID_VIDEO_STATUS, Modules::HOMEMOD);
}

void VideoStatusPoller::onVideoStatus(qint64 videoId, int status, const QString& coverUrl) {
    if (status == kStateDone) {
        _works->setState(videoId, kStateDone, coverUrl);
    } else if (status == kStateFailed) {
        _works->setState(videoId, kStateFailed);
    }
    refresh();
}

void VideoStatusPoller::onPollFinished() {
    _inFlight = false;
    refresh();
}

void VideoStatusPoller::refresh() {
    const bool hasPending = !_works->videoIdsWithState(kStateTranscoding).isEmpty();
    if (!hasPending) {
        if (_timer->isActive()) {
            _timer->stop();
        }
        _inFlight = false;
        _deadline = QDateTime();
        return;
    }
    if (!_timer->isActive()) {
        _deadline = QDateTime::currentDateTime().addSecs(kVideoStatusPollMaxSec);
        _timer->start();
    }
}

void VideoStatusPoller::reset() {
    _timer->stop();
    _inFlight = false;
    _deadline = QDateTime();
    // 停表后可能还挂着一个在途请求，让它的回包被忽略即可。
}
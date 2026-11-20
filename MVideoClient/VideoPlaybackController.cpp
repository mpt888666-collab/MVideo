//
// Created by mpt on 2026/11/19.
//

#include "VideoPlaybackController.h"

#include <QTimer>
#include <QUrl>

#include "ApiRoutes.h"
#include "AuthWork.h"
#include "HttpMgr.h"
#include "UserInfo.h"
#include "const.h"

namespace {


constexpr int kPlayRequestTimeoutMs = 15'000;
constexpr int kPlayUrlDefaultTtlSec = 900;
constexpr int kPlayUrlRefreshMarginSec = 60;
constexpr int kPlayCacheMaxEntries = 64;

}

VideoPlaybackController::VideoPlaybackController(HttpMgr& http, AuthWork* authWork, QObject* parent)
    : QObject(parent), _http(http) {
    _timeout = new QTimer(this);
    _timeout->setSingleShot(true);
    connect(_timeout, &QTimer::timeout, this, &VideoPlaybackController::onTimeout);

    if (authWork) {
        connect(authWork, &AuthWork::sigplayVideoInfoComplete,
                this, &VideoPlaybackController::onVideoDetail, Qt::QueuedConnection);
    }
}

VideoPlaybackController::~VideoPlaybackController() = default;

void VideoPlaybackController::requestPlay(qint64 videoId) {
    if (videoId <= 0) {
        emit playFailed(videoId, tr("无效的视频编号"));
        return;
    }

    const auto cached = _cache.constFind(videoId);
    if (cached != _cache.cend() && isFresh(*cached)) {
        emit playReady(videoId, toVariantMap(videoId, *cached));
        return;
    }

    if (_inFlight) {
        _queuedId = videoId;
        return;
    }

    sendPending(videoId);
}

void VideoPlaybackController::clear() {
    _cache.clear();
    _queuedId = 0;
    _pendingId = 0;
    _inFlight = false;
    _timeout->stop();
}

void VideoPlaybackController::sendPending(qint64 videoId) {
    const auto user = UserInfo::GetInstance();
    if (!user || user->userId() <= 0 || user->token().isEmpty()) {
        finishFailure(videoId, tr("登录状态已失效，请重新登录"));
        return;
    }

    QJsonObject json;
    json[QStringLiteral("user_id")] = user->userId();
    json[QStringLiteral("token")] = user->token();
    json[QStringLiteral("video_id")] = videoId;

    _pendingId = videoId;
    _inFlight = true;
    _timeout->start(kPlayRequestTimeoutMs);


    _http.PostRequest(api::PlayVideo(), json, ID_VIDEO_DETAIL, Modules::HOMEMOD);
}

void VideoPlaybackController::onVideoDetail(const PlayInfo& info) {
    if (!_inFlight || _pendingId <= 0) {
        return;
    }

    _timeout->stop();
    const qint64 pendingId = _pendingId;
    _inFlight = false;
    _pendingId = 0;

    if (info.videoId > 0 && info.videoId != pendingId) {
        emit playFailed(pendingId, tr("服务器返回的视频与请求不一致"));
        takeQueued();
        return;
    }
    const qint64 videoId = info.videoId > 0 ? info.videoId : pendingId;

    if (info.masterUrl.isEmpty()) {
        const QString reason = info.description.isEmpty()
                ? tr("视频还在处理中，暂时无法播放")
                : info.description;
        emit playFailed(videoId, reason);
        takeQueued();
        return;
    }

    PlayInfo cached = info;
    if (!cached.expiresAt.isValid()) {
        const qint64 usable = qMax<qint64>(60, kPlayUrlDefaultTtlSec - kPlayUrlRefreshMarginSec);
        cached.expiresAt = QDateTime::currentDateTimeUtc().addSecs(usable);
    }

    for (auto it = _cache.begin(); it != _cache.end();) {
        if (!isFresh(*it)) {
            it = _cache.erase(it);
        } else {
            ++it;
        }
    }
    if (_cache.size() >= kPlayCacheMaxEntries && !_cache.contains(videoId)) {
        _cache.erase(_cache.begin());
    }
    _cache.insert(videoId, cached);

    emit playReady(videoId, toVariantMap(videoId, cached));
    takeQueued();
}

void VideoPlaybackController::onTimeout() {
    if (!_inFlight) {
        return;
    }

    const qint64 videoId = _pendingId;
    _pendingId = 0;
    _inFlight = false;
    emit playFailed(videoId, tr("获取播放地址超时，请稍后重试"));
    takeQueued();
}

void VideoPlaybackController::finishFailure(qint64 videoId, const QString& message) {
    if (videoId > 0) {
        emit playFailed(videoId, message);
    }
    _pendingId = 0;
    _inFlight = false;
    _timeout->stop();
    takeQueued();
}

void VideoPlaybackController::takeQueued() {
    if (_inFlight) {
        return;
    }
    const qint64 next = _queuedId;
    _queuedId = 0;
    if (next > 0) {
        sendPending(next);
    }
}

bool VideoPlaybackController::isFresh(const PlayInfo& info) {
    return !info.masterUrl.isEmpty() && info.expiresAt.isValid() &&
           info.expiresAt > QDateTime::currentDateTimeUtc();
}

QVariantMap VideoPlaybackController::toVariantMap(qint64 videoId, const PlayInfo& info) {
    QVariantMap map;
    map.insert(QStringLiteral("videoId"), videoId);
    map.insert(QStringLiteral("title"), info.title);
    map.insert(QStringLiteral("description"), info.description);
    map.insert(QStringLiteral("coverUrl"), info.coverUrl);
    map.insert(QStringLiteral("masterUrl"), info.masterUrl);
    map.insert(QStringLiteral("playCount"), info.playCount);
    map.insert(QStringLiteral("likeCount"), info.likeCount);
    map.insert(QStringLiteral("commentCount"), info.commentCount);
    map.insert(QStringLiteral("favoriteCount"), info.favoriteCount);
    map.insert(QStringLiteral("expiresAt"), info.expiresAt.toString(Qt::ISODate));
    return map;
}
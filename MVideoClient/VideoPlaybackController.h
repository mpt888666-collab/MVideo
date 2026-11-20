//
// Created by mpt on 2026/11/19.
//

#ifndef MVIDEOCLIENT_VIDEOPLAYBACKCONTROLLER_H
#define MVIDEOCLIENT_VIDEOPLAYBACKCONTROLLER_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "AuthTypes.h"

class HttpMgr;
class AuthWork;
class QTimer;

class VideoPlaybackController : public QObject {
    Q_OBJECT
public:
    explicit VideoPlaybackController(HttpMgr& http, AuthWork* authWork, QObject* parent = nullptr);
    ~VideoPlaybackController() override;

    Q_INVOKABLE void requestPlay(qint64 videoId);
    void clear();

signals:
    void playReady(qint64 videoId, const QVariantMap& info);
    void playFailed(qint64 videoId, const QString& message);

private slots:
    void onVideoDetail(const PlayInfo& info);
    void onTimeout();

private:
    [[nodiscard]] static bool isFresh(const PlayInfo& info);
    [[nodiscard]] static QVariantMap toVariantMap(qint64 videoId, const PlayInfo& info);

    void sendPending(qint64 videoId);
    void finishFailure(qint64 videoId, const QString& message);
    void takeQueued();

    QHash<qint64, PlayInfo> _cache;
    HttpMgr& _http;
    QTimer* _timeout{nullptr};
    qint64 _pendingId{0};
    qint64 _queuedId{0};
    bool _inFlight{false};
};

#endif //MVIDEOCLIENT_VIDEOPLAYBACKCONTROLLER_H
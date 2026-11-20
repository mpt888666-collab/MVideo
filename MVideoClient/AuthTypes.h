#ifndef MVIDEOCLIENT_AUTHTYPES_H
#define MVIDEOCLIENT_AUTHTYPES_H

#include <QMetaType>
#include <QString>
#include <QDateTime>
struct LoginResult {
    qint64 userId{0};
    QString account;
    QString userName;
    QString email;
    QString phone;
    QString token;
    QString host;
    int port{0};
    qint64 follow{0};
    qint64 fan{0};
    QString headPath;
    QString avatarUrl;
};

struct ResponseVideoData {
    unsigned long long videoId{0};
    QString objectKey;
    QString uploadUrl;
    QString requestId;
    QString message;
    QString localPath;
    int expiresIn{0};
};

struct PlayInfo {
    qint64 videoId{0};
    QString title;
    QString description;
    QString coverUrl;
    QString masterUrl;
    QDateTime expiresAt;
    qint64 likeCount{};
    qint64 favoriteCount{};
    qint64 playCount{};
    qint64 commentCount{};
};

Q_DECLARE_METATYPE(LoginResult)
Q_DECLARE_METATYPE(ResponseVideoData)
Q_DECLARE_METATYPE(PlayInfo)

#endif //MVIDEOCLIENT_AUTHTYPES_H

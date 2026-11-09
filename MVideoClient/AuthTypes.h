#ifndef MVIDEOCLIENT_AUTHTYPES_H
#define MVIDEOCLIENT_AUTHTYPES_H

#include <QMetaType>
#include <QString>

struct LoginResult {
    qint64 userId{0};
    QString account;
    QString userName;
    QString email;
    QString phone;
    QString token;
    QString host;
    int port{0};
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

Q_DECLARE_METATYPE(LoginResult)
Q_DECLARE_METATYPE(ResponseVideoData)

#endif //MVIDEOCLIENT_AUTHTYPES_H

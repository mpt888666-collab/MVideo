//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_AUTHWORK_H
#define MVIDEOCLIENT_AUTHWORK_H

#include <QObject>
#include <QString>
#include "const.h"
#include "AuthTypes.h"

class AuthWork : public QObject {
    Q_OBJECT

public:
    explicit AuthWork(QObject* parent = nullptr);

public slots:
    void parseReply(ReqId id, Modules mod, int httpStatus, int networkError,
                    const QString& body);

signals:
    void sigVerifyCodeParsed(int error, const QString& message);
    void sigRegisterParsed(int error, const QString& message, qint64 userId);
    void sigResetParsed(int error, const QString& message);
    void sigLoginParsed(int error, const QString& message, const LoginResult& result);
    void sigHomeUpLoadParsed(int error, const QString& message, ResponseVideoData);
    void sigServerCheckComplete(qint64 videoId, bool ok);
    void sigVideoStatus(qint64 videoId, int status);
    void sigVideoStatusDone();

private:
    static QString defaultMessage(int error);

    void handleHomeMon(ReqId id, const QJsonObject& object);
};


#endif //MVIDEOCLIENT_AUTHWORK_H

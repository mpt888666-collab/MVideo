//
// Created by mpt on 2026/10/24.
//

#ifndef MVIDEOCLIENT_USERINFO_H
#define MVIDEOCLIENT_USERINFO_H

#include <QObject>
#include <QString>

#include "Singleton.h"

class UserInfo : public QObject, public Singleton<UserInfo> {
    Q_OBJECT
    Q_PROPERTY(qint64 userId READ userId NOTIFY changed)
    Q_PROPERTY(QString email READ email NOTIFY changed)
    Q_PROPERTY(QString account READ account NOTIFY changed)
    Q_PROPERTY(QString userName READ userName NOTIFY changed)
    Q_PROPERTY(QString phone READ phone NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(QString chatHost READ chatHost NOTIFY changed)
    Q_PROPERTY(int chatPort READ chatPort NOTIFY changed)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)

public:
    UserInfo();

    qint64 userId() const { return _userId; }
    QString email() const { return _email; }
    QString account() const { return _account; }
    QString userName() const { return _userName; }
    QString phone() const { return _phone; }
    QString displayName() const;
    QString chatHost() const { return _chatHost; }
    int chatPort() const { return _chatPort; }
    bool loggedIn() const { return _loggedIn; }
    QString token() const { return _token; }

    void applyLogin(qint64 userId, const QString& account, const QString& userName,
                    const QString& email, const QString& phone, const QString& token,
                    const QString& chatHost, int chatPort);
    void clear();

signals:
    void changed();

private:
    qint64 _userId{0};
    QString _email;
    QString _account;
    QString _userName;
    QString _phone;
    QString _token;
    QString _chatHost;
    int _chatPort{0};
    bool _loggedIn{false};
};


#endif //MVIDEOCLIENT_USERINFO_H

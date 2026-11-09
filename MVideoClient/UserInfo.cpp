//
// Created by mpt on 2026/10/24.
//

#include "UserInfo.h"

#include <QDebug>

UserInfo::UserInfo() = default;

QString UserInfo::displayName() const {
    if (!_userName.isEmpty())
        return _userName;
    if (!_account.isEmpty())
        return _account;
    return _email;
}

void UserInfo::applyLogin(qint64 userId, const QString& account, const QString& userName,
                          const QString& email, const QString& phone, const QString& token,
                          const QString& chatHost, int chatPort) {
    _userId = userId;
    _account = account;
    _userName = userName;
    _email = email;
    _phone = phone;
    _token = token;
    _chatHost = chatHost;
    _chatPort = chatPort;
    _loggedIn = _userId > 0;

    qDebug() << "UserInfo: uid =" << _userId << ", account =" << _account
             << ", username =" << _userName << ", email =" << _email << ", phone =" << _phone
             << ", chat =" << _chatHost << ":" << _chatPort
             << ", token length =" << _token.size();
    emit changed();
}

void UserInfo::clear() {
    _userId = 0;
    _email.clear();
    _account.clear();
    _userName.clear();
    _phone.clear();
    _token.clear();
    _chatHost.clear();
    _chatPort = 0;
    _loggedIn = false;
    emit changed();
}

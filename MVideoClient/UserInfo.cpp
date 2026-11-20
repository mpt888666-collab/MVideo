//
// Created by mpt on 2026/10/24.
//

#include "UserInfo.h"

#include <QDebug>

#include "AuthTypes.h"

UserInfo::UserInfo() = default;

QString UserInfo::displayName() const {
    if (!_userName.isEmpty())
        return _userName;
    if (!_account.isEmpty())
        return _account;
    return _email;
}

void UserInfo::applyLogin(const LoginResult& result) {
    _userId = result.userId;
    _account = result.account;
    _userName = result.userName;
    _email = result.email;
    _phone = result.phone;
    _token = result.token;
    _chatHost = result.host;
    _chatPort = result.port;
    _follow = result.follow;
    _fan = result.fan;
    _loggedIn = _userId > 0;
    _head_path = result.headPath;

    qDebug() << "UserInfo: uid =" << _userId << ", account =" << _account
             << ", username =" << _userName << ", email =" << _email << ", phone =" << _phone
             << ", chat =" << _chatHost << ":" << _chatPort
             << ", follow =" << _follow << ", fans =" << _fan
             << ", token length =" << _token.size();
    emit changed();
}

void UserInfo::setHeadPath(const QString& headPath) {
    if (_head_path == headPath) {
        return;
    }
    _head_path = headPath;
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
    _follow = 0;
    _fan = 0;
    _loggedIn = false;
    _head_path.clear();
    emit changed();
}

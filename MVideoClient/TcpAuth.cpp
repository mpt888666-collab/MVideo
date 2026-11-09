//
// Created by mpt on 2026/10/25.
//

#include "TcpAuth.h"
#include "UserInfo.h"
#include "const.h"
#include <QDebug>
#include <iostream>
#include <QJsonObject>
TcpAuth::TcpAuth() {
    _si = std::make_shared<ServerInfo>();
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sigSocketConnected, this, [this](bool success) {
        if (success) {
            qDebug() << "tcp socket connected, sending login id";
            sendLoginId();
        } else {
            emit loginFailed(QStringLiteral("无法连接聊天服务器"));
        }
    });
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sigLoginComplete, this, [this](bool success) {
        if (success) {
            qDebug() << "chat server login succeeded";
            emit loginSucceeded();
        } else {
            qDebug() << "chat server login failed";
            emit loginFailed(QStringLiteral("聊天服务器登录失败"));
        }
    });
}

void TcpAuth::loginChatServer() {
    const auto user = UserInfo::GetInstance();
    const QString host = user->chatHost();
    const int port = user->chatPort();
    if (host.isEmpty() || port <= 0) {
        emit loginFailed(QStringLiteral("聊天服务器地址为空"));
        return;
    }
    _si->_uid = static_cast<unsigned long long>(user->userId());
    _si->_token = user->token();
    _si->_chat_host = host;
    _si->_chat_port = QString::number(port);
    TcpMgr::GetInstance()->login(_si);
}

void TcpAuth::sendLoginId() {
    QJsonObject data;
    data["uid"] = static_cast<qint64>(_si->_uid);
    data["token"] = _si->_token;
    QJsonDocument doc;
    doc.setObject(data);
    const QByteArray bytes = doc.toJson(QJsonDocument::Compact);
    TcpMgr::GetInstance()->sendData(ReqId::ID_CHAT_LOGIN_REQ, bytes);
}

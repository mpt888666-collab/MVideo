//
// Created by mpt on 2026/10/25.
//

#ifndef MVIDEOCLIENT_TCPAUTH_H
#define MVIDEOCLIENT_TCPAUTH_H
#include <qqmlintegration.h>

#include "TcpMgr.h"
class TcpAuth : public QObject{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    TcpAuth();
    Q_INVOKABLE void loginChatServer();
    Q_INVOKABLE void sendLoginId();

signals:
    void loginSucceeded();
    void loginFailed(const QString& message);
private:
    std::shared_ptr<ServerInfo> _si{};
};


#endif //MVIDEOCLIENT_TCPAUTH_H

//
// Created by mpt on 2026/10/25.
//

#ifndef MVIDEOCLIENT_TCPMGR_H
#define MVIDEOCLIENT_TCPMGR_H
#include <QThread>
#include <QTcpSocket>
#include <QObject>
#include <memory>
#include "Singleton.h"
#include "const.h"
#include <QQueue>
class TcpWork;
class TcpWorkThread {
public:
    TcpWorkThread();
    ~TcpWorkThread();
    TcpWorkThread(const TcpWorkThread&) = delete;
    TcpWorkThread& operator=(const TcpWorkThread&) = delete;

    [[nodiscard]] TcpWork* GetWork() const {
        return _work;
    }

private:
    QThread* _tcpWorkerThread;
    TcpWork* _work{nullptr};

};

class TcpMgr : public QObject, public Singleton<TcpMgr>, public std::enable_shared_from_this<TcpMgr>{
    Q_OBJECT
    friend class Singleton<TcpMgr>;
public:
    void login(const std::shared_ptr<ServerInfo>& si);
    void sendData(ReqId reqId, const QByteArray& data);
signals:
    void sigLogin(std::shared_ptr<ServerInfo> si);
    void sigSendData(ReqId reqId, QByteArray data);
    void sigSocketConnected(bool success);
    void sigLoginComplete(bool success);

private:
    TcpMgr();

    TcpWorkThread _workThread;

};

class TcpWork : public QObject{
    Q_OBJECT
public:
    TcpWork();

public slots:
    void slotLogin(std::shared_ptr<ServerInfo> si);

    void slotSendData(ReqId reqId, const QByteArray& data);
signals:
    void sigConnectComplete(bool);
    void sigLoginComplete(bool);
private:
    QTcpSocket* socket();
    QTcpSocket* _socket{};
    QByteArray _buffer{};
    qint16 _msg_id{};
    qint16 _msg_len{};
    bool _b_head_pending{false};
    QMap<ReqId, std::function<void(ReqId, qint16, const QByteArray&)>> _handlers{};
    QString _host;
    uint16_t _port{0};
    QQueue<QByteArray> _sendQueue;
    qint64 _send_bytes{0};
    bool _pending{false};
    QByteArray _currentBlock{};
    void initHandlers();

    void handleMsg(ReqId id, qint16 len, const QByteArray &data);
};

#endif //MVIDEOCLIENT_TCPMGR_H

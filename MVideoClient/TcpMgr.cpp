//
// Created by mpt on 2026/10/25.
//

#include "TcpMgr.h"
#include <QDebug>
#include <QMetaType>
#include <QThread>
#include <QtEndian>
#include <QJsonObject>
TcpWorkThread::TcpWorkThread() : _tcpWorkerThread(new QThread()), _work(new TcpWork()){
    _tcpWorkerThread->setObjectName("TcpWorkThread");
    _work->moveToThread(_tcpWorkerThread);
    QObject::connect(_tcpWorkerThread, &QThread::finished,_work, &QObject::deleteLater);
    _tcpWorkerThread->start();
}

TcpWorkThread::~TcpWorkThread()
{
    if (!_tcpWorkerThread)
        return;

    _tcpWorkerThread->quit();
    _tcpWorkerThread->wait();
    delete _tcpWorkerThread;
    _tcpWorkerThread = nullptr;
    _work = nullptr;
}

TcpMgr::TcpMgr() {
    qRegisterMetaType<std::shared_ptr<ServerInfo>>("std::shared_ptr<ServerInfo>");

    TcpWork* work = _workThread.GetWork();
    connect(work, &TcpWork::sigConnectComplete, this, &TcpMgr::sigSocketConnected);
    connect(work, &TcpWork::sigLoginComplete, this, &TcpMgr::sigLoginComplete);
    connect(this, &TcpMgr::sigLogin, work, &TcpWork::slotLogin, Qt::QueuedConnection);
    connect(this, &TcpMgr::sigSendData, work, &TcpWork::slotSendData, Qt::QueuedConnection);
}

void TcpMgr::login(const std::shared_ptr<ServerInfo>& si)
{
    emit sigLogin(si);
}

void TcpMgr::sendData(ReqId reqId, const QByteArray& data)
{
    emit sigSendData(reqId, data);
}

TcpWork::TcpWork() {
    initHandlers();
}

QTcpSocket* TcpWork::socket(){
    if (!_socket) {
        _socket = new QTcpSocket(this);

        connect(_socket, &QTcpSocket::connected, this, [this]() {
            qDebug() << "Connected to server!";
            emit sigConnectComplete(true);
        });
        connect(_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            qDebug() << "Connect to server failed:" << _socket->errorString();
            emit sigConnectComplete(false);
        });

        connect(_socket, &QTcpSocket::readyRead, this, [this]() {
            _buffer.append(_socket->readAll());
            while (true) {
                if (!_b_head_pending) {
                    if (_buffer.size() < 4) return;
                    const auto data = _buffer.constData();
                    _msg_id = qFromBigEndian<qint16>(data);
                    _msg_len = qFromBigEndian<qint16>(data + 2);
                    _buffer.remove(0, 4);
                    qDebug() << "ID:" << _msg_id << "Len:" << _msg_len;
                }
                if (_buffer.size() < _msg_len) {
                    _b_head_pending = true;
                    return;
                }

                _b_head_pending = false;

                QByteArray messageBody = _buffer.left(_msg_len);
                _buffer.remove(0, _msg_len);

                handleMsg(static_cast<ReqId>(_msg_id), _msg_len, messageBody);
            }
        });

        connect(_socket, &QTcpSocket::bytesWritten, this, [this](const qint64 bytes) {
            _send_bytes += bytes;
            if (_send_bytes < _currentBlock.size()) {
                return;
            }
            if (!_sendQueue.isEmpty()) {
                _currentBlock = _sendQueue.dequeue();
                _send_bytes = 0;
                _socket->write(_currentBlock);
                return;
            }
            _pending = false;
            _currentBlock.clear();
            _send_bytes = 0;
        });
    }
    return _socket;
}

void TcpWork::slotLogin(std::shared_ptr<ServerInfo> si) {
    qDebug() << "Connecting to server...";
    _host = si->_chat_host;
    _port = si->_chat_port.toUInt();

    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->abort();
    }
    _buffer.clear();
    _b_head_pending = false;
    _sendQueue.clear();
    _currentBlock.clear();
    _send_bytes = 0;
    _pending = false;

    socket()->connectToHost(si->_chat_host, _port);
}

void TcpWork::handleMsg(const ReqId id, const qint16 len, const QByteArray &data) {
    qDebug() << "~~~~~~~~~~~~~~~~~~~~~~Received Id" << id << "Len:" << len;
    const auto find_iter =  _handlers.find(id);
    if(find_iter == _handlers.end()){
        qDebug()<< "not found id ["<< id << "] to handle";
        return ;
    }

    find_iter.value()(id,len,data);
}

void TcpWork::slotSendData(ReqId reqId, const QByteArray& data) {
    if (!_socket || _socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket is not connected. Cannot send data.";
        return;
    }
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    qint16 id = reqId;
    qint16 len = data.size();

    out << id;
    out << len;
    block.append(data);

    if (_pending) {
        _sendQueue.enqueue(block);
        return;
    }

    _pending = true;
    _currentBlock = block;
    _send_bytes = 0;
    _socket->write(_currentBlock);
}

void TcpWork::initHandlers() {
    _handlers.insert(ID_CHAT_LOGIN_RSP, [this](ReqId id, qint16 len, const QByteArray &data) {
        Q_UNUSED(id)
        Q_UNUSED(len)

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "chat login reply is not a json object:" << parseError.errorString();
            emit sigLoginComplete(false);
            return;
        }

        const int ec = doc.object().value("ec").toInt(-1);
        if (ec != ErrorCodes::Success) {
            qDebug() << "chat login rejected, ec =" << ec;
            emit sigLoginComplete(false);
            return;
        }

        qDebug() << "chat login accepted";
        emit sigLoginComplete(true);
    });
}

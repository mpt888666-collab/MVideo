//
// Created by mpt on 2026/10/24.
//

#include "AuthServer.h"
#include "AuthWork.h"

#include <QThread>

AuthServer::AuthServer() : _authThread(new QThread),_authWork(new AuthWork){
    _authThread->setObjectName(QStringLiteral("authParseThread"));
    _authWork->moveToThread(_authThread);

    connect(_authThread, &QThread::finished, _authWork, &QObject::deleteLater);
    connect(&_httpMgr, &HttpMgr::sigHttpReply, _authWork, &AuthWork::parseReply, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigVerifyCodeParsed, this, &AuthServer::onVerifyCodeParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigRegisterParsed, this, &AuthServer::onRegisterParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigResetParsed, this, &AuthServer::onResetParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigLoginParsed, this, &AuthServer::onLoginParsed, Qt::QueuedConnection);

    _authThread->start();
}

AuthServer::~AuthServer(){
    if (!_authThread)
        return;

    _authThread->quit();
    _authThread->wait();
    delete _authThread;
    _authThread = nullptr;
    _authWork = nullptr;
}

void AuthServer::requestVarityCode(const QString& email) {
    QJsonObject json;
    json["email"] = email;
    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/get_varifycode"), json, ReqId::ID_GET_VARIFY_CODE, Modules::VARITYMOD);
}

void AuthServer::requestRegister(const QString& account, const QString& password,
        const QString& username, const QString& email, const QString& phone, const QString& code) {
    QJsonObject json;
    json["account"] = account;
    json["password"] = password;
    json["username"] = username;
    json["email"] = email;
    json["phone"] = phone;
    json["code"] = code;
    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/user_register"), json, ReqId::ID_REG_USER, Modules::REGISTERMOD);
}

void AuthServer::requestReset(const QString& email, const QString& code, const QString& password, const QString& confirm) {
    QJsonObject json;
    json["email"] = email;
    json["code"] = code;
    json["password"] = password;
    json["confirm"] = confirm;
    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/reset_pwd"), json, ReqId::ID_RESET_PWD, Modules::RESETMOD);
}

void AuthServer::requestLogin(const QString& email, const QString& password, const QString& request_id) {
    QJsonObject json;
    json["email"] = email;
    json["password"] = password;
    json["request_id"] = request_id;
    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/logic_user"), json, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}

void AuthServer::onVerifyCodeParsed(int error, const QString& message){
    emit verifyCodeFinished(error, message);
}

void AuthServer::onRegisterParsed(int error, const QString& message, qint64 userId){
    emit registerFinished(error, message, userId);
}

void AuthServer::onResetParsed(int error, const QString& message){
    emit resetPasswordFinished(error, message);
}

void AuthServer::onLoginParsed(int error, const QString& message, const QString& token, const QString& host, int port, qint64 userId){
    emit loginFinished(error, message, token, host, port, userId);
}

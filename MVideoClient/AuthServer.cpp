//
// Created by mpt on 2026/10/24.
//

#include "AuthServer.h"

#include <qfileinfo.h>

#include "AuthWork.h"
#include "UserInfo.h"
#include "const.h"

#include <QThread>
#include <QUuid>
#include <QJsonArray>

#include "UploadWork.h"

namespace {
QString MakeRequestId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

AuthServer::AuthServer() : _authThread(new QThread),_authWork(new AuthWork){
    qRegisterMetaType<LoginResult>("LoginResult");

    _authThread->setObjectName(QStringLiteral("authParseThread"));
    _authWork->moveToThread(_authThread);

    connect(_authThread, &QThread::finished, _authWork, &QObject::deleteLater);
    connect(&_httpMgr, &HttpMgr::sigHttpReply, _authWork, &AuthWork::parseReply, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigVerifyCodeParsed, this, &AuthServer::onVerifyCodeParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigRegisterParsed, this, &AuthServer::onRegisterParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigResetParsed, this, &AuthServer::onResetParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigLoginParsed, this, &AuthServer::onLoginParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigHomeUpLoadParsed, this, &AuthServer::onHomeUpLoadParsed, Qt::QueuedConnection);
    connect(_authWork, &AuthWork::sigServerCheckComplete, this, &AuthServer::serverCheckComplete, Qt::QueuedConnection);
    connect(UploadMgr::GetInstance().get(), &UploadMgr::sigUploadFailed, this, &AuthServer::uploadFailed);
    connect(UploadMgr::GetInstance().get(), &UploadMgr::sigUploadSuccess, this, &AuthServer::uploadSuccess);
    connect(UploadMgr::GetInstance().get(), &UploadMgr::sigUploadDone, this, &AuthServer::slotUploadConfirm);

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

void AuthServer::requestLogin(const QString& email, const QString& password) {
    if (!_pendingLoginRequestId.isEmpty())
        return;

    _pendingLoginRequestId = MakeRequestId();
    _pendingLoginEmail = email;

    QJsonObject json;
    json["email"] = email;
    json["password"] = password;
    json["request_id"] = _pendingLoginRequestId;
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

void AuthServer::onLoginParsed(int error, const QString& message, const LoginResult& result){
    const QString resolvedEmail = result.email.isEmpty() ? _pendingLoginEmail : result.email;
    if (error == ErrorCodes::Success) {
        UserInfo::GetInstance()->applyLogin(
                result.userId, result.account, result.userName, resolvedEmail,
                result.phone, result.token, result.host, result.port);
    }
    _pendingLoginRequestId.clear();
    _pendingLoginEmail.clear();

    emit loginFinished(error, message);
}

void AuthServer::logout() {
    _pendingLoginRequestId.clear();
    _pendingLoginEmail.clear();
    UserInfo::GetInstance()->clear();
}

void AuthServer::uploadVideo(const QString& fileUrl, const QString& title, const QString& description, const QStringList& tags) {
    qDebug() << "upload video local Url" << fileUrl << " title:" << title << " description:" << description;
    auto filePath = QUrl(fileUrl).toLocalFile();
    auto info = QFileInfo(filePath);
    if (!info.exists() || !info.isFile()) {
        qWarning() << "uploadVideo: file not found" << filePath;
        return;
    }

    QJsonObject json;
    const auto request_id = MakeRequestId();
    json["uid"] = UserInfo::GetInstance()->userId();
    json["request_id"] = request_id;
    json["title"] = title;
    json["description"] = description;
    json["file_name"] = info.fileName();
    json["file_size"] = static_cast<double>(info.size());
    json["token"] = UserInfo::GetInstance()->token();
    QJsonArray tagArray;
    for (auto& tag : tags) {
        tagArray.append(QJsonValue(tag));
    }
    json["tags"] = tagArray;

    auto pendingUpload = std::make_shared<PendingUpload>();
    pendingUpload->requestId = request_id;
    pendingUpload->title = title;
    pendingUpload->localPath = filePath;
    pendingUpload->tags = tags;
    _uploads.insert(request_id, pendingUpload);

    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/video_upload"), json, ReqId::ID_UPLOAD_VIDEO, Modules::HOMEMOD);

}

void AuthServer::onHomeUpLoadParsed(int error, const QString& message, ResponseVideoData data) {
    if (error != 0) { emit uploadFailed(error, message); return;}

    const auto it = _uploads.find(data.requestId);
    if (it == _uploads.end()) { emit uploadFailed(-1, "找不到对应的上传任务"); return; }

    data.localPath = (*it)->localPath;
    UploadMgr::GetInstance()->submit(std::move(data));
    _uploads.erase(it);
}

void AuthServer::slotUploadConfirm(const ResponseVideoData& data) {
    QJsonObject json;
    json["uid"] = UserInfo::GetInstance()->userId();
    json["request_id"] = data.requestId;
    json["video_id"] = static_cast<qint64>(data.videoId);
    json["object_key"] = data.objectKey;
    _httpMgr.PostRequest(QUrl("http://127.0.0.1:8080/video_upload_complete"), json, ReqId::ID_UPLOAD_SERVER_CHECK, Modules::HOMEMOD);
}

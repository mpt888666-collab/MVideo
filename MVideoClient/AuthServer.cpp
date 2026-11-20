//
// Created by mpt on 2026/10/24.
//

#include "AuthServer.h"

#include <QThread>

#include "AccountController.h"
#include "AuthWork.h"
#include "AvatarController.h"
#include "UserInfo.h"
#include "VideoPlaybackController.h"
#include "VideoStatusPoller.h"
#include "VideoUploadController.h"

AuthServer::AuthServer()
    : _authThread(new QThread), _authWork(new AuthWork), _worksModel(new WorksModel(this)) {
    qRegisterMetaType<LoginResult>("LoginResult");

    startAuthThread();
    wireControllers();
}

AuthServer::~AuthServer() {
    _statusPoller.reset();
    _videoUpload.reset();
    _avatar.reset();
    _account.reset();

    stopAuthThread();
}

void AuthServer::startAuthThread() {
    _authThread->setObjectName(QStringLiteral("authParseThread"));
    _authWork->moveToThread(_authThread);
    connect(_authThread, &QThread::finished, _authWork, &QObject::deleteLater);
    connect(&_httpMgr, &HttpMgr::sigHttpReply,
            _authWork, &AuthWork::parseReply, Qt::QueuedConnection);
    _authThread->start();
}

void AuthServer::stopAuthThread() {
    if (!_authThread) {
        return;
    }
    _authThread->quit();
    _authThread->wait();
    delete _authThread;
    _authThread = nullptr;
    _authWork = nullptr;
}

void AuthServer::wireControllers() {
    _account = std::make_unique<AccountController>(_httpMgr, _authWork);
    _avatar = std::make_unique<AvatarController>(_httpMgr, _authWork);
    _videoUpload = std::make_unique<VideoUploadController>(_httpMgr, _authWork, _worksModel);
    _statusPoller = std::make_unique<VideoStatusPoller>(_httpMgr, _authWork, _worksModel);
    _videoPlaybackController = std::make_unique<VideoPlaybackController>(_httpMgr, _authWork);

    connect(_videoPlaybackController.get(), &VideoPlaybackController::playReady,
            this, &AuthServer::playReady);
    connect(_videoPlaybackController.get(), &VideoPlaybackController::playFailed,
            this, &AuthServer::playFailed);

    connect(_account.get(), &AccountController::verifyCodeFinished,
            this, &AuthServer::verifyCodeFinished);
    connect(_account.get(), &AccountController::registerFinished,
            this, &AuthServer::registerFinished);
    connect(_account.get(), &AccountController::resetPasswordFinished,
            this, &AuthServer::resetPasswordFinished);
    connect(_account.get(), &AccountController::loginFinished,
            this, &AuthServer::loginFinished);
    connect(_account.get(), &AccountController::loginSucceeded,
            _avatar.get(), &AvatarController::loadForLogin);
    // 登录失败时清掉上一位用户留下的头像，避免界面串号。
    connect(_account.get(), &AccountController::loginFinished, this,
            [this](int error, const QString&) {
                if (error != ErrorCodes::Success) {
                    _avatar->cancelPending();
                }
            });

    connect(_avatar.get(), &AvatarController::avatarUrlChanged,
            this, &AuthServer::avatarUrlChanged);
    connect(_avatar.get(), &AvatarController::uploadFinished,
            this, &AuthServer::avatarUploadFinished);

    connect(_videoUpload.get(), &VideoUploadController::uploadFailed,
            this, &AuthServer::uploadFailed);
    connect(_videoUpload.get(), &VideoUploadController::uploadSuccess,
            this, &AuthServer::uploadSuccess);
    connect(_videoUpload.get(), &VideoUploadController::serverCheckComplete,
            this, &AuthServer::serverCheckComplete);
    connect(_videoUpload.get(), &VideoUploadController::addSendQueueComplete,
            this, &AuthServer::addSendQueueComplete);
    connect(_videoUpload.get(), &VideoUploadController::queueChanged,
            this, &AuthServer::queueChanged);
    connect(_videoUpload.get(), &VideoUploadController::workStateChanged,
            _statusPoller.get(), &VideoStatusPoller::refresh);
    connect(_videoUpload.get(), &VideoUploadController::uploadProgress,
            this, &AuthServer::uploadProgress);
}

void AuthServer::requestVarityCode(const QString& email) {
    _account->requestVerifyCode(email);
}

void AuthServer::requestRegister(const QString& account, const QString& password,
                                 const QString& username, const QString& email,
                                 const QString& phone, const QString& code) {
    _account->requestRegister(account, password, username, email, phone, code);
}

void AuthServer::requestReset(const QString& email, const QString& code,
                              const QString& password, const QString& confirm) {
    _account->requestReset(email, code, password, confirm);
}

void AuthServer::requestLogin(const QString& email, const QString& password) {
    _account->requestLogin(email, password);
}

void AuthServer::uploadAvatar(const QString& fileUrl, qreal scaleX, qreal scaleY,
                              qreal offsetX, qreal offsetY) {
    _avatar->startUpload(fileUrl, scaleX, scaleY, offsetX, offsetY);
}

void AuthServer::uploadVideo(const QString& fileUrl, const QString& title,
                             const QString& description, const QStringList& tags) {
    _videoUpload->uploadVideo(fileUrl, title, description, tags);
}

void AuthServer::playVideo(qint64 videoId) {
    _videoPlaybackController->requestPlay(videoId);
}

QString AuthServer::avatarUrl() const {
    return _avatar ? _avatar->avatarUrl() : QString();
}

void AuthServer::logout() {
    _account->resetPending();
    _avatar->cancelPending();
    _statusPoller->reset();
    _videoPlaybackController->clear();
    UserInfo::GetInstance()->clear();
    _worksModel->clear();
}
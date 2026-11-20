//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_APIROUTES_H
#define MVIDEOCLIENT_APIROUTES_H

#include <QString>
#include <QUrl>

// 网关 HTTP 接口地址的唯一来源，新增接口只改这里，避免地址散落在各个 controller 中。
namespace api {

inline QString BaseUrl() {
    return QStringLiteral("http://127.0.0.1:8080");
}

inline QUrl Url(const QString& path) {
    return QUrl(BaseUrl() + path);
}

inline QUrl VerifyCode() { return Url(QStringLiteral("/get_varifycode")); }
inline QUrl Register() { return Url(QStringLiteral("/user_register")); }
inline QUrl ResetPassword() { return Url(QStringLiteral("/reset_pwd")); }
inline QUrl Login() { return Url(QStringLiteral("/logic_user")); }

inline QUrl AvatarUpload() { return Url(QStringLiteral("/avatar_upload")); }
inline QUrl AvatarComplete() { return Url(QStringLiteral("/avatar_complete")); }

inline QUrl VideoUpload() { return Url(QStringLiteral("/video_upload")); }
inline QUrl VideoUploadComplete() { return Url(QStringLiteral("/video_upload_complete")); }
inline QUrl VideoStatus() { return Url(QStringLiteral("/video_status")); }
inline QUrl PlayVideo() {return Url(QStringLiteral("/video_detail"));}
} // namespace api

#endif //MVIDEOCLIENT_APIROUTES_H
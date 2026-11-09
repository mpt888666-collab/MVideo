#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <qqml.h>
#include "HttpMgr.h"
#include "UserInfo.h"
#include <QThread>

extern void qml_register_types_MVideoClient();

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    qDebug() << "主线程id" << QThread::currentThreadId();

    QGuiApplication::setApplicationName(QStringLiteral("MVideoClient"));
    QGuiApplication::setOrganizationName(QStringLiteral("MVideo"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (qmlTypeId("MVideoClient", 1, 0, "AuthServer") < 0)
        qml_register_types_MVideoClient();
    qmlRegisterSingletonInstance("MVideoClient", 1, 0, "UserInfo",
                                 UserInfo::GetInstance().get());

    QQmlApplicationEngine engine;
    QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() { QCoreApplication::exit(-1); },
            Qt::QueuedConnection
    );

    engine.loadFromModule(QStringLiteral("MVideoClient"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return QGuiApplication::exec();
}

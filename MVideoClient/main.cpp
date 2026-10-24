#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "HttpMgr.h"
#include <QThread>
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    qDebug() << "主线程id" << QThread::currentThreadId();

    QGuiApplication::setApplicationName(QStringLiteral("MVideoClient"));
    QGuiApplication::setOrganizationName(QStringLiteral("MVideo"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

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

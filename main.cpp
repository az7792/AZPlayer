// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "controller/mediacontroller.h"
#include "renderer/videorenderer.h"

int main(int argc, char *argv[]) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL); // 绘图使用的FBO只有在OpenGL模式下才生效

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    MediaController mc;

    QQmlApplicationEngine engine;
    qmlRegisterType<VideoWindow>("VideoWindow", 1, 0, "VideoWindow");
    engine.rootContext()->setContextProperty("MediaCtrl", &mc);
    engine.rootContext()->setContextProperty("appDirPath", QCoreApplication::applicationDirPath());
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}

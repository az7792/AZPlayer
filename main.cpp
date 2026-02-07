// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "controller/mediacontroller.h"
#include "renderer/videorenderer.h"
#include "utils/filehelper.h"

int main(int argc, char *argv[]) {
    // 支持使用文件和文件夹作为启动参数
    QStringList startupFiles;
    for (int i = 1; i < argc; ++i)
        startupFiles << QString::fromLocal8Bit(argv[i]);
    QVariantList mediaFiles = FileHelper::instance().expandFiles(startupFiles);

    // for (const QVariant &item : list) {
    //     QVariantMap map = item.toMap();
    //     qDebug() << "text:" << map["text"].toString() << ", fileUrl:" << map["fileUrl"].toString();
    // }

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

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [mediaFiles](QObject *obj, const QUrl &) {
            if (!obj || mediaFiles.isEmpty())
                return;

            QMetaObject::invokeMethod(
                obj,
                "onStartupFiles",
                Qt::QueuedConnection,
                Q_ARG(QVariant, mediaFiles));
        });

    engine.load(url);
    return app.exec();
}

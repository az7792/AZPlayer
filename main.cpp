// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

// 针对 Windows 的高精度定时器支持
#ifdef Q_OS_WIN
#include <windows.h>
class TimerResolution {
public:
    TimerResolution() { timeBeginPeriod(1); }
    ~TimerResolution() { timeEndPeriod(1); }
};
#else
class TimerResolution {
public:
    TimerResolution() {}
    ~TimerResolution() {}
};
#endif

#include "controller/mediacontroller.h"
#include "renderer/videorenderer.h"
#include "stats/playbackstats.h"
#include "utils/filehelper.h"
#include "utils/powermanager.h"

int main(int argc, char *argv[]) {
    TimerResolution timerResolution{};

    // 支持使用文件和文件夹作为启动参数
    QStringList startupFiles;
    for (int i = 1; i < argc; ++i)
        startupFiles << QString::fromLocal8Bit(argv[i]);
    QVariantList mediaFiles = FileHelper::instance().expandFiles(startupFiles);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL); // 绘图使用的FBO只有在OpenGL模式下才生效

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    MediaController mc;
    QObject::connect(&mc, &MediaController::pausedChanged, &app, [&mc]() {
        bool shouldKeepAwake = !mc.paused();
        PowerManager::instance().setKeepScreenOn(shouldKeepAwake);
        qDebug() << "AZPlayer Power State Sync:" << (shouldKeepAwake ? "Awake" : "Allow Sleep");
    });

    QQmlApplicationEngine engine;
    qmlRegisterType<VideoWindow>("VideoWindow", 1, 0, "VideoWindow");
    engine.rootContext()->setContextProperty("MediaCtrl", &mc);    
    engine.rootContext()->setContextProperty("PlaybackStats", &PlaybackStats::instance());
    engine.rootContext()->setContextProperty("appDirPath", QCoreApplication::applicationDirPath());

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

    engine.loadFromModule("AZPlayer", "Main");
    return app.exec();
}

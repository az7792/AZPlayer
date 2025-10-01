#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <decode/decodevideo.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include <QQuickItem>
#include <QThread>

#include <QFile>
#include <QQuickWindow>
#include <QTimer>

#include "decode/decodeaudio.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/audioplayer.h"
#include "renderer/videoplayer.h"
#include "renderer/videorenderer.h"

#include "main.moc"
int main(int argc, char *argv[]) {
    // const AVPixFmtDescriptor *t;
    // const AVPixFmtDescriptor *t1;
    // t = av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P10LE);
    // t1 = av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P);
    // return 0;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    std::string URL = R"(D:\视频\[VCB-Studio] Karakai Jouzu no Takagi-san\[VCB-Studio] Karakai Jouzu no Takagi-san [Ma10p_1080p]\[VCB-Studio] Karakai Jouzu no Takagi-san [02][Ma10p_1080p][x265_flac].mkv)";
    // std::string URL = R"(D:\视频\[VCB-Studio] K-ON!\[VCB-Studio] K-ON!\[VCB-Studio] K-ON! The Movie [Ma10p_1080p]\[VCB-S&philosophy-raws][K-ON! The Movie][MOIVE][BDRIP][Hi10P FLAC][1920X1080].mkv)";
    //  std::string URL = R"(D:\视频\[SweetSub&VCB-Studio] Horimiya [Ma10p_1080p]\\[SweetSub&VCB-Studio] Horimiya [01][Ma10p_1080p][x265_flac].mkv)";
    //  std::string URL = R"(D:\视频\[Mabors&VCB-Studio] Shigatsu wa Kimi no Uso [Hi10p_1080p]\[Mabors&VCB-Studio] Shigatsu wa Kimi no Uso [01][Hi10p_1080p][x264_2flac].mkv)";

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    qmlRegisterType<VideoWindow>("VideoWindow", 1, 0, "VideoWindow");
    // AudioPlayer *audioPlayer = new AudioPlayer();
    // engine.rootContext()->setContextProperty("audioPlayer", audioPlayer);
    const QUrl url(QStringLiteral("qrc:/AZPlayer/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    QThread *threadDemux = new QThread;
    QThread *threadAudioDecode = new QThread;
    QThread *threadVideoDecode = new QThread;
    QThread *threadAudioPlayer = new QThread;
    QThread *threadVideoPlayer = new QThread;

    sharedPktQueue pktAudioBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue frmAudioBuf = std::make_shared<SPSCQueue<AVFrmItem>>(16);
    sharedPktQueue pktVideoBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue frmVideoBuf = std::make_shared<SPSCQueue<AVFrmItem>>(16);

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        qDebug() << "AudioPkt:" << pktAudioBuf->size() << "VideoPkt:" << pktVideoBuf->size();
        qDebug() << "AudioFrm:" << frmAudioBuf->size() << "VideoFrm:" << frmVideoBuf->size() << "\n";
    });
    timer.start(1000); // 每1000ms触发一次

    Demux *demux = new Demux;
    DecodeAudio *decodeAudio = new DecodeAudio;
    DecodeVideo *decodeVideo = new DecodeVideo;
    AudioPlayer *audioPlayer = new AudioPlayer;
    const QList<QObject *> rootObjects = engine.rootObjects();
    VideoWindow *videoWindow = nullptr;
    if (!rootObjects.isEmpty()) {
        videoWindow = rootObjects.first()->findChild<VideoWindow *>("videoWindow");
    }
    VideoPlayer *videoPlayer = new VideoPlayer(frmVideoBuf);
    QObject::connect(videoPlayer, &VideoPlayer::renderDataReady,
                     videoWindow, &VideoWindow::updateRenderData,
                     Qt::QueuedConnection);

    demux->init(URL.c_str(), pktAudioBuf, pktVideoBuf, nullptr);
    decodeAudio->init(demux->getAudioCodecID(), demux->getAudioCodecpar(), demux->getAudioStream()->time_base, pktAudioBuf, frmAudioBuf);
    decodeVideo->init(demux->getVideoCodecID(), demux->getVideoCodecpar(), demux->getVideoStream()->time_base, pktVideoBuf, frmVideoBuf);
    audioPlayer->init(demux->getAudioCodecpar(), frmAudioBuf);

    demux->moveToThread(threadDemux);
    QObject::connect(threadDemux, &QThread::started, demux, &Demux::startDemux);
    threadDemux->start();

    decodeAudio->moveToThread(threadAudioDecode);
    QObject::connect(threadAudioDecode, &QThread::started, decodeAudio, &DecodeAudio::startDecoding);
    threadAudioDecode->start();

    decodeVideo->moveToThread(threadVideoDecode);
    QObject::connect(threadVideoDecode, &QThread::started, decodeVideo, &DecodeVideo::startDecoding);
    threadVideoDecode->start();

    audioPlayer->moveToThread(threadAudioPlayer);
    QObject::connect(threadAudioPlayer, &QThread::started, audioPlayer, &AudioPlayer::startPlay);
    threadAudioPlayer->start();

    videoPlayer->moveToThread(threadVideoPlayer);
    QObject::connect(threadVideoPlayer, &QThread::started, videoPlayer, &VideoPlayer::startPlay);
    threadVideoPlayer->start();

    return app.exec();
}

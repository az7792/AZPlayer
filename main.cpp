#include "renderer/audioplayer.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#ifdef __cplusplus
extern "C" {
#endif

#include <decode/decodevideo.h>
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

#include "main.moc"

#include "decode/decodeaudio.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/videoplayer.h"
int main(int argc, char *argv[]) {
    // const AVPixFmtDescriptor *t;
    // const AVPixFmtDescriptor *t1;
    // t = av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P10LE);
    // t1 = av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P);
    // return 0;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    // std::string URL = R"(D:\视频\[VCB-Studio] Karakai Jouzu no Takagi-san\[VCB-Studio] Karakai Jouzu no Takagi-san [Ma10p_1080p]\[VCB-Studio] Karakai Jouzu no Takagi-san [02][Ma10p_1080p][x265_flac].mkv)";
    std::string URL = R"(D:\视频\[VCB-Studio] K-ON!\[VCB-Studio] K-ON!\[VCB-Studio] K-ON! The Movie [Ma10p_1080p]\[VCB-S&philosophy-raws][K-ON! The Movie][MOIVE][BDRIP][Hi10P FLAC][1920X1080].mkv)";
    // std::string URL = R"(D:\视频\[SweetSub&VCB-Studio] Horimiya [Ma10p_1080p]\\[SweetSub&VCB-Studio] Horimiya [01][Ma10p_1080p][x265_flac].mkv)";
    // std::string URL = R"(D:\视频\[Mabors&VCB-Studio] Shigatsu wa Kimi no Uso [Hi10p_1080p]\[Mabors&VCB-Studio] Shigatsu wa Kimi no Uso [01][Hi10p_1080p][x264_2flac].mkv)";

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

    Demux *demux = new Demux;
    DecodeAudio *decodeAudio = new DecodeAudio;
    DecodeVideo *decodeVideo = new DecodeVideo;
    AudioPlayer *audioPlayer = new AudioPlayer;
    const QList<QObject *> rootObjects = engine.rootObjects();
    VideoWindow *videoWindow = nullptr;
    if (!rootObjects.isEmpty()) {
        videoWindow = rootObjects.first()->findChild<VideoWindow *>("videoWindow");
    }
    VideoPlayer *videoPlayer = new VideoPlayer(frmVideoBuf, videoWindow);

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

    // QTimer *timer = new QTimer();
    // QObject::connect(timer, &QTimer::timeout, [&]() {
    //     static qint64 last = 0;
    //     qint64 tmp = globalClock.getPts() / 1000;
    //     qDebug() << "audioPts:" << tmp - last;
    //     last = tmp;
    // });
    // timer->start(1);

    return app.exec();

    // QList<QObject *> roots = engine.rootObjects();
    // VideoItem *videoItem = roots.first()->findChild<VideoItem *>("video");

    // char errBuf[1024];
    // AVFormatContext *formatCtx = nullptr;
    // const AVCodec *codec = nullptr;
    // AVCodecContext *codecCtx = nullptr;
    // AVPacket *pkt = nullptr;
    // AVFrame *frm = nullptr;
    // SwsContext *swsCtx = nullptr;
    // std::vector<int> videoIdx, audioIdx, subtitleIdx;
    // const int testIdx = 0;

    // // 打开文件并初始化FormatContext
    // int ret = avformat_open_input(&formatCtx, URL, nullptr, nullptr);
    // if (ret != 0) {
    //     av_strerror(ret, errBuf, 1024);
    //     qDebug() << errBuf;
    //     return -1;
    // }

    // // 读取媒体文件的流信息
    // ret = avformat_find_stream_info(formatCtx, nullptr);
    // if (ret < 0) {
    //     av_strerror(ret, errBuf, 1024);
    //     qDebug() << errBuf;
    //     avformat_close_input(&formatCtx);
    //     return -1;
    // }

    // // 获取各种流的idx
    // for (int i = 0; i < formatCtx->nb_streams; ++i) {
    //     AVMediaType sType = formatCtx->streams[i]->codecpar->codec_type;
    //     if (sType == AVMEDIA_TYPE_VIDEO)
    //         videoIdx.push_back(i);
    //     else if (sType == AVMEDIA_TYPE_AUDIO)
    //         audioIdx.push_back(i);
    //     else if (sType == AVMEDIA_TYPE_SUBTITLE)
    //         subtitleIdx.push_back(i);
    // }

    // std::cout << "各种流的idx\nvideoIdx:";
    // for (auto &v : videoIdx)
    //     std::cout << v << " ";
    // std::cout << std::endl
    //           << "audioIdx:";
    // for (auto &v : audioIdx)
    //     std::cout << v << " ";
    // std::cout << std::endl
    //           << "subtitleIdx:";
    // for (auto &v : subtitleIdx)
    //     std::cout << v << " ";
    // std::cout << std::endl;

    // if (videoIdx.empty()) {
    //     qDebug() << "媒体文件无视频流";
    //     avformat_close_input(&formatCtx);
    //     return -1;
    // }

    // // 为第一个视频流创建解码器
    // codec = avcodec_find_decoder(formatCtx->streams[audioIdx[testIdx]]->codecpar->codec_id);
    // if (codec == nullptr) {
    //     qDebug() << "无合适的解码器";
    //     avformat_close_input(&formatCtx);
    //     return -1;
    // }

    // // 使用解码器初始化AVCodecContext
    // codecCtx = avcodec_alloc_context3(codec);
    // if (codecCtx == nullptr) {
    //     qDebug() << "无法为解码器分配AVCodecContext";
    //     avformat_close_input(&formatCtx);
    //     return -1;
    // }

    // // 将流的参数复制到AVCodecContext,也就是设置解码器的解码参数
    // ret = avcodec_parameters_to_context(codecCtx, formatCtx->streams[audioIdx[testIdx]]->codecpar);
    // if (ret < 0) {
    //     av_strerror(ret, errBuf, 1024);
    //     qDebug() << errBuf;
    //     avformat_close_input(&formatCtx);
    //     avcodec_free_context(&codecCtx);
    //     return -1;
    // }

    // // 启动编码器
    // ret = avcodec_open2(codecCtx, codec, nullptr);
    // if (ret != 0) {
    //     qDebug() << "打开编码器失败";
    //     avformat_close_input(&formatCtx);
    //     avcodec_free_context(&codecCtx);
    //     return -1;
    // }

    // pkt = av_packet_alloc();
    // frm = av_frame_alloc();

    // if (pkt == nullptr || frm == nullptr) {
    //     qDebug() << "pkt or frm初始化失败";
    //     avformat_close_input(&formatCtx);
    //     avcodec_free_context(&codecCtx);
    //     return -1;
    // }

    // // swsCtx = sws_getContext(codecCtx->width,codecCtx->height,codecCtx->pix_fmt,codecCtx->width,codecCtx->height,AV_PIX_FMT_RGBA,
    // //                         SWS_BILINEAR, nullptr, nullptr, nullptr);
    // // if(swsCtx == nullptr)
    // // {
    // //     qDebug() << "sws初始化失败";
    // //     avformat_close_input(&formatCtx);
    // //     avcodec_free_context(&codecCtx);
    // //     return -1;
    // // }
    // QThread *decodeThread = QThread::create([=] {
    //     AudioPlayer *audioPlayer = new AudioPlayer();
    //     audioPlayer->init(formatCtx->streams[audioIdx[testIdx]]->codecpar, frmBuf);
    //     QObject::connect(audioCtrl, &AudioPlayerCtrl::play, audioPlayer, &AudioPlayer::play, Qt::DirectConnection);
    //     QObject::connect(audioCtrl, &AudioPlayerCtrl::pause, audioPlayer, &AudioPlayer::pause, Qt::DirectConnection);
    //     int ct = -1;
    //     qint64 sT = QDateTime::currentMSecsSinceEpoch();
    //     while (av_read_frame(formatCtx, pkt) >= 0) {
    //         // qDebug() << pkt->stream_index ;
    //         if (pkt->stream_index != audioIdx[testIdx]) {
    //             av_packet_unref(pkt);
    //             // qDebug() << "err";
    //             continue;
    //         }
    //         if ((QDateTime::currentMSecsSinceEpoch() - sT) / 1000 != ct) {
    //             ct = (QDateTime::currentMSecsSinceEpoch() - sT) / 1000;
    //             qDebug() << ct << (QDateTime::currentMSecsSinceEpoch() - sT) << audioPlayer->audioClock();
    //         }

    //         int ret = avcodec_send_packet(codecCtx, pkt);
    //         av_packet_unref(pkt);
    //         if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    //             continue;

    //         ret = avcodec_receive_frame(codecCtx, frm);
    //         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    //             continue;

    //         // QImage img = frameToQImage(frm, swsCtx);
    //         // QMetaObject::invokeMethod(videoItem, "setFrame",
    //         //                           Qt::QueuedConnection,
    //         //                           Q_ARG(QImage, img));

    //         assert(frm->format == codecCtx->sample_fmt);
    //         int retwritnb = audioPlayer->write(frm);
    //     }
    // });
    // decodeThread->start();

    // app.exec();
    // avformat_close_input(&formatCtx);
    // avcodec_free_context(&codecCtx);
    // sws_freeContext(swsCtx);
    // av_packet_free(&pkt);
    // av_frame_free(&frm);

    // return 0;
}

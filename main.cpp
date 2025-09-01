#include "audioplayer.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <iostream>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include <QAudioOutput>
#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QThread>

class VideoItem : public QQuickItem {
    Q_OBJECT
public:
    VideoItem(QQuickItem *parent = nullptr);

    Q_INVOKABLE void setFrame(const QImage &image);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QImage m_frame;
    QMutex m_mutex;
    bool m_frameDirty = false;
};

#include <QAudioFormat>
#include <QBuffer>
#include <QMediaDevices>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <assert.h>

VideoItem::VideoItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void VideoItem::setFrame(const QImage &image) {
    {
        QMutexLocker locker(&m_mutex);
        m_frame = image.copy();
        m_frameDirty = true;
    }
    update(); // 通知 QML 重绘
}

QSGNode *VideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node)
        node = new QSGSimpleTextureNode();

    QImage img;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_frameDirty)
            return node;
        img = m_frame;
        m_frameDirty = false;
    }

    if (m_frame.isNull())
        return node;

    QSGTexture *texture = window()->createTextureFromImage(img);
    node->setTexture(texture);
    node->setOwnsTexture(true);
    node->setRect(boundingRect());

    return node;
}

QImage frameToQImage(AVFrame *frame, SwsContext *swsCtx) {
    QImage img(frame->width, frame->height, QImage::Format_RGBA8888);
    uint8_t *dest[4] = {img.bits(), nullptr, nullptr, nullptr};
    int destLinesize[4] = {static_cast<int>(img.bytesPerLine()), 0, 0, 0};

    sws_scale(swsCtx, frame->data, frame->linesize, 0,
              frame->height, dest, destLinesize);
    return img;
}

#include "main.moc"
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    qmlRegisterType<VideoItem>("CustomItems", 1, 0, "VideoItem");
    const QUrl url(QStringLiteral("qrc:/AZPlayer/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    QList<QObject *> roots = engine.rootObjects();
    VideoItem *videoItem = roots.first()->findChild<VideoItem *>("video");

    const char *URL = "D:\\视频\\[VCB-Studio] Karakai Jouzu no Takagi-san\\[VCB-Studio] Karakai Jouzu no Takagi-san [Ma10p_1080p]\\[VCB-Studio] Karakai Jouzu no Takagi-san [02][Ma10p_1080p][x265_flac].mkv";
    // const char *URL = "D:\\视频\\[VCB-Studio] K-ON!\\[VCB-Studio] K-ON!\\[VCB-Studio] K-ON! The Movie [Ma10p_1080p]\\[VCB-S&philosophy-raws][K-ON! The Movie][MOIVE][BDRIP][Hi10P FLAC][1920X1080].mkv";
    char errBuf[1024];
    AVFormatContext *formatCtx = nullptr;
    const AVCodec *codec = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frm = nullptr;
    SwsContext *swsCtx = nullptr;
    std::vector<int> videoIdx, audioIdx, subtitleIdx;
    const int testIdx = 0;

    // 打开文件并初始化FormatContext
    int ret = avformat_open_input(&formatCtx, URL, nullptr, nullptr);
    if (ret != 0) {
        av_strerror(ret, errBuf, 1024);
        qDebug() << errBuf;
        return -1;
    }

    // 读取媒体文件的流信息
    ret = avformat_find_stream_info(formatCtx, nullptr);
    if (ret < 0) {
        av_strerror(ret, errBuf, 1024);
        qDebug() << errBuf;
        avformat_close_input(&formatCtx);
        return -1;
    }

    // 获取各种流的idx
    for (int i = 0; i < formatCtx->nb_streams; ++i) {
        AVMediaType sType = formatCtx->streams[i]->codecpar->codec_type;
        if (sType == AVMEDIA_TYPE_VIDEO)
            videoIdx.push_back(i);
        else if (sType == AVMEDIA_TYPE_AUDIO)
            audioIdx.push_back(i);
        else if (sType == AVMEDIA_TYPE_SUBTITLE)
            subtitleIdx.push_back(i);
    }

    std::cout << "各种流的idx\nvideoIdx:";
    for (auto &v : videoIdx)
        std::cout << v << " ";
    std::cout << std::endl
              << "audioIdx:";
    for (auto &v : audioIdx)
        std::cout << v << " ";
    std::cout << std::endl
              << "subtitleIdx:";
    for (auto &v : subtitleIdx)
        std::cout << v << " ";
    std::cout << std::endl;

    if (videoIdx.empty()) {
        qDebug() << "媒体文件无视频流";
        avformat_close_input(&formatCtx);
        return -1;
    }

    // 为第一个视频流创建解码器
    codec = avcodec_find_decoder(formatCtx->streams[audioIdx[testIdx]]->codecpar->codec_id);
    if (codec == nullptr) {
        qDebug() << "无合适的解码器";
        avformat_close_input(&formatCtx);
        return -1;
    }

    // 使用解码器初始化AVCodecContext
    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        qDebug() << "无法为解码器分配AVCodecContext";
        avformat_close_input(&formatCtx);
        return -1;
    }

    // 将流的参数复制到AVCodecContext,也就是设置解码器的解码参数
    ret = avcodec_parameters_to_context(codecCtx, formatCtx->streams[audioIdx[testIdx]]->codecpar);
    if (ret < 0) {
        av_strerror(ret, errBuf, 1024);
        qDebug() << errBuf;
        avformat_close_input(&formatCtx);
        avcodec_free_context(&codecCtx);
        return -1;
    }

    // 启动编码器
    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret != 0) {
        qDebug() << "打开编码器失败";
        avformat_close_input(&formatCtx);
        avcodec_free_context(&codecCtx);
        return -1;
    }

    pkt = av_packet_alloc();
    frm = av_frame_alloc();

    if (pkt == nullptr || frm == nullptr) {
        qDebug() << "pkt or frm初始化失败";
        avformat_close_input(&formatCtx);
        avcodec_free_context(&codecCtx);
        return -1;
    }

    // swsCtx = sws_getContext(codecCtx->width,codecCtx->height,codecCtx->pix_fmt,codecCtx->width,codecCtx->height,AV_PIX_FMT_RGBA,
    //                         SWS_BILINEAR, nullptr, nullptr, nullptr);
    // if(swsCtx == nullptr)
    // {
    //     qDebug() << "sws初始化失败";
    //     avformat_close_input(&formatCtx);
    //     avcodec_free_context(&codecCtx);
    //     return -1;
    // }

    QThread *decodeThread = QThread::create([=] {
        AudioPlayer *audioPlayer = new AudioPlayer();
        audioPlayer->init(formatCtx->streams[audioIdx[testIdx]]->codecpar);
        int ct = -1;
        qint64 sT = QDateTime::currentMSecsSinceEpoch();
        while (av_read_frame(formatCtx, pkt) >= 0) {
            // qDebug() << pkt->stream_index ;
            if (pkt->stream_index != audioIdx[testIdx]) {
                av_packet_unref(pkt);
                // qDebug() << "err";
                continue;
            }
            if ((QDateTime::currentMSecsSinceEpoch() - sT) / 1000 != ct) {
                ct = (QDateTime::currentMSecsSinceEpoch() - sT) / 1000;
                qDebug() << ct;
            }

            int ret = avcodec_send_packet(codecCtx, pkt);
            av_packet_unref(pkt);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;

            ret = avcodec_receive_frame(codecCtx, frm);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;

            // QImage img = frameToQImage(frm, swsCtx);
            // QMetaObject::invokeMethod(videoItem, "setFrame",
            //                           Qt::QueuedConnection,
            //                           Q_ARG(QImage, img));

            assert(frm->format == codecCtx->sample_fmt);
            int retwritnb = audioPlayer->write(frm);
        }
    });
    decodeThread->start();

    app.exec();
    avformat_close_input(&formatCtx);
    avcodec_free_context(&codecCtx);
    sws_freeContext(swsCtx);
    av_packet_free(&pkt);
    av_frame_free(&frm);

    return 0;
}

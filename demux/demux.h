#ifndef DEMUX_H
#define DEMUX_H
#include "utils.h"
#include <QObject>
#include <atomic>
#include <string>
#include <thread>
#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#ifdef __cplusplus
}
#endif

class Demux : public QObject {
    Q_OBJECT
public:
    explicit Demux(QObject *parent = nullptr);
    ~Demux();

    // 初始化
    bool init(const std::string URL, sharedPktQueue audioQ, sharedPktQueue videoQ, sharedPktQueue subtitleQ);
    // 反初始化，恢复到未初始化之前的状态
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    // seek
    void seekBySec(double ts, double rel);

    int getDuration();

    AVStream *getVideoStream();
    AVStream *getAudioStream();
    AVStream *getSubtitleStream();

    AVFormatContext *formatCtx();
public slots:

private:
    sharedPktQueue m_audioPktBuf;
    sharedPktQueue m_videoPktBuf;
    sharedPktQueue m_subtitlePktBuf;

    AVFormatContext *m_formatCtx = nullptr;
    std::string m_URL;                                               // 媒体URL
    std::vector<int> m_videoIdx, m_audioIdx, m_subtitleIdx;          // 各个流的ID
    std::atomic<int> m_usedVIdx{-1}, m_usedAIdx{-1}, m_usedSIdx{-1}; // 当前使用的流ID

    char errBuf[512];
    std::atomic<bool> m_stop{true};
    std::thread m_thread;
    bool m_initialized = false;

    std::atomic<bool> m_needSeek{false};
    double m_seekRel = 0.0;

private:
    void demuxLoop();
    void pushVideoPkt(AVPacket *pkt);
    void pushAudioPkt(AVPacket *pkt);
    void pushSubtitlePkt(AVPacket *pkt);
    void pushPkt(sharedPktQueue q, AVPacket *pkt);
};

#endif // DEMUX_H

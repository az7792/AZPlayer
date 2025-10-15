#ifndef DEMUX_H
#define DEMUX_H
#include "utils.h"
#include <QObject>
#include <atomic>
#include <string>
#include <mutex>
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
    bool init(const std::string URL);
    // 反初始化，恢复到未初始化之前的状态
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    // seek
    void seekBySec(double ts, double rel);

    bool switchVideoStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq);
    bool switchSubtitleStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq);
    bool switchAudioStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq);

    // 0视频 1字幕 2音频
    void closeStream(int streamType);

    int getDuration();

    AVStream *getVideoStream();
    AVStream *getAudioStream();
    AVStream *getSubtitleStream();

    bool haveVideoStream() { return !m_videoIdx.empty(); }
    bool haveAudioStream() { return !m_audioIdx.empty(); }
    bool haveSubtitleStream() { return !m_subtitleIdx.empty(); }

    const std::array<std::vector<QString>, 3> &streamInfo() { return m_stringInfo; }

    AVFormatContext *formatCtx();
public slots:

private:
    weakPktQueue m_audioPktBuf;
    weakPktQueue m_videoPktBuf;
    weakPktQueue m_subtitlePktBuf;
    weakFrmQueue m_audioFrmBuf;    // 只用与修改队列序号用
    weakFrmQueue m_videoFrmBuf;    // 只用与修改队列序号用
    weakFrmQueue m_subtitleFrmBuf; // 只用与修改队列序号用

    void addAllQueueSerial();

    AVFormatContext *m_formatCtx = nullptr;
    std::string m_URL;                                               // 媒体URL
    std::vector<int> m_videoIdx, m_audioIdx, m_subtitleIdx;          // 各个流的ID
    std::atomic<int> m_usedVIdx{-1}, m_usedAIdx{-1}, m_usedSIdx{-1}; // 当前使用的流ID
    std::array<std::vector<QString>, 3> m_stringInfo;                // 流描述

    char errBuf[512];
    std::atomic<bool> m_stop{true};
    std::thread m_thread;
    bool m_initialized = false;
    std::mutex m_mutex;//保护队列和流ID的更新

    std::atomic<bool> m_needSeek{false};
    double m_seekRel = 0.0;

private:
    void demuxLoop();
    void pushVideoPkt(AVPacket *pkt);
    void pushAudioPkt(AVPacket *pkt);
    void pushSubtitlePkt(AVPacket *pkt);
    void pushPkt(weakPktQueue wq, AVPacket *pkt);
    void fillStreamInfo();
};

#endif // DEMUX_H

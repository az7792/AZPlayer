#ifndef DEMUX_H
#define DEMUX_H

#include "clock/globalclock.h"
#include "utils.h"
#include <QObject>
#include <atomic>
#include <string>
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

    bool init(const std::string URL, sharedPktQueue audioQ, sharedPktQueue videoQ, sharedPktQueue subtitleQ);
    bool uninit();

    enum AVCodecID getVideoCodecID();
    enum AVCodecID getAudioCodecID();
    enum AVCodecID getSubtitleID();

    AVCodecParameters *getVideoCodecpar();
    AVCodecParameters *getAudioCodecpar();
    AVCodecParameters *getSubtitleCodecpar();

    AVStream *getVideoStream();
    AVStream *getAudioStream();
    AVStream *getSubtitleStream();

    AVFormatContext *formatCtx();

public:
    sharedPktQueue m_audioPktBuf;
    sharedPktQueue m_videoPktBuf;
    sharedPktQueue m_subtitlePktBuf;

public slots:
    void startDemux();
signals:
    void finished(); // 解复用线程退出信号
private:
    AVFormatContext *m_formatCtx = nullptr;
    std::string m_URL;                                      // 媒体URL
    std::vector<int> m_videoIdx, m_audioIdx, m_subtitleIdx; // 各个流的ID
    int m_usedVIdx = -1, m_usedAIdx = -1, m_usedSIdx = -1;  // 当前使用的流ID

    char errBuf[512];
    std::atomic<bool> m_stop{true};
    bool m_initialized = false;

    int seekCnt = 0;

private:
    void pushVideoPkt(AVPacket *pkt);
    void pushAudioPkt(AVPacket *pkt);
    void pushSubtitlePkt(AVPacket *pkt);
    void pushPkt(sharedPktQueue q, AVPacket *pkt);
};

#endif // DEMUX_H

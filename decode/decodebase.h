#ifndef DECODEBASE_H
#define DECODEBASE_H

#include "utils.h"
#include <QObject>
#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

class DecodeBase : public QObject {
    Q_OBJECT
public:
    explicit DecodeBase();
    ~DecodeBase();
    bool init(enum AVCodecID id,
              const struct AVCodecParameters *par,
              AVRational time_base,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

    bool initialized();

public:
    sharedPktQueue m_pktBuf;
    sharedFrmQueue m_frmBuf;

public slots:
    virtual void startDecoding() { emit finished(); }
    void stopDecoding();

signals:
    void finished(); // 解码线程退出信号

protected:
    const AVCodec *m_codec = nullptr; // FFmpeg内部管理，不用释放
    AVCodecContext *m_codecCtx;
    char errBuf[512] = {0};
    std::atomic<bool> m_stop{true};
    bool m_initialized = false;
    AVRational m_time_base;
};

#endif // DECODEBASE_H

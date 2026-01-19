// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODEBASE_H
#define DECODEBASE_H

#include "types/ptrs.h"
#include <QObject>
#include <atomic>
#include <thread>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
AZ_EXTERN_C_END

class DecodeBase : public QObject {
    Q_OBJECT
public:
    explicit DecodeBase(QObject *parent = nullptr);
    ~DecodeBase();
    bool init(AVStream *stream,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

    // 反初始化，恢复到未初始化之前的状态
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    bool initialized() const;

protected:
    sharedPktQueue m_pktBuf;
    sharedFrmQueue m_frmBuf;

    const AVCodec *m_codec = nullptr; // FFmpeg内部管理，不用释放
    AVCodecContext *m_codecCtx = nullptr;
    int m_streamIdx = -1;
    char errBuf[512] = {0};
    std::atomic<bool> m_stop{true};
    bool m_isEOF = false;
    int m_serial = 0;
    std::thread m_thread;
    bool m_initialized = false;
    AVRational m_time_base;

protected:
    virtual void decodingLoop() = 0;
    bool getPkt(AVPktItem &pktItem, bool &needFlushBuffers);
};

#endif // DECODEBASE_H

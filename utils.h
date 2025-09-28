#ifndef UTILS_H
#define UTILS_H
#include "spscqueue.h"
#include <QAudioFormat>
#include <limits>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

struct AVPktItem {
    AVPacket *pkt = nullptr;
    int seekCnt = 0;
};

struct AVFrmItem {
    AVFrame *frm = nullptr;
    int seekCnt = 0;
    double pts = std::numeric_limits<double>::quiet_NaN();
};

using sharedPktQueue = std::shared_ptr<SPSCQueue<AVPktItem>>;
using sharedFrmQueue = std::shared_ptr<SPSCQueue<AVFrmItem>>;

struct AudioPar {
    int sampleRate = 0;                               // 采样率
    AVSampleFormat sampleFormat = AV_SAMPLE_FMT_NONE; // 采样格式
    AVChannelLayout ch_layout;                        // 通道布局

    AudioPar() {
        av_channel_layout_default(&ch_layout, 0);
    }

    ~AudioPar() {
        av_channel_layout_uninit(&ch_layout);
    }

    void reset() {
        sampleRate = 0;
        sampleFormat = AV_SAMPLE_FMT_NONE;
        av_channel_layout_default(&ch_layout, 0);
    }
};

struct AVPacketGuard {
    AVPacket *p = nullptr;
    AVPacketGuard() : p(av_packet_alloc()) {}
    ~AVPacketGuard() {
        if (p)
            av_packet_free(&p);
    }

    void unref() {
        if (p)
            av_packet_unref(p);
    }

    // 禁止拷贝
    AVPacketGuard(const AVPacketGuard &) = delete;
    AVPacketGuard &operator=(const AVPacketGuard &) = delete;

    // 支持移动
    AVPacketGuard(AVPacketGuard &&other) noexcept : p(other.p) {
        other.p = nullptr;
    }
    AVPacketGuard &operator=(AVPacketGuard &&other) noexcept {
        if (this != &other) {
            if (p)
                av_packet_free(&p);
            p = other.p;
            other.p = nullptr;
        }
        return *this;
    }
};

struct AVFrameGuard {
    AVFrame *f = nullptr;
    AVFrameGuard() : f(av_frame_alloc()) {}
    ~AVFrameGuard() {
        if (f)
            av_frame_free(&f);
    }

    void unref() {
        if (f)
            av_frame_unref(f);
    }

    // 禁止拷贝
    AVFrameGuard(const AVFrameGuard &) = delete;
    AVFrameGuard &operator=(const AVFrameGuard &) = delete;

    // 支持移动
    AVFrameGuard(AVFrameGuard &&other) noexcept : f(other.f) {
        other.f = nullptr;
    }
    AVFrameGuard &operator=(AVFrameGuard &&other) noexcept {
        if (this != &other) {
            if (f)
                av_frame_free(&f);
            f = other.f;
            other.f = nullptr;
        }
        return *this;
    }
};

struct AVCodecContextGuard {
    AVCodecContext *c = nullptr;
    AVCodecContextGuard() = default;
    ~AVCodecContextGuard() {
        if (c)
            avcodec_free_context(&c);
    }

    // 禁止拷贝
    AVCodecContextGuard(const AVCodecContextGuard &) = delete;
    AVCodecContextGuard &operator=(const AVCodecContextGuard &) = delete;

    // 支持移动
    AVCodecContextGuard(AVCodecContextGuard &&other) noexcept : c(other.c) {
        other.c = nullptr;
    }
    AVCodecContextGuard &operator=(AVCodecContextGuard &&other) noexcept {
        if (this != &other) {
            if (c)
                avcodec_free_context(&c);
            c = other.c;
            other.c = nullptr;
        }
        return *this;
    }
};

#endif // UTILS_H

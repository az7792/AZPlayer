// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TYPES_H
#define TYPES_H
#include "compat/compat.h"
#include <limits>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
AZ_EXTERN_C_END

constexpr double INVALID_DOUBLE = std::numeric_limits<double>::quiet_NaN();

enum class MediaType : uint8_t {
    Video = 0,
    Subtitle,
    Audio,
    Count
};
using MediaIdx = MediaType;

struct AVPktItem {
    AVPacket *pkt = nullptr;
    int serial = 0;
};

struct AVFrmItem {
    AVFrame *frm = nullptr;
    AVSubtitle sub{};
    int width{0}, height{0}; // HACK 目前仅用于表示字幕的分辨率大小
    int serial = 0;
    double pts = INVALID_DOUBLE;
    double duration = INVALID_DOUBLE;
};

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

#endif /* TYPES_H */

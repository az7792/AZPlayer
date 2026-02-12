// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include "compat/compat.h"
#include "renderdata.h"
#include "types/ptrs.h"
#include <QObject>
#include <QPair>
#include <atomic>
#include <thread>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
AZ_EXTERN_C_END

class VideoPlayer : public QObject {
    Q_OBJECT
public:
    using FrameInterval = QPair<double, double>;

    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool init(sharedFrmQueue frmBuf, sharedFrmQueue subFrmBuf);
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    void togglePaused();

    void clearSubtitle();

public:
    sharedFrmQueue m_frmBuf;    // 视频
    sharedFrmQueue m_subFrmBuf; // 字幕

signals:
    void renderDataReady(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData);
    void seeked();
    void playedOneFrame(); // 播放了一帧

private:
    // 双缓冲
    VideoDoubleBuf m_videoRenderData;       // 视频
    SubtitleDoubleBuf m_subRenderData;      // 字幕
    FrameInterval m_lastVideoFrameInterval; // 上一帧视频帧区间
    double m_subtitleEndDisplayTime;        // 上一帧字幕结束时间
    bool m_needClearSubtitle;               // 需要清空字幕
    double m_renderTime;                    // 实际渲染指令被发出的时间(相对现实时间，秒)

    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_paused{false};
    bool m_forceRefresh{false};
    int m_serial = 0;
    std::thread m_thread;
    bool m_initialized = false; // 是否已经初始化

    int m_width{0};  // 视频宽
    int m_height{0}; // 视频高

private:
    /**
     * 写入一帧数据
     * @warning 方法会阻塞线程
     */
    bool write(AVFrmItem &videoFrmitem);

    bool getVideoFrm(AVFrmItem &item);

    void playerLoop();

    static double getDuration(const FrameInterval &last, const FrameInterval &now);
};

#endif // VIDEOPLAYER_H

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLAYBACKSTATS_H
#define PLAYBACKSTATS_H

#include <QObject>
#include <QSize>
#include <QString>
#include <chrono>

class PlaybackStats : public QObject {
    Q_OBJECT
public:
    static PlaybackStats &instance();

    void reset();

    void frameRendered(); // 每渲染一帧调用一次，统计FPS用

    // 获取拼接好的文本信息（HTML主要是为了带颜色）
    Q_INVOKABLE QString getPlaybackStatsStringHTML() const;

public:
    // ==== 队列长度 ====
    int audioPacketCount{};
    int videoPacketCount{};
    int subtitlePacketCount{};

    int audioFrameCount{};
    int videoFrameCount{};
    int subtitleFrameCount{};

    // ==== 视频/字幕尺寸 ====
    QSize videoSize{};
    QSize subtitleSize{};
    QSize FBOSize{};

    // ==== FPS ====
    double videoFps{};
    double outputFps{};

    // ==== 帧状态 ====
    int lateFrameCount{};
    int earlyFrameCount{};
    int droppedFrameCount{};

    // ==== 时间戳 ====
    double videoPTS{};
    double audioPTS{};
    double avPtsDiff{};

    // 视频信息
    QString videoPixFormat;
    int videoFormat; // AVFrame->format 这儿仅用于标记，避免重复更新videoPixFormat

private:
    PlaybackStats(const PlaybackStats &) = delete;
    PlaybackStats &operator=(const PlaybackStats &) = delete;
    explicit PlaybackStats(QObject *parent = nullptr);

private:
    int m_frameCounter{};
    std::chrono::steady_clock::time_point m_lastFpsTime{std::chrono::steady_clock::now()};
};

#endif // PLAYBACKSTATS_H

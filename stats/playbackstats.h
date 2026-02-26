// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLAYBACKSTATS_H
#define PLAYBACKSTATS_H

#include <QObject>
#include <QSize>
#include <QString>
#include <chrono>
#include <deque>

class PlaybackStats : public QObject {
    Q_OBJECT
public:
    static PlaybackStats &instance();

    void reset();

    void frameRendered(); // 每渲染一帧调用一次，统计FPS用

    void updateVideoDecodeTime(double ms); // 解码一次视频调用一次
    void updateVideoPrepTime(double ms);   // 准备一次视频调用一次
    void updateSubPrepTime(double ms);     // 准备一次字幕调用一次

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

    // ==== 视频解码耗时统计 ms ====
    double videoDecodeTime{0.0};    // 当前视频帧解码耗时
    double avgVideoDecodeTime{0.0}; // 视频解码平均耗时

    // ==== 视频数据准备耗时 ms====
    double videoPrepTime{0.0};
    double avgVideoPrepTime{0.0};

    // ==== 字幕数据准备耗时 ms====
    double subPrepTime{0.0};
    double avgSubPrepTime{0.0};

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

    // 辅助函数：计算队列平均值
    double calculateAverage(std::deque<double> &deq, double newValue);

private:
    int m_frameCounter{};
    std::chrono::steady_clock::time_point m_lastFpsTime{std::chrono::steady_clock::now()};

    // 最近10次的样本容器
    std::deque<double> m_vDecSamples;
    std::deque<double> m_vPrepSamples;
    std::deque<double> m_sPrepSamples;
    const size_t m_maxSamples = 10;
};

#endif // PLAYBACKSTATS_H

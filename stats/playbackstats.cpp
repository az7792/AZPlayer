// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "playbackstats.h"

PlaybackStats::PlaybackStats(QObject *parent)
    : QObject{parent} { reset(); }

double PlaybackStats::calculateAverage(std::deque<double> &deq, double newValue) {
    deq.push_back(newValue);
    if (deq.size() > m_maxSamples) {
        deq.pop_front();
    }
    double sum = std::accumulate(deq.begin(), deq.end(), 0.0);
    return sum / deq.size();
}

PlaybackStats &PlaybackStats::instance() {
    static PlaybackStats instance;
    return instance;
}

void PlaybackStats::reset() {
    // ==== 队列长度 ====
    audioPacketCount = 0;
    videoPacketCount = 0;
    subtitlePacketCount = 0;

    audioFrameCount = 0;
    videoFrameCount = 0;
    subtitleFrameCount = 0;

    // ==== 视频/字幕尺寸 ====
    videoSize = QSize{};
    subtitleSize = QSize{};
    FBOSize = QSize{};

    // ==== 像素格式 ====
    videoFormat = -1;
    videoPixFormat = "";

    // ==== FPS ====
    videoFps = 0.0;
    outputFps = 0.0;
    m_frameCounter = 0;
    m_lastFpsTime = std::chrono::steady_clock::now();

    // ==== 视频解码耗时统计 ms ====
    videoDecodeTime = 0.0;
    avgVideoDecodeTime = 0.0;
    // ==== 视频数据准备耗时 ms====
    videoPrepTime = 0.0;
    avgVideoPrepTime = 0.0;
    // ==== 字幕数据准备耗时 ms====
    subPrepTime = 0.0;
    avgSubPrepTime = 0.0;

    // ==== 帧状态 ====
    lateFrameCount = 0;
    earlyFrameCount = 0;
    droppedFrameCount = 0;

    // ==== 时间戳 ====
    videoPTS = 0.0;
    audioPTS = 0.0;
    avPtsDiff = 0.0;
}

void PlaybackStats::frameRendered() {
    using namespace std::chrono;

    constexpr double alpha = 0.9; // 平滑因子，0~1，越小越平滑
    m_frameCounter++;

    auto now = steady_clock::now();
    auto elapsedMs = duration_cast<milliseconds>(now - m_lastFpsTime).count();

    if (elapsedMs >= 1000) {
        // 当前瞬时帧率
        double instantFps = static_cast<double>(m_frameCounter) * 1000.0 / elapsedMs;

        // 指数平滑
        outputFps = alpha * instantFps + (1.0 - alpha) * outputFps;
        m_frameCounter = 0;
        m_lastFpsTime = now;
    }
}

void PlaybackStats::updateVideoDecodeTime(double ms) {
    videoDecodeTime = ms;
    avgVideoDecodeTime = calculateAverage(m_vDecSamples, ms);
}

void PlaybackStats::updateVideoPrepTime(double ms) {
    videoPrepTime = ms;
    avgVideoPrepTime = calculateAverage(m_vPrepSamples, ms);
}

void PlaybackStats::updateSubPrepTime(double ms) {
    subPrepTime = ms;
    avgSubPrepTime = calculateAverage(m_sPrepSamples, ms);
}

QString PlaybackStats::getPlaybackStatsStringHTML() const {
    QString str;

    // 辅助 Lambda：用于生成带颜色标签和数值的项目
    auto item = [](const QString &label, const QString &value, const QString &labelColor, const QString &valColor) {
        return QString("<b><span style='color:%1;'>%2:</span></b> <span style='color:%3;'>%4</span> ")
            .arg(labelColor, label, valColor, value);
    };

    // ==== 队列长度 (Packets) ====
    str += item("Pkt队列: A", QString::number(audioPacketCount), "white", "green");
    str += item("V", QString::number(videoPacketCount), "white", "blue");
    str += item("S", QString::number(subtitlePacketCount), "white", "orange");
    str += "<br>";

    // ==== 队列长度 (Frames) ====
    str += item("Frm队列: A", QString::number(audioFrameCount), "white", "green");
    str += item("V", QString::number(videoFrameCount), "white", "blue");
    str += item("S", QString::number(subtitleFrameCount), "white", "orange");
    str += "<br>";

    // ==== 尺寸 (Size) ====
    str += item("尺寸: Video", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "white", "cyan");
    str += item("Subtitle", QString("%1x%2").arg(subtitleSize.width()).arg(subtitleSize.height()), "white", "magenta");
    str += item("Frame Buffer", QString("%1x%2").arg(FBOSize.width()).arg(FBOSize.height()), "white", "gray");
    str += "<br>";

    // ==== 4. 像素格式 & 视频解码耗时 ====
    str += item("像素格式", videoPixFormat, "white", "yellow");

    // 性能统计：解码与准备 ms
    int vdec = avgVideoDecodeTime;
    int vprep = avgVideoPrepTime;
    int sprep = avgSubPrepTime;

    str += item("视频解码", QString::number(vdec) + "ms", "white", (vdec > 30 ? "red" : "#55FF55"));
    str += item("视频准备", QString::number(vprep) + "ms", "white", (vprep > 30 ? "red" : "#55FF55"));
    str += item("字幕准备", QString::number(sprep) + "ms", "white", (sprep > 5 ? "red" : "#55FF55"));
    str += "<br>";

    // ==== FPS 逻辑处理 ====
    QString outputFpsColor = "green";
    if (videoFps > 0) {
        double ratio = outputFps / videoFps;
        if (ratio < 0.5)
            outputFpsColor = "red";
        else if (ratio < 0.7)
            outputFpsColor = "yellow";
    }
    str += item("源FPS", QString::number(videoFps, 'f', 3), "white", "cyan");
    str += item("当前FPS", QString::number(outputFps, 'f', 3), "white", outputFpsColor);
    str += "<br>";

    // ==== 帧状态 (Frames Status) ====
    str += item("落后", QString::number(lateFrameCount), "white", "yellow");
    str += item("超前", QString::number(earlyFrameCount), "white", "green");
    str += item("丢失", QString::number(droppedFrameCount), "white", "red");
    str += "<br>";

    // ==== PTS & 同步 (PTS) ====
    QString avDiffColor = "green";
    if (qAbs(avPtsDiff) * 1000 > 10)
        avDiffColor = "red";
    else if (qAbs(avPtsDiff) * 1000 > 5)
        avDiffColor = "yellow";

    str += item("VideoPTS", QString::number(videoPTS, 'f', 3), "white", "cyan");
    str += item("AudioPTS", QString::number(audioPTS, 'f', 3), "white", "magenta");
    str += item("AVDiff", QString::number(avPtsDiff, 'f', 3), "white", avDiffColor);
    str += "<br>";

    return str;
}

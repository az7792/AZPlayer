// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videoplayer.h"
#include "clock/globalclock.h"
#include "stats/playbackstats.h"
#include <QDateTime>
#include <QDebug>

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject{parent} {}

VideoPlayer::~VideoPlayer() {
    uninit();
}

bool VideoPlayer::init(sharedFrmQueue frmBuf, sharedFrmQueue subFrmBuf) {
    if (m_initialized) {
        uninit();
    }
    m_forceRefresh = false;
    m_frmBuf = frmBuf;
    m_subFrmBuf = subFrmBuf;
    m_videoRenderData.reset([](VideoRenderData &b1, VideoRenderData &b2) -> bool {
        b1.reset();
        b2.reset();
        return true;
    });
    m_subRenderData.reset([](SubRenderData &b1, SubRenderData &b2) -> bool {
        b1.reset();
        b2.reset();
        return true;
    });
    m_lastVideoFrameInterval = qMakePair(INVALID_DOUBLE, INVALID_DOUBLE);
    m_needClearSubtitle = false;
    m_subtitleEndDisplayTime = 1e9;
    m_renderTime = std::numeric_limits<double>::quiet_NaN();
    m_serial = 0;
    m_width = 0;
    m_height = 0;
    return true;
}

bool VideoPlayer::uninit() {
    stop();

    m_frmBuf.reset();
    m_subFrmBuf.reset();
    m_videoRenderData.reset([](VideoRenderData &b1, VideoRenderData &b2) -> bool {
        b1.reset();
        b2.reset();
        return true;
    });
    m_subRenderData.reset([](SubRenderData &b1, SubRenderData &b2) -> bool {
        b1.reset();
        b2.reset();
        return true;
    });
    emit renderDataReady(nullptr, nullptr);
    return true;
}

void VideoPlayer::start() {
    if (m_thread.joinable()) {
        return; // 已经在运行了
    }
    m_stop.store(false, std::memory_order_relaxed);
    m_paused.store(false, std::memory_order_relaxed);
    m_thread = std::thread([this]() {
        playerLoop();
    });
}

void VideoPlayer::stop() {
    if (!m_thread.joinable()) {
        return; // 已经退出
    }
    m_stop.store(true, std::memory_order_relaxed);
    m_thread.join();
}

void VideoPlayer::togglePaused() {
    if (m_stop.load(std::memory_order_relaxed)) {
        return;
    }
    bool paused = m_paused.load(std::memory_order_relaxed);
    if (paused) {
        m_renderTime = getRelativeSeconds();
    }
    m_paused.store(!paused, std::memory_order_release);
}

void VideoPlayer::clearSubtitle() {
    m_needClearSubtitle = true;
}

void VideoPlayer::playerLoop() {
    // 确保音视频设备都完成了基本初始化
    while (!DeviceStatus::instance().initialized() && !m_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    AVFrmItem frmItem;
    while (!m_stop.load(std::memory_order_relaxed)) {
        // 处理视频seek/切流
        int ok = getVideoFrm(frmItem);
        if (!ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (m_serial != m_frmBuf->serial()) {
            m_serial = m_frmBuf->serial();
            m_forceRefresh = true;
        }

        if (!m_forceRefresh && m_paused.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 写如入设备
        write(frmItem);

        if (m_forceRefresh && GlobalClock::instance().mainClockType() == ClockType::VIDEO) {
            emit seeked();
        }
        m_forceRefresh = false;
        emit playedOneFrame();
    }
    return;
}

bool VideoPlayer::write(AVFrmItem &videoFrmitem) {
    if (!videoFrmitem.frm) {
        return false;
    }

    // 更新视频宽高
    m_width = videoFrmitem.frm->width;
    m_height = videoFrmitem.frm->height;

    FrameInterval nowVideoFrameInterval = qMakePair(videoFrmitem.pts, videoFrmitem.duration);
    // 上一帧持续时间
    double delay = getDuration(m_lastVideoFrameInterval, nowVideoFrameInterval);

    // 处理字幕seek/切流
    AVFrmItem subFrmItem;
    while (m_subFrmBuf->peekFirst(subFrmItem) && subFrmItem.serial != m_subFrmBuf->serial()) {
        m_subFrmBuf->pop(subFrmItem);
        avsubtitle_free(&subFrmItem.sub);
        m_forceRefresh = true;
    }

    // ==============在渲染之前准备好数据==============
    // clang-format off
    double startTime = getRelativeSeconds();
    m_videoRenderData.write([&](VideoRenderData &renData, [[maybe_unused]] int idx) -> bool {
        m_lastVideoFrameInterval = nowVideoFrameInterval;
        renData.updateFormat(videoFrmitem);
        return true;
    }, false);
    PlaybackStats::instance().updateVideoPrepTime((getRelativeSeconds() - startTime) * 1000);
    // clang-format on

    startTime = getRelativeSeconds();
    if (ASSRender::instance().initialized()) {
        handleASSSubtitle(videoFrmitem.pts);
        // ASS字幕下应该没有图形字幕了
        Q_ASSERT(m_subFrmBuf->peekFirst(subFrmItem) == false);
    } else {
        // 位图字幕
        handleBitmapSubtitle();
    }

    if (m_needClearSubtitle || m_forceRefresh) {
        handleEmptySubtitle();
        m_needClearSubtitle = false;
    }
    PlaybackStats::instance().updateSubPrepTime((getRelativeSeconds() - startTime) * 1000);
    // ==============渲染数据准备完毕==============

    // 同步主时钟
    double maxFrameDuration = GlobalClock::instance().maxFrameDuration();
    double diff = GlobalClock::instance().videoPts() - GlobalClock::instance().getMainPts();
    // //[0.004 - 1/fps -  0.01]与主时钟差距保证在范围外需要同步 //NOTE： ffplay用的[0.04-0.1]
    double syncThreshold = std::max(0.004, std::min(0.01, delay));
    if (!std::isnan(diff) && std::abs(diff) < maxFrameDuration) {
        if (diff <= -syncThreshold) { // 落后太多，尝试直接播放
            PlaybackStats::instance().lateFrameCount++;
            delay = std::max(0.0, delay + diff);
        } else if (diff >= syncThreshold) { // 领先太多
            PlaybackStats::instance().earlyFrameCount++;
            delay = delay + (delay > 0.1 ? diff : delay);
        }
    }

    if (std::isnan(m_renderTime) || m_forceRefresh) {
        m_renderTime = getRelativeSeconds();
    } else {
        m_renderTime += delay;
    }

    double nowTime = getRelativeSeconds();
    double dt = m_renderTime - nowTime;

    if (dt > 0) {
        if (dt > 0.1) {
            dt = 0.1;
            m_renderTime = nowTime + dt;
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }

    AVFrmItem tmpItem;
    bool peekOk = m_frmBuf->peekFirst(tmpItem);

    nowTime = getRelativeSeconds();
    if (!peekOk || nowTime <= m_renderTime + getDuration(nowVideoFrameInterval, qMakePair(tmpItem.pts, tmpItem.duration))) {
        m_videoRenderData.release();
        m_subRenderData.release();
        emit renderDataReady(&m_videoRenderData, &m_subRenderData);
    } else {
        PlaybackStats::instance().droppedFrameCount++;
    }

    // 更新视频时钟
    GlobalClock::instance().setVideoClk(videoFrmitem.pts);
    GlobalClock::instance().syncExternalClk(ClockType::VIDEO);

    PlaybackStats::instance().videoPTS = videoFrmitem.pts;
    PlaybackStats::instance().avPtsDiff = GlobalClock::instance().getMainPts() - GlobalClock::instance().videoPts();
    return true;
}

bool VideoPlayer::getVideoFrm(AVFrmItem &item) {
    if (item.frm != nullptr) {
        if (item.serial != m_frmBuf->serial()) {
            av_frame_free(&item.frm);
            m_forceRefresh = true;
            return false;
        }
        return true;
    }

    if (!m_frmBuf->pop(item)) { // 空
        return false;
    } else if (item.serial != m_frmBuf->serial()) { // 非空 但是序号不同
        av_frame_free(&item.frm);
        m_forceRefresh = true;
        return false;
    }
    return true;
}

double VideoPlayer::getDuration(const FrameInterval &last, const FrameInterval &now) {
    double maxFrameDuration = GlobalClock::instance().maxFrameDuration();
    double duration = now.first - last.first;
    if (std::isnan(duration) || duration <= 0 || duration > maxFrameDuration)
        duration = std::isnan(last.second) ? 0.0 : last.second;
    return duration;
}

// clang-format off
void VideoPlayer::handleASSSubtitle(double pts)
{
    m_subRenderData.write([&](SubRenderData &renData, int) -> bool {
        renData.updateASSImage(pts, m_width, m_height);
        renData.frmItem.width  = m_width;
        renData.frmItem.height = m_height;
        return true;
    }, false);
}

void VideoPlayer::handleBitmapSubtitle()
{
    AVFrmItem subFrmItem;
    double videoPts = GlobalClock::instance().videoPts();
    if (m_subFrmBuf->peekFirst(subFrmItem) && videoPts >= subFrmItem.pts) {
        // 有新字幕
        m_subFrmBuf->pop(subFrmItem);
        m_subRenderData.write([&](SubRenderData &renData, [[maybe_unused]] int idx) -> bool {
            renData.updateBitmapImage(&subFrmItem, m_width, m_height);
            m_subtitleEndDisplayTime = subFrmItem.pts + subFrmItem.duration;
            return true;
        }, false);
    } else if (videoPts >= m_subtitleEndDisplayTime) {
        // 上一帧字幕显示时间到期
        m_subtitleEndDisplayTime = 1e9;
        handleEmptySubtitle();
    }
}

void VideoPlayer::handleEmptySubtitle()
{
    m_subRenderData.write([&](SubRenderData &renData, int) -> bool {
        renData.updateBitmapImage(nullptr, m_width, m_height);
        return true;
    }, false);
}
// clang-format on

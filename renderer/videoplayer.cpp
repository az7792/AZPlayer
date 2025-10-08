#include "videoplayer.h"
#include "clock/globalclock.h"
#include <QDateTime>
#include <QDebug>

namespace {
    double getDuration(const AVFrmItem &last, const AVFrmItem &now) {
        double maxFrameDuration = GlobalClock::instance().maxFrameDuration();
        double duration = now.pts - last.pts;
        if (std::isnan(duration) || duration <= 0 || duration > maxFrameDuration)
            duration = std::isnan(last.duration) ? 0.0 : last.duration;
        return duration;
    }
}

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject{parent} {}

VideoPlayer::~VideoPlayer() {
    uninit();
}

bool VideoPlayer::init(sharedFrmQueue frmBuf) {
    if (m_initialized) {
        uninit();
    }
    m_isSeeking = false;
    m_frmBuf = frmBuf;
    renderData1.reset();
    renderData2.reset();
    renderTime = std::numeric_limits<double>::quiet_NaN();
    renderData = &renderData1;
    return true;
}

bool VideoPlayer::uninit() {
    stop();

    m_frmBuf.reset();
    renderData1.reset();
    renderData2.reset();
    emit renderDataReady(nullptr);
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
        renderTime = getRelativeSeconds();
    }
    m_paused.store(!paused, std::memory_order_release);
}

double d0, d1, d2, d3, d4, d5;
void VideoPlayer::playerLoop() {
    // 确保音视频设备都完成了基本初始化
    while (!DeviceStatus::instance().initialized() && !m_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    AVFrmItem frmItem;
    while (!m_stop.load(std::memory_order_relaxed)) {
        d0 = getRelativeSeconds();
        // 进行seek
        AVFrmItem refFrm;
        if (!m_frmBuf->peekFirst(refFrm)) { // 空的
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        int seekCnt = GlobalClock::instance().seekCnt();
        if (refFrm.seekCnt != seekCnt) {
            Q_ASSERT(frmItem.frm == nullptr);
            m_isSeeking = true;
            m_frmBuf->pop(frmItem);
            av_frame_free(&frmItem.frm);
            continue;
        }

        if (!m_isSeeking && m_paused.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 读frm
        m_frmBuf->pop(frmItem);

        d1 = getRelativeSeconds();

        // 写如入设备
        write(frmItem);

        if (m_isSeeking && GlobalClock::instance().mainClockType() == ClockType::VIDEO) {
            emit seeked();
        }
        m_isSeeking = false;
    }
    return;
}

bool VideoPlayer::write(AVFrmItem &item) {
    if (!item.frm) {
        return false;
    }

    // 在渲染之前准备好数据
    RenderData *tmpPtr = renderData == &renderData1 ? &renderData2 : &renderData1;
    // Qt信号会使用事件队列，可能导致渲染线程使用的renderData与tmpPtr是同一个
    if (!tmpPtr->mutex.try_lock_for(std::chrono::milliseconds(1))) {
        std::swap(tmpPtr, renderData);
        tmpPtr->mutex.lock();
    }
    tmpPtr->updateFormat(item);
    tmpPtr->mutex.unlock();

    d2 = getRelativeSeconds();

    // 上一帧持续时间
    double delay = getDuration(renderData->frmItem, item);

    static int lessCt = 0, moreCt = 0, loseCt = 0;
    // 同步主时钟
    double maxFrameDuration = GlobalClock::instance().maxFrameDuration();
    double diff = GlobalClock::instance().videoPts() - GlobalClock::instance().getMainPts();
    // //[0.004 - 1/fps -  0.01]与主时钟差距保证在范围外需要同步 //NOTE： ffplay用的[0.04-0.1]
    double syncThreshold = std::max(0.004, std::min(0.01, delay));
    if (!std::isnan(diff) && std::abs(diff) < maxFrameDuration) {
        if (diff <= -syncThreshold) { // 落后太多，尝试直接播放
            lessCt++;
            delay = std::max(0.0, delay + diff);
        } else if (diff >= syncThreshold) { // 领先太多
            moreCt++;
            delay = delay + (delay > 0.1 ? diff : delay);
        }
    }

    if (std::isnan(renderTime) || m_isSeeking) {
        renderTime = getRelativeSeconds();
    } else {
        renderTime += delay;
    }

    double nowTime = getRelativeSeconds();
    double dt = renderTime - nowTime;

    d3 = getRelativeSeconds();

    if (dt > 0) {
        if (dt > 0.1) {
            dt = 0.1;
            renderTime = nowTime + dt;
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }

    d4 = getRelativeSeconds();

    AVFrmItem tmpItem;
    bool peekOk = m_frmBuf->peekFirst(tmpItem);
    renderData = tmpPtr;
    nowTime = getRelativeSeconds();
    if (!peekOk || nowTime <= renderTime + getDuration(item, tmpItem)) {
        emit renderDataReady(renderData);
    } else {
        loseCt++;
    }

    // 更新视频时钟
    // qDebug() << "v:" << pts << GlobalClock::instance().videoPts();
    GlobalClock::instance().setVideoClk(item.pts);
    GlobalClock::instance().syncExternalClk(ClockType::VIDEO);

    d5 = getRelativeSeconds();

    static int ct = 0;
    if ((int)nowTime != ct) {
        ct = (int)nowTime;
        qDebug() << "sleep:" << dt
                 << "delay:" << delay;
        qDebug() << (d1 - d0) * 1e6 << (d2 - d1) * 1e6 << (d3 - d2) * 1e6 << (d4 - d3) * 1e6 << (d5 - d4) * 1e6;
        qDebug() << "视频调用间隔" << d5 - d0;
        qDebug() << "lessCt:" << lessCt << "moreCt:" << moreCt << "loseCt:" << loseCt;
        qDebug() << "AVdiff1" << GlobalClock::instance().videoPts() - GlobalClock::instance().getMainPts();
    }
    return true;
}

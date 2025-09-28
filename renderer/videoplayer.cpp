#include "videoplayer.h"
#include <QDateTime>
#include <QDebug>
#include <QThread>

VideoPlayer::VideoPlayer(sharedFrmQueue frmBuf, VideoWindow *videoWindow, QObject *parent)
    : m_frmBuf(frmBuf), renderTime(std::numeric_limits<double>::quiet_NaN()), m_videoWindow(videoWindow) {
    renderData = &renderData1;
}

VideoPlayer::~VideoPlayer() {
}

void VideoPlayer::startPlay() {
    m_stop.store(false, std::memory_order_relaxed);
    AVFrmItem frmItem;
    while (!m_stop.load(std::memory_order_relaxed)) {
        // 读frm
        while (!m_frmBuf->pop(frmItem)) {
            while (m_stop.load(std::memory_order_relaxed)) {
                goto end;
            }
            QThread::msleep(5);
        }
        // 写如入设备
        write(&frmItem);
    }
end:
    emit finished();
}

bool VideoPlayer::write(AVFrmItem *item) {
    if (!item || !item->frm) {
        return false;
    }

    // 在渲染之前准备好数据
    RenderData *tmpPtr = renderData == &renderData1 ? &renderData2 : &renderData1;
    // QMetaObject::invokeMethod会使用事件队列，可能导致渲染线程使用的renderData与tmpPtr是同一个
    if (!tmpPtr->mutex.try_lock_for(std::chrono::milliseconds(1))) {
        std::swap(tmpPtr, renderData);
        tmpPtr->mutex.lock();
    }
    tmpPtr->updateFormat(item);
    double pts = tmpPtr->pts;
    tmpPtr->mutex.unlock();

    // 当前帧持续时间（还未渲染）
    double maxFrameDuration = GlobalClock::instance().maxFrameDuration();
    double duration = tmpPtr->pts - renderData->pts;
    if (std::isnan(duration) || duration <= 0 || duration > maxFrameDuration)
        duration = tmpPtr->frm->duration;

    // 同步主时钟
    double diff = GlobalClock::instance().videoPts() - GlobalClock::instance().getMainPts();
    // qDebug() << "AVdiff0" << diff;
    //[0.004 - 1/fps -  0.01]与主时钟差距保证在范围外需要同步 //NOTE： ffplay用的[0.04-0.1]
    double syncThreshold = std::max(0.004, std::min(0.01, duration));
    if (!std::isnan(diff) && std::abs(diff) < maxFrameDuration) {
        if (diff < -syncThreshold) { // 落后太多，直接播放
            duration = 0;
            qDebug() << "----------------------shao";
        } else if (diff > syncThreshold) { // 领先太多
            duration += diff;
            qDebug() << "----------------------duo";
        }
    }

    if (duration <= 0 || std::isnan(renderTime)) {
        renderTime = getRelativeSeconds();
    } else {
        renderTime = renderTime + duration;
    }

    double nowTime = getRelativeSeconds();
    if (nowTime < renderTime) {
        QThread::msleep((renderTime - nowTime) * 1000);
    }

    static int ct = 0;
    static auto st = getRelativeSeconds();
    auto nQT = getRelativeSeconds();
    double tmt = GlobalClock::instance().audioPts() * 1000;
    double ext = GlobalClock::instance().externalPts() * 1000;
    double vid = GlobalClock::instance().videoPts() * 1000;
    if ((int)nQT != ct) {
        // qDebug() << tmt << vid << ext << nQT - st << tmt - nQT + st << vid - nQT + st << ext - nQT + st;
        ct = (int)nQT;
        qDebug() << "sleep:" << (renderTime - nowTime) * 1000;
        qDebug() << "AVdiff1" << GlobalClock::instance().videoPts() - GlobalClock::instance().getMainPts();
    }

    if (m_videoWindow) {
        renderData = tmpPtr;
        QMetaObject::invokeMethod(m_videoWindow, "updateRenderData", Qt::QueuedConnection, Q_ARG(RenderData *, renderData));
    }

    // 更新视频时钟
    // qDebug() << "v:" << pts << GlobalClock::instance().videoPts();
    GlobalClock::instance().setVideoClk(pts);
    GlobalClock::instance().syncExternalClk(GlobalClock::instance().videoClk());

    return true;
}

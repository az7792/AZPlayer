#include "mediacontroller.h"
#include "clock/globalclock.h"
#include "renderer/videorenderer.h"
#include <QFileInfo>

namespace {
    void clearPktQ(sharedPktQueue pktq) {
        if (!pktq) {
            return;
        }
        AVPktItem tmp;
        while (pktq->pop(tmp)) {
            av_packet_free(&tmp.pkt);
        }
    }
    void clearFrmQ(sharedFrmQueue frmq) {
        if (!frmq) {
            return;
        }
        AVFrmItem tmp;
        while (frmq->pop(tmp)) {
            av_frame_free(&tmp.frm);
        }
    }
}

MediaController::MediaController(QObject *parent)
    : QObject{parent} {

    m_demux = new Demux(parent);
    m_decodeAudio = new DecodeAudio(parent);
    m_decodeVideo = new DecodeVideo(parent);

    m_audioPlayer = new AudioPlayer(parent);
    m_videoPlayer = new VideoPlayer(parent);

    QObject::connect(&m_timer, &QTimer::timeout, this, [&]() {
        updateCurrentTimeWork();
    });

    // DEBUG:start
    static QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, this, [&]() {
        qDebug() << "AudioPkt:" << m_pktAudioBuf->size() << "VideoPkt:" << m_pktVideoBuf->size();
        qDebug() << "AudioFrm:" << m_frmAudioBuf->size() << "VideoFrm:" << m_frmVideoBuf->size() << "\n";
    });
    timer.start(1000); // 每1000ms触发一次
    // DEBUG: end
}

bool MediaController::setVideoWindow(QObject *videoWindow) {
    VideoWindow *wd = static_cast<VideoWindow *>(videoWindow);
    QObject::connect(m_videoPlayer, &VideoPlayer::renderDataReady,
                     wd, &VideoWindow::updateRenderData,
                     Qt::QueuedConnection);
    return true;
}

bool MediaController::open(const QUrl &URL) {
    QString localFile = URL.toLocalFile();
    if (!QFileInfo::exists(localFile)) {
        qDebug() << "无效路径:" << localFile;
        return false;
    }

    if (m_opened) {
        close();
    }

    bool ok = true;
    ok &= m_demux->init(localFile.toUtf8().constData(), m_pktAudioBuf, m_pktVideoBuf, nullptr);
    if (!ok) {
        close();
        return false;
    }

    bool haveAudio = false, haveVideo = false;
    if (m_demux->getAudioStream()) {
        ok &= m_decodeAudio->init(m_demux->getAudioStream(), m_pktAudioBuf, m_frmAudioBuf);
        ok &= m_audioPlayer->init(m_demux->getAudioStream()->codecpar, m_frmAudioBuf);
        haveAudio = true;
    }
    if (m_demux->getVideoStream()) {
        ok &= m_decodeVideo->init(m_demux->getVideoStream(), m_pktVideoBuf, m_frmVideoBuf);
        haveVideo = true;
    }

    DeviceStatus::instance().setHaveAudio(haveAudio);
    DeviceStatus::instance().setHaveVideo(haveVideo);

    if (!haveAudio && !haveVideo) {
        qDebug() << "文件不包含视频和音频";
        close();
        return false;
    } else if (!haveAudio && haveVideo) {
        GlobalClock::instance().setMainClockType(ClockType::VIDEO);
    }

    ok &= m_videoPlayer->init(m_frmVideoBuf);
    if (!ok) {
        close();
        return false;
    }

    setDuration(m_demux->getDuration());
    GlobalClock::instance().reset();
    m_audioPlayer->setVolume(m_muted ? 0.0 : m_volume); // start之前设置好

    m_demux->start();
    m_decodeAudio->start();
    m_decodeVideo->start();
    m_audioPlayer->start();
    m_videoPlayer->start();
    m_timer.start(500);

    m_opened = true;
    setPaused(false);
    qDebug() << "打开成功";
    return true;
}

bool MediaController::close() {
    if (!m_opened) {
        return true;
    }
    bool ok = true;
    m_timer.stop();
    ok &= m_demux->uninit();
    ok &= m_decodeAudio->uninit();
    ok &= m_decodeVideo->uninit();
    ok &= m_audioPlayer->uninit();
    ok &= m_videoPlayer->uninit();
    clearPktQ(m_pktAudioBuf);
    clearPktQ(m_pktVideoBuf);
    clearFrmQ(m_frmAudioBuf);
    clearFrmQ(m_frmVideoBuf);
    DeviceStatus::instance().setHaveAudio(false);
    DeviceStatus::instance().setHaveVideo(false);
    m_opened = false;
    setPaused(true);
    setDuration(0);
    setCurrentTime(0);
    qDebug() << "关闭";
    return ok;
}

void MediaController::togglePaused() {
    if (!m_opened) {
        return;
    }
    m_videoPlayer->togglePaused();
    m_audioPlayer->togglePaused();
    GlobalClock::instance().togglePaused();
    setPaused(!m_paused);
}

void MediaController::toggleMuted() {
    // 不需要用setVolume(x);
    if (m_muted) {
        m_audioPlayer->setVolume(m_volume);
    } else {
        m_audioPlayer->setVolume(0.0);
    }
    setMuted(!m_muted);
}

bool MediaController::muted() const {
    return m_muted;
}

void MediaController::setMuted(bool newMuted) {
    if (m_muted == newMuted)
        return;
    m_muted = newMuted;
    emit mutedChanged();
}

double MediaController::volume() const {
    return m_volume;
}

void MediaController::setVolume(double newVolume) {
    if (qFuzzyCompare(m_volume, newVolume))
        return;
    m_audioPlayer->setVolume(newVolume);
    m_volume = newVolume;
    setMuted(false);
    emit volumeChanged();
}

void MediaController::updateCurrentTimeWork() {
    double t = GlobalClock::instance().getMainPts();
    if (std::isnan(t))
        setCurrentTime(0);
    else
        setCurrentTime(t);
}

int MediaController::currentTime() const {
    return m_currentTime;
}

void MediaController::setCurrentTime(int newCurrentTime) {
    if (m_currentTime == newCurrentTime)
        return;
    m_currentTime = newCurrentTime;
    emit currentTimeChanged();
}

int MediaController::duration() const {
    return m_duration;
}

void MediaController::setDuration(int newDuration) {
    if (m_duration == newDuration)
        return;
    m_duration = newDuration;
    emit durationChanged();
}

bool MediaController::paused() const {
    return m_paused;
}

void MediaController::setPaused(bool newPaused) {
    if (m_paused == newPaused)
        return;
    m_paused = newPaused;
    emit pausedChanged();
}

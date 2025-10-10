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
    m_decodeSubtitl = new DecodeSubtitle(parent);

    m_audioPlayer = new AudioPlayer(parent);
    m_videoPlayer = new VideoPlayer(parent);

    QObject::connect(m_audioPlayer, &AudioPlayer::seeked, this, &MediaController::seeked);
    QObject::connect(m_videoPlayer, &VideoPlayer::seeked, this, &MediaController::seeked);

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
    ok &= m_demux->init(localFile.toUtf8().constData(), m_pktAudioBuf, m_pktVideoBuf, m_pktSubtitleBuf);
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
    if (m_demux->getSubtitleStream()) {
        ok &= m_decodeSubtitl->init(m_demux->getSubtitleStream(), m_pktSubtitleBuf, m_frmSubtitleBuf);
    }

    DeviceStatus::instance().setHaveAudio(haveAudio);
    DeviceStatus::instance().setHaveVideo(haveVideo);

    setDuration(m_demux->getDuration()); // 总时长
    GlobalClock::instance().reset();
    GlobalClock::instance().setMainClockType(ClockType::AUDIO); // 默认
    m_audioPlayer->setVolume(m_muted ? 0.0 : m_volume);         // start之前设置好

    if (!haveAudio && !haveVideo) {
        qDebug() << "文件不包含视频和音频";
        close();
        return false;
    } else if (!haveAudio && haveVideo) {
        GlobalClock::instance().setMainClockType(ClockType::VIDEO);
    }

    ok &= m_videoPlayer->init(m_frmVideoBuf, m_frmSubtitleBuf);
    if (!ok) {
        close();
        return false;
    }

    m_demux->start();
    m_decodeAudio->start();
    m_decodeVideo->start();
    m_decodeSubtitl->start();
    m_audioPlayer->start();
    m_videoPlayer->start();

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
    ok &= m_demux->uninit();
    ok &= m_decodeAudio->uninit();
    ok &= m_decodeVideo->uninit();
    ok &= m_decodeSubtitl->uninit();
    ok &= m_audioPlayer->uninit();
    ok &= m_videoPlayer->uninit();
    clearPktQ(m_pktAudioBuf);
    clearPktQ(m_pktVideoBuf);
    clearPktQ(m_pktSubtitleBuf);
    clearFrmQ(m_frmAudioBuf);
    clearFrmQ(m_frmVideoBuf);
    clearFrmQ(m_frmSubtitleBuf);
    DeviceStatus::instance().setHaveAudio(false);
    DeviceStatus::instance().setHaveVideo(false);
    GlobalClock::instance().reset();
    m_opened = false;
    setPaused(true);
    setDuration(0);
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

void MediaController::seekBySec(double ts, double rel) {
    if (!m_opened)
        return;
    m_demux->seekBySec(ts, rel);
}

void MediaController::fastForward() {
    seekBySec(std::min((double)m_duration, getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

int MediaController::getCurrentTime() const {
    double ptsSecond = GlobalClock::instance().getMainPts();
    return std::isnan(ptsSecond) ? 0 : ptsSecond;
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

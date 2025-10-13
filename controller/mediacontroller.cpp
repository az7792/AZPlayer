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

    for (int i = 0; i < 3; ++i) {
        m_demuxs[i] = new Demux(parent);
    }

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
        qDebug() << "AudioPkt:" << m_pktAudioBuf->size() << "VideoPkt:" << m_pktVideoBuf->size() << "SubtitlePkt:" << m_pktSubtitleBuf->size();
        qDebug() << "AudioFrm:" << m_frmAudioBuf->size() << "VideoFrm:" << m_frmVideoBuf->size() << "SubtitleFrm:" << m_frmSubtitleBuf->size() << "\n";
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

    static constexpr int defDemuxIdx = 0;

    bool ok = true;
    ok &= m_demuxs[defDemuxIdx]->init(localFile.toUtf8().constData());
    if (!ok) {
        close();
        return false;
    }

    bool haveAudio = m_demuxs[defDemuxIdx]->haveAudioStream(), haveVideo = m_demuxs[defDemuxIdx]->haveVideoStream();
    if (haveAudio) {
        const int audioIdx = 0;
        m_demuxs[defDemuxIdx]->switchAudioStream(audioIdx, m_pktAudioBuf, m_frmAudioBuf);
        m_streams[MediaIdx::AUDIO] = {defDemuxIdx, audioIdx};
        ok &= m_decodeAudio->init(m_demuxs[defDemuxIdx]->getAudioStream(), m_pktAudioBuf, m_frmAudioBuf);
        ok &= m_audioPlayer->init(m_demuxs[defDemuxIdx]->getAudioStream()->codecpar, m_frmAudioBuf);
    }

    if (haveVideo) {
        const int videoIdx = 0;
        m_streams[MediaIdx::VIDEO] = {defDemuxIdx, videoIdx};
        m_demuxs[defDemuxIdx]->switchVideoStream(videoIdx, m_pktVideoBuf, m_frmVideoBuf);
        ok &= m_decodeVideo->init(m_demuxs[defDemuxIdx]->getVideoStream(), m_pktVideoBuf, m_frmVideoBuf);
    }

    if (m_demuxs[defDemuxIdx]->haveSubtitleStream()) {
        const int subtitleIdx = 0;
        m_streams[MediaIdx::SUBTITLE] = {defDemuxIdx, subtitleIdx};
        m_demuxs[defDemuxIdx]->switchSubtitleStream(subtitleIdx, m_pktSubtitleBuf, m_frmSubtitleBuf);
        ok &= m_decodeSubtitl->init(m_demuxs[defDemuxIdx]->getSubtitleStream(), m_pktSubtitleBuf, m_frmSubtitleBuf);
    }

    DeviceStatus::instance().setHaveAudio(haveAudio);
    DeviceStatus::instance().setHaveVideo(haveVideo);

    setDuration(m_demuxs[defDemuxIdx]->getDuration()); // 总时长
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

    m_demuxs[defDemuxIdx]->start();
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
    for (int i = 0; i < 3; ++i) {
        if (m_demuxs[i]) {
            ok &= m_demuxs[i]->uninit();
        }
    }
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
    m_streams.fill({-1, -1});
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            m_streamInfo[i][j].clear();
    }
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
    m_demuxs[0]->seekBySec(ts, rel);
}

void MediaController::fastForward() {
    seekBySec(std::min((double)m_duration, getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

bool MediaController::switchSubtitleStream(int demuxIdx, int streamIdx) {
    if (m_streams[MediaIdx::SUBTITLE] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    m_demuxs[m_streams[MediaIdx::SUBTITLE].first]->closeStream(MediaIdx::SUBTITLE);
    m_decodeSubtitl->uninit();
    m_videoPlayer->forceRefreshSubtitle(); // 切换后需要一定时间才会解码到新字幕，因此提前强制清掉旧字幕

    clearPktQ(m_pktSubtitleBuf);
    m_pktSubtitleBuf->addSerial();
    m_frmSubtitleBuf->addSerial(); // videoPlayer可能还在使用，不需要清空，仅修改序号即可
    // 打开新的
    m_demuxs[demuxIdx]->switchSubtitleStream(streamIdx, m_pktSubtitleBuf, m_frmSubtitleBuf);
    m_decodeSubtitl->init(m_demuxs[demuxIdx]->getSubtitleStream(), m_pktSubtitleBuf, m_frmSubtitleBuf);
    m_decodeSubtitl->start();

    m_streams[MediaIdx::SUBTITLE] = {demuxIdx, streamIdx}; // 更新

    return true;
}

bool MediaController::switchAudioStream(int demuxIdx, int streamIdx) {
    if (m_streams[MediaIdx::AUDIO] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    m_demuxs[m_streams[MediaIdx::AUDIO].first]->closeStream(MediaIdx::AUDIO);
    m_decodeAudio->uninit();
    m_audioPlayer->uninit(); // 音频设备需要重新设置

    clearPktQ(m_pktAudioBuf);
    clearFrmQ(m_frmAudioBuf);
    m_pktAudioBuf->addSerial();
    m_frmAudioBuf->addSerial();

    // 打开新的
    m_demuxs[demuxIdx]->switchAudioStream(streamIdx, m_pktAudioBuf, m_frmAudioBuf);
    m_decodeAudio->init(m_demuxs[demuxIdx]->getAudioStream(), m_pktAudioBuf, m_frmAudioBuf);
    m_audioPlayer->init(m_demuxs[demuxIdx]->getAudioStream()->codecpar, m_frmAudioBuf);
    m_decodeAudio->start();
    m_audioPlayer->start();

    m_streams[MediaIdx::AUDIO] = {demuxIdx, streamIdx}; // 更新

    return true;
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

// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

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

    m_streams.fill({-1, -1});

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
    QObject::connect(this, &MediaController::seeked, this, [&]() {
        m_played = false;
    });

    // DEBUG:start
    static QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, this, [&]() {
        qDebug() << "AudioPkt:" << m_pktAudioBuf->size() << "VideoPkt:" << m_pktVideoBuf->size() << "SubtitlePkt:" << m_pktSubtitleBuf->size();
        qDebug() << "AudioFrm:" << m_frmAudioBuf->size() << "VideoFrm:" << m_frmVideoBuf->size() << "SubtitleFrm:" << m_frmSubtitleBuf->size() << "\n";
    });
    timer.start(3000); // 每1000ms触发一次
    // DEBUG: end

    // 定时判断是否播完
    QObject::connect(&m_timer, &QTimer::timeout, this, [&]() {
        if (m_opened && !m_played && getCurrentTime() > m_duration) {
            if (m_loopOnEnd) {
                seekBySec(0.0, 0.0);
            } else {
                togglePaused();
                m_played = true;
                emit played();
            }
        }
    });
    m_timer.start(100);
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

    if (haveVideo && m_demuxs[defDemuxIdx]->haveSubtitleStream()) {
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
    m_played = false;
    qDebug() << "打开成功";
    emit streamInfoUpdate();
    return true;
}

bool MediaController::openSubtitleStream(const QUrl &URL) {
    return openStreamByFile(URL, MediaIdx::SUBTITLE);
}

bool MediaController::openAudioStream(const QUrl &URL) {
    return openStreamByFile(URL, MediaIdx::AUDIO);
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
    setPaused(true);
    setDuration(0);
    qDebug() << "关闭";
    emit streamInfoUpdate();
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

void MediaController::addVolum() {
    setVolume(std::min(1.0, m_volume + 0.04));
}

void MediaController::subVolum() {
    setVolume(std::max(0.0, m_volume - 0.04));
}

void MediaController::seekBySec(double ts, double rel) {
    if (!m_opened)
        return;
    // HACK: 视频和音频(字幕)实际seek到的位置并不一样
    for (int i = 0; i < 3; ++i) {
        if (m_demuxs[i])
            m_demuxs[i]->seekBySec(ts, rel);
    }
}

void MediaController::fastForward() {
    seekBySec(std::min((double)m_duration, getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

bool MediaController::switchSubtitleStream(int demuxIdx, int streamIdx) {
    if (m_streams[MediaIdx::VIDEO].first < 0 || m_streams[MediaIdx::VIDEO].first < 0)
        return false;

    if (m_streams[MediaIdx::SUBTITLE] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    if (m_streams[MediaIdx::SUBTITLE].first != -1)
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
    if(m_streams[MediaIdx::AUDIO].first != -1)
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

    // 由于音频设备启动非常耗时，因此可以先把视频关了再开，这样可以利用DeviceStatus同步设备启动时间
    m_videoPlayer->stop();
    m_videoPlayer->start();

    m_streams[MediaIdx::AUDIO] = {demuxIdx, streamIdx}; // 更新

    return true;
}

bool MediaController::loopOnEnd() const { return m_loopOnEnd; }

void MediaController::setLoopOnEnd(bool newLoopOnEnd) { m_loopOnEnd = newLoopOnEnd; }

bool MediaController::opened() const {
    return m_opened;
}

void MediaController::setOpened(bool newOpened) {
    if (m_opened == newOpened)
        return;
    m_opened = newOpened;
    emit openedChanged();
}

QVariantList MediaController::getStreamInfo(MediaIdx type) const {
    QVariantList list;
    for (size_t i = 0; i < m_demuxs.size(); ++i) {
        Demux *demux = m_demuxs[i];
        if (demux == nullptr)
            continue;

        const auto &v = demux->streamInfo()[type];
        for (size_t j = 0; j < v.size(); ++j) {
            QVariantMap item;
            item["text"] = v[j];     // 流的文本描述
            item["demuxIdx"] = i;    // 属于哪个解复用器
            item["mediaIdx"] = type; // 属于那种流
            item["streamIdx"] = j;   // 第几个流（stream[j]才是实际formatCtx里用的）
            list.append(item);
        }
    }
    return list;
}

bool MediaController::openStreamByFile(const QUrl &URL, MediaIdx idx) {
    if (!m_opened) {
        return false;
    }

    QString localFile = URL.toLocalFile();
    if (!QFileInfo::exists(localFile)) {
        qDebug() << "无效路径:" << localFile;
        return false;
    }

    m_demuxs[idx]->uninit();
    if (m_streams[MediaIdx::AUDIO].first == idx) {
        m_pktAudioBuf->clear();
        m_streams[MediaIdx::AUDIO] = {-1, -1};
    }
    if (m_streams[MediaIdx::SUBTITLE].first == idx) {
        m_pktSubtitleBuf->clear();
        m_streams[MediaIdx::SUBTITLE] = {-1, -1};
    }

    bool ok = true;
    ok &= m_demuxs[idx]->init(localFile.toUtf8().constData());
    if (!ok) {
        close();
        return false;
    }
    emit streamInfoUpdate();
    return true;
}

int MediaController::getCurrentTime() const {
    double ptsSecond = GlobalClock::instance().getMainPts();
    return std::isnan(ptsSecond) ? 0 : ptsSecond;
}

QVariantList MediaController::getSubtitleInfo() const {
    return getStreamInfo(MediaIdx::SUBTITLE);
}

QVariantList MediaController::getAudioInfo() const {
    return getStreamInfo(MediaIdx::AUDIO);
}

int MediaController::getSubtitleIdx() const {
    if (m_streams[MediaIdx::SUBTITLE].first == -1)
        return -1;
    int idx = 0;
    for (int i = 0; i < m_streams[MediaIdx::SUBTITLE].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[MediaIdx::SUBTITLE];
    }
    return idx + m_streams[MediaIdx::SUBTITLE].second;
}

int MediaController::getAudioIdx() const {
    if (m_streams[MediaIdx::AUDIO].first == -1)
        return -1;
    int idx = 0;
    for (int i = 0; i < m_streams[MediaIdx::AUDIO].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[MediaIdx::AUDIO];
    }
    return idx + m_streams[MediaIdx::AUDIO].second;
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

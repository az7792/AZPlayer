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
    // 如果是多个解复用器同时seek，需要使用第一个解复用器seek到的实际位置来seek剩余的解复用器
    QObject::connect(m_demuxs[0], &Demux::seeked, this, &MediaController::seekAudioAndSubtitleDemux);
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
    ok &= m_demuxs[defDemuxIdx]->init(localFile.toUtf8().constData(), true);
    if (!ok) {
        close();
        return false;
    }

    bool haveAudio = m_demuxs[defDemuxIdx]->haveStream(MediaType::Audio), haveVideo = m_demuxs[defDemuxIdx]->haveStream(MediaType::Video);
    if (haveAudio) {
        const int audioIdx = 0;
        m_demuxs[defDemuxIdx]->switchAudioStream(audioIdx, m_pktAudioBuf, m_frmAudioBuf);
        m_streams[to_index(MediaIdx::Audio)] = {defDemuxIdx, audioIdx};
        ok &= m_decodeAudio->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf);
        ok &= m_audioPlayer->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf);
    }

    if (haveVideo) {
        const int videoIdx = 0;
        m_streams[to_index(MediaIdx::Video)] = {defDemuxIdx, videoIdx};
        m_demuxs[defDemuxIdx]->switchVideoStream(videoIdx, m_pktVideoBuf, m_frmVideoBuf);
        ok &= m_decodeVideo->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Video), m_pktVideoBuf, m_frmVideoBuf);
    }

    if (haveVideo && m_demuxs[defDemuxIdx]->haveStream(MediaType::Subtitle)) {
        const int subtitleIdx = 0;
        m_streams[to_index(MediaIdx::Subtitle)] = {defDemuxIdx, subtitleIdx};
        bool isAssSub;
        m_demuxs[defDemuxIdx]->switchSubtitleStream(subtitleIdx, m_pktSubtitleBuf, m_frmSubtitleBuf, isAssSub);
        if (!isAssSub)
            ok &= m_decodeSubtitl->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf);
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
    emit chaptersInfoUpdate();
    return true;
}

bool MediaController::openSubtitleStream(const QUrl &URL) {
    return openStreamByFile(URL, MediaIdx::Subtitle);
}

bool MediaController::openAudioStream(const QUrl &URL) {
    return openStreamByFile(URL, MediaIdx::Audio);
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
    ASSRender::instance().uninit();
    m_opened = false;
    m_streams.fill({-1, -1});
    setPaused(true);
    setDuration(0);
    qDebug() << "关闭";
    emit streamInfoUpdate();
    emit chaptersInfoUpdate();
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

    m_demuxs[0]->seekBySec(ts, rel);
    // NOTE: 另外两个解复用器需要等拥有视频的解复用器seek完成后再进行seek，这儿是通过信号的方式触发另外两个解复用器seek的
}

void MediaController::fastForward() {
    seekBySec(std::min((double)m_duration, getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

bool MediaController::switchSubtitleStream(int demuxIdx, int streamIdx) {
    if (m_streams[to_index(MediaIdx::Video)].first < 0 || m_streams[to_index(MediaIdx::Video)].first < 0)
        return false;

    if (m_streams[to_index(MediaIdx::Subtitle)] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    if (m_streams[to_index(MediaIdx::Subtitle)].first != -1)
        m_demuxs[m_streams[to_index(MediaIdx::Subtitle)].first]->closeStream(MediaIdx::Subtitle);
    m_decodeSubtitl->uninit();
    ASSRender::instance().uninit();
    m_videoPlayer->forceRefreshSubtitle(); // 切换后需要一定时间才会解码到新字幕，因此提前强制清掉旧字幕

    clearPktQ(m_pktSubtitleBuf);
    m_pktSubtitleBuf->addSerial();
    m_frmSubtitleBuf->addSerial(); // videoPlayer可能还在使用，不需要清空，仅修改序号即可
    // 打开新的
    bool isAssSub;
    m_demuxs[demuxIdx]->switchSubtitleStream(streamIdx, m_pktSubtitleBuf, m_frmSubtitleBuf, isAssSub);
    if (!isAssSub) {
        m_decodeSubtitl->init(m_demuxs[demuxIdx]->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf);
        m_decodeSubtitl->start();
    }

    m_streams[to_index(MediaIdx::Subtitle)] = {demuxIdx, streamIdx}; // 更新

    return true;
}

bool MediaController::switchAudioStream(int demuxIdx, int streamIdx) {
    if (m_streams[to_index(MediaIdx::Audio)] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    if (m_streams[to_index(MediaIdx::Audio)].first != -1)
        m_demuxs[m_streams[to_index(MediaIdx::Audio)].first]->closeStream(MediaIdx::Audio);
    m_decodeAudio->uninit();
    m_audioPlayer->uninit(); // 音频设备需要重新设置

    clearPktQ(m_pktAudioBuf);
    clearFrmQ(m_frmAudioBuf);
    m_pktAudioBuf->addSerial();
    m_frmAudioBuf->addSerial();

    // 打开新的
    m_demuxs[demuxIdx]->switchAudioStream(streamIdx, m_pktAudioBuf, m_frmAudioBuf);
    m_decodeAudio->init(m_demuxs[demuxIdx]->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf);
    m_audioPlayer->init(m_demuxs[demuxIdx]->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf);
    m_decodeAudio->start();
    m_audioPlayer->start();

    // 由于音频设备启动非常耗时，因此可以先把视频关了再开，这样可以利用DeviceStatus同步设备启动时间
    m_videoPlayer->stop();
    m_videoPlayer->start();

    m_streams[to_index(MediaIdx::Audio)] = {demuxIdx, streamIdx}; // 更新

    return true;
}

bool MediaController::loopOnEnd() const { return m_loopOnEnd; }

void MediaController::setLoopOnEnd(bool newLoopOnEnd) { m_loopOnEnd = newLoopOnEnd; }

bool MediaController::opened() const { return m_opened; }

void MediaController::setOpened(bool newOpened) {
    if (m_opened == newOpened)
        return;
    m_opened = newOpened;
    emit openedChanged();
}

QVariantList MediaController::getStreamInfo(MediaIdx type) const {
    auto index = to_index(type);
    QVariantList list;
    for (size_t i = 0; i < m_demuxs.size(); ++i) {
        Demux *demux = m_demuxs[i];
        if (demux == nullptr)
            continue;

        const auto &v = demux->streamInfo()[index];
        for (size_t j = 0; j < v.size(); ++j) {
            QVariantMap item;
            item["text"] = v[j];      // 流的文本描述
            item["demuxIdx"] = i;     // 属于哪个解复用器
            item["mediaIdx"] = index; // 属于那种流
            item["streamIdx"] = j;    // 第几个流（stream[j]才是实际formatCtx里用的）
            list.append(item);
        }
    }
    return list;
}

bool MediaController::openStreamByFile(const QUrl &URL, MediaIdx idx) {
    if (!m_opened) {
        return false;
    }
    uint8_t index = to_index(idx);
    QString localFile = URL.toLocalFile();
    if (!QFileInfo::exists(localFile)) {
        qDebug() << "无效路径:" << localFile;
        return false;
    }

    m_demuxs[index]->uninit();
    if (m_streams[to_index(MediaIdx::Audio)].first == index) {
        m_pktAudioBuf->clear();
        m_streams[to_index(MediaIdx::Audio)] = {-1, -1};
    }
    if (m_streams[to_index(MediaIdx::Subtitle)].first == index) {
        m_pktSubtitleBuf->clear();
        m_streams[to_index(MediaIdx::Subtitle)] = {-1, -1};
    }

    bool ok = true;
    ok &= m_demuxs[index]->init(localFile.toUtf8().constData());
    if (!ok) {
        close();
        return false;
    }
    emit streamInfoUpdate();
    return true;
}

bool MediaController::seekAudioAndSubtitleDemux(double pts) {
    m_demuxs[1]->seekBySec(pts, 0.0);
    m_demuxs[2]->seekBySec(pts, 0.0);
    return true;
}

int MediaController::getCurrentTime() const {
    double ptsSecond = GlobalClock::instance().getMainPts();
    return std::isnan(ptsSecond) ? 0 : ptsSecond;
}

QVariantList MediaController::getSubtitleInfo() const {
    return getStreamInfo(MediaIdx::Subtitle);
}

QVariantList MediaController::getAudioInfo() const {
    return getStreamInfo(MediaIdx::Audio);
}

QVariantList MediaController::getChaptersInfo() const {
    Demux *demux = m_demuxs[to_index(MediaIdx::Video)];
    QVariantList list;

    if (demux == nullptr)
        return list;

    const std::vector<ChapterInfo> &chapters = demux->chaptersInfo();
    for (auto &v : chapters) {
        int total = static_cast<int>(v.pts * 1000.0 + 0.5);

        int ms = total % 1000;
        total /= 1000;

        int sec = total % 60;
        total /= 60;

        int min = total % 60;
        int hour = total / 60;

        QString timeStr = QString("[%1:%2:%3:%4]")
                              .arg(hour, 2, 10, QLatin1Char('0'))
                              .arg(min, 2, 10, QLatin1Char('0'))
                              .arg(sec, 2, 10, QLatin1Char('0'))
                              .arg(ms, 3, 10, QLatin1Char('0'));

        QString title = QString::fromStdString(v.title);

        QVariantMap item;
        item["pts"] = QVariant(v.pts);
        item["text"] = timeStr + " " + title;

        list.push_back(item);
    }
    return list;
}

int MediaController::getSubtitleIdx() const {
    if (m_streams[to_index(MediaIdx::Subtitle)].first == -1)
        return -1;
    int idx = 0;
    for (int i = 0; i < m_streams[to_index(MediaIdx::Subtitle)].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[to_index(MediaIdx::Subtitle)];
    }
    return idx + m_streams[to_index(MediaIdx::Subtitle)].second;
}

int MediaController::getAudioIdx() const {
    if (m_streams[to_index(MediaIdx::Audio)].first == -1)
        return -1;
    int idx = 0;
    for (int i = 0; i < m_streams[to_index(MediaIdx::Audio)].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[to_index(MediaIdx::Audio)];
    }
    return idx + m_streams[to_index(MediaIdx::Audio)].second;
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

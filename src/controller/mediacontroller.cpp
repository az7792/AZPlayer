// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controller/mediacontroller.h"
#include <QFileInfo>
#include "clock/globalclock.h"
#include "renderer/videorenderer.h"
#include "stats/playbackstats.h"
#include "utils/episodeassetmanager.h"

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
    m_decodeSubtitle = new DecodeSubtitle(parent);

    m_audioPlayer = new AudioPlayer(parent);
    m_videoPlayer = new VideoPlayer(parent);

    // ==== seek ====
    QObject::connect(m_audioPlayer, &AudioPlayer::seeked, this, &MediaController::seeked);
    QObject::connect(m_videoPlayer, &VideoPlayer::seeked, this, &MediaController::seeked);
    // 如果是多个解复用器同时seek，需要使用第一个解复用器seek到的实际位置来seek剩余的解复用器
    QObject::connect(m_demuxs[0], &Demux::seeked, this, &MediaController::seekAudioAndSubtitleDemux);
    QObject::connect(this, &MediaController::seeked, this, [&]() {
        m_played = false;
    });

    // ==== 播放进度 ====
    QObject::connect(m_videoPlayer, &VideoPlayer::playedOneFrame, this, [&]() {
        setProgress(getCurrentTime());
        m_progressFallbackTimer.start(1000); // 重置定时器
    });
    QObject::connect(m_audioPlayer, &AudioPlayer::playedOneFrame, this, [&]() {
        setProgress(getCurrentTime());
        m_progressFallbackTimer.start(1000); // 重置定时器
    });

    // ==== Timer ====
    QObject::connect(&m_progressFallbackTimer, &QTimer::timeout, this, [&]() {
        setProgress(getCurrentTime());
    });
    m_progressFallbackTimer.start(1000); // 每1000ms触发一次

    // 更新队列长度
    QObject::connect(&m_updatePktAndFrmQueueSizeTimer, &QTimer::timeout, this, [&]() {
        PlaybackStats::instance().audioPacketCount = m_pktAudioBuf->size();
        PlaybackStats::instance().videoPacketCount = m_pktVideoBuf->size();
        PlaybackStats::instance().subtitlePacketCount = m_pktSubtitleBuf->size();

        PlaybackStats::instance().audioFrameCount = m_frmAudioBuf->size();
        PlaybackStats::instance().videoFrameCount = m_frmVideoBuf->size();
        PlaybackStats::instance().subtitleFrameCount = m_frmSubtitleBuf->size();
    });
    m_updatePktAndFrmQueueSizeTimer.start(1000); // 每1000ms触发一次

    // 定时判断是否播完
    QObject::connect(&m_checkPlayerFinishedTimer, &QTimer::timeout, this, &MediaController::checkPlayerFinished);
    m_checkPlayerFinishedTimer.start(100);
}

MediaController::~MediaController()
{
    m_checkPlayerFinishedTimer.stop();
    m_updatePktAndFrmQueueSizeTimer.stop();
    m_progressFallbackTimer.stop();
    close();
}

bool MediaController::setVideoWindow(QObject *videoWindow) {
    // HACK: 目前仅支持调用一次，多次调用不会清理旧连接
    static int ct = 0;
    ct++;
    Q_ASSERT(ct == 1);
    VideoWindow *wd = static_cast<VideoWindow *>(videoWindow);
    QObject::connect(m_videoPlayer, &VideoPlayer::renderDataReady,
                     wd, &VideoWindow::updateRenderData,
                     Qt::QueuedConnection);

    QObject::connect(this, &MediaController::clearVideoFBOSubtitleTex,
                     wd, &VideoWindow::forceClearSubtitle,
                     Qt::QueuedConnection);
    return true;
}

bool MediaController::open(const QUrl &URL) {
    const QString localFile = URL.toLocalFile();
    if (!QFileInfo::exists(localFile)) {
        qDebug() << "无效路径:" << localFile;
        return false;
    }

    if (m_opened) {
        close();
    }

    PlaybackStats::instance().reset();
    static constexpr int defDemuxIdx = 0;

    bool ok = true;
    ok &= m_demuxs[defDemuxIdx]->init(localFile.toUtf8().constData(), true);
    if (!ok) {
        close();
        return false;
    }

    const bool haveAudio = m_demuxs[defDemuxIdx]->haveStream(MediaType::Audio);
    const bool haveVideo = m_demuxs[defDemuxIdx]->haveStream(MediaType::Video);
    const bool haveSubtitle = m_demuxs[defDemuxIdx]->haveStream(MediaType::Subtitle);

    // 加载音频
    if (haveAudio) {
        const int audioIdx = 0;
        ok &= m_demuxs[defDemuxIdx]->switchAudioStream(audioIdx, m_pktAudioBuf, m_frmAudioBuf);
        m_streams[to_index(MediaIdx::Audio)] = {defDemuxIdx, audioIdx};
        ok &= m_decodeAudio->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf, 1);
        ok &= m_audioPlayer->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf);
    }

    // 加载视频
    if (haveVideo) {
        const int videoIdx = 0;
        m_streams[to_index(MediaIdx::Video)] = {defDemuxIdx, videoIdx};
        ok &= m_demuxs[defDemuxIdx]->switchVideoStream(videoIdx, m_pktVideoBuf, m_frmVideoBuf);
        const int cores = std::thread::hardware_concurrency(); // 逻辑核心数
        ok &= m_decodeVideo->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Video), m_pktVideoBuf, m_frmVideoBuf, cores >= 6 ? 6 : 0);
    }

    // 加载字幕
    if (haveVideo) {
        bool loadOK = false;
        if (m_autoLoadExtSub || !haveSubtitle) {
            const QUrl subtitleFile = EpisodeAssetManager::instance().resolveSubtitleURL(URL);
            loadOK = loadExternalSubtitle(subtitleFile);
        }

        if (!loadOK && haveSubtitle) {
            const int subtitleIdx = 0;
            m_streams[to_index(MediaIdx::Subtitle)] = {defDemuxIdx, subtitleIdx};
            bool isAssSub;
            ok &= m_demuxs[defDemuxIdx]->switchSubtitleStream(subtitleIdx, m_pktSubtitleBuf, m_frmSubtitleBuf, isAssSub);
            if (!isAssSub) ok &= m_decodeSubtitle->init(m_demuxs[defDemuxIdx]->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf, 1);
        }
    }

    DeviceStatus::instance().setHaveAudio(haveAudio);
    DeviceStatus::instance().setHaveVideo(haveVideo);

    setDuration(m_demuxs[defDemuxIdx]->getDuration()); // 总时长
    setProgress(0);
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
    m_decodeSubtitle->start();
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
    if (m_streams[to_index(MediaIdx::Video)].first < 0 || m_streams[to_index(MediaIdx::Video)].second < 0) return false;
    return openStreamByFile(URL, MediaIdx::Subtitle);
}

bool MediaController::openAudioStream(const QUrl &URL) {
    // TODO: 允许在没有主 demux 的情况下使用副解复用器播放音频
    if (m_streams[to_index(MediaIdx::Video)].first < 0 || m_streams[to_index(MediaIdx::Video)].second < 0) return false;
    return openStreamByFile(URL, MediaIdx::Audio);
}

bool MediaController::loadExternalSubtitle(const QUrl &URL) {
    bool loadOK = openSubtitleStream(URL);
    if (loadOK) {
        loadOK = switchSubtitleStream(to_index(MediaIdx::Subtitle), 0);
    }
    return loadOK;
}

bool MediaController::loadExternalAudio(const QUrl &URL) {
    bool loadOK = openAudioStream(URL);
    if (loadOK) {
        loadOK = switchAudioStream(to_index(MediaIdx::Audio), 0);
    }
    return loadOK;
}

void MediaController::close() {
    if (!m_opened) return;

    for (int i = 0; i < 3; ++i) {
        if (m_demuxs[i]) m_demuxs[i]->uninit();
    }

    m_decodeAudio->uninit();
    m_decodeVideo->uninit();
    m_decodeSubtitle->uninit();
    m_audioPlayer->uninit();
    m_videoPlayer->uninit();

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
    setOpened(false);
    m_streams.fill({-1, -1});
    setPaused(true);
    setDuration(0);
    setProgress(0);
    PlaybackStats::instance().reset();
    qDebug() << "关闭";
    emit streamInfoUpdate();
    emit chaptersInfoUpdate();
    emit clearVideoFBOSubtitleTex();
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
    setMuted(!m_muted);
}

bool MediaController::muted() const {
    return m_muted;
}

void MediaController::setMuted(bool newMuted) {
    if (m_muted == newMuted)
        return;
    m_muted = newMuted;
    // 不要使用 MediaController::setVolume, 因为设置音量时会强制解除静音
    m_audioPlayer->setVolume(m_muted ? 0.0 : m_volume);
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

void MediaController::addVolume() {
    setVolume(std::min(1.0, m_volume + 0.04));
}

void MediaController::subVolume() {
    setVolume(std::max(0.0, m_volume - 0.04));
}

void MediaController::seekBySec(double ts, double rel) {
    if (!m_opened)
        return;

    m_demuxs[0]->seekBySec(ts, rel);
    // NOTE: 另外两个解复用器需要等拥有视频的解复用器seek完成后再进行seek，这儿是通过信号的方式触发另外两个解复用器seek的
}

void MediaController::fastForward() {
    seekBySec(std::min(static_cast<double>(m_duration), getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

bool MediaController::switchSubtitleStream(int demuxIdx, int streamIdx) {
    if (m_streams[to_index(MediaIdx::Video)].first < 0 || m_streams[to_index(MediaIdx::Video)].second < 0)
        return false;

    if (m_streams[to_index(MediaIdx::Subtitle)] == std::pair{demuxIdx, streamIdx}) {
        return true;
    }
    // 关闭旧的
    if (m_streams[to_index(MediaIdx::Subtitle)].first != -1)
        m_demuxs[m_streams[to_index(MediaIdx::Subtitle)].first]->closeStream(MediaIdx::Subtitle);
    m_decodeSubtitle->uninit();
    ASSRender::instance().uninit();
    m_videoPlayer->clearSubtitle(); // 切换后需要一定时间才会解码到新字幕，因此提前强制清掉旧字幕

    clearPktQ(m_pktSubtitleBuf);
    m_pktSubtitleBuf->addSerial();
    m_frmSubtitleBuf->addSerial(); // videoPlayer可能还在使用，不需要清空，仅修改序号即可
    // 打开新的
    bool isAssSub;
    if (!m_demuxs[demuxIdx]->switchSubtitleStream(streamIdx, m_pktSubtitleBuf, m_frmSubtitleBuf, isAssSub)) { return false; }
    if (!isAssSub) {
        if (!m_decodeSubtitle->init(m_demuxs[demuxIdx]->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf, 1)) {
            return false;
        }
        m_decodeSubtitle->start();
    }

    m_streams[to_index(MediaIdx::Subtitle)] = {demuxIdx, streamIdx}; // 更新
    emit streamInfoUpdate();

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
    if (!m_demuxs[demuxIdx]->switchAudioStream(streamIdx, m_pktAudioBuf, m_frmAudioBuf)) { return false; }
    if (!m_decodeAudio->init(m_demuxs[demuxIdx]->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf, 1)) { return false; }
    if (!m_audioPlayer->init(m_demuxs[demuxIdx]->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf)) { return false; }
    DeviceStatus::instance().setHaveAudio(true);
    m_decodeAudio->start();
    m_audioPlayer->start();

    if (m_paused) {
        // start 会无条件开始播放，这儿重新设置一下状态
        m_audioPlayer->togglePaused();
    }

    m_streams[to_index(MediaIdx::Audio)] = {demuxIdx, streamIdx}; // 更新
    emit streamInfoUpdate();

    return true;
}

bool MediaController::autoLoadExtSub() const {
    return m_autoLoadExtSub;
}

void MediaController::setAutoLoadExtSub(bool newAutoLoadExtSub) {
    if (m_autoLoadExtSub == newAutoLoadExtSub) return;
    m_autoLoadExtSub = newAutoLoadExtSub;
    emit autoLoadExtSubChanged();
}

int MediaController::progress() const {
    return m_progress;
}

void MediaController::setProgress(int newProgress) {
    if (m_progress == newProgress)
        return;
    m_progress = newProgress;
    emit progressChanged();
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
    const auto index = to_index(type);
    QVariantList list;
    for (size_t i = 0; i < m_demuxs.size(); ++i) {
        Demux *const demux = m_demuxs[i];
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
    const uint8_t index = to_index(idx);
    const QString localFile = URL.toLocalFile();
    QFileInfo fileInfo(localFile);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "无效路径，或不是文件" << localFile;
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

    bool initOk = m_demuxs[index]->init(localFile.toUtf8().constData());
    if (initOk && m_demuxs[index]->getStreamsCount()[to_index(idx)] < 1) {
        qDebug() << "文件中没有对应的流" << localFile;
        m_demuxs[index]->uninit();
        emit streamInfoUpdate();
        return false;
    }
    emit streamInfoUpdate();
    return initOk;
}

bool MediaController::seekAudioAndSubtitleDemux(double pts) {
    m_demuxs[1]->seekBySec(pts, 0.0);
    m_demuxs[2]->seekBySec(pts, 0.0);
    return true;
}

void MediaController::checkPlayerFinished() {
    if (!m_opened || m_played || !m_demuxs[0]->isEOF())
        return;

    const bool haveAudio = m_streams[to_index(MediaIdx::Audio)].second != -1;

    const bool audioQueueIsEmpty = m_pktAudioBuf->size() == 0 && m_frmAudioBuf->size() == 0;
    const bool videoQueueIsEmpty = m_pktVideoBuf->size() == 0 && m_frmVideoBuf->size() == 0;
    bool playerFinished = false;

    if (haveAudio) {
        if (audioQueueIsEmpty) {
            playerFinished = true;
        }
    } else if (videoQueueIsEmpty) {
        playerFinished = true;
    }

    if (playerFinished) {
        if (m_loopOnEnd) {
            seekBySec(0.0, 0.0);
        } else {
            if (!m_paused)
                togglePaused(); // HACK: 已经暂停有可能触发播放结束吗？
            m_played = true;
            emit played();
        }
    }
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
    Demux *const demux = m_demuxs[to_index(MediaIdx::Video)];
    QVariantList list;

    if (demux == nullptr)
        return list;

    const std::vector<ChapterInfo> &chapters = demux->chaptersInfo();
    for (auto &v : chapters) {
        int total = static_cast<int>(v.pts * 1000.0 + 0.5);

        const int ms = total % 1000;
        total /= 1000;

        const int sec = total % 60;
        total /= 60;

        const int min = total % 60;
        const int hour = total / 60;

        const QString timeStr = QString("[%1:%2:%3:%4]")
                                    .arg(hour, 2, 10, QLatin1Char('0'))
                                    .arg(min, 2, 10, QLatin1Char('0'))
                                    .arg(sec, 2, 10, QLatin1Char('0'))
                                    .arg(ms, 3, 10, QLatin1Char('0'));

        const QString title = QString::fromStdString(v.title);

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
    size_t idx = 0;
    for (int i = 0; i < m_streams[to_index(MediaIdx::Subtitle)].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[to_index(MediaIdx::Subtitle)];
    }
    return static_cast<int>(idx) + m_streams[to_index(MediaIdx::Subtitle)].second;
}

int MediaController::getAudioIdx() const {
    if (m_streams[to_index(MediaIdx::Audio)].first == -1)
        return -1;
    size_t idx = 0;
    for (int i = 0; i < m_streams[to_index(MediaIdx::Audio)].first; ++i) {
        idx += m_demuxs[i]->getStreamsCount()[to_index(MediaIdx::Audio)];
    }
    return static_cast<int>(idx) + m_streams[to_index(MediaIdx::Audio)].second;
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

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "demux.h"
#include "clock/globalclock.h"
#include "renderer/assrender.h"
#include <QDebug>
namespace {
    static char _infoBuf[512];

    QString getStringInfo(AVStream *st) {
        const AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
        QString str = lang ? QString("(%1), ").arg(lang->value) : "";
        lang = av_dict_get(st->metadata, "title", NULL, 0);
        str += lang ? QString("%1, ").arg(lang->value) : "";

        AVCodecContext *ctx = avcodec_alloc_context3(nullptr);
        if (!ctx)
            return str;
        int ret = avcodec_parameters_to_context(ctx, st->codecpar);
        if (ret < 0) {
            avcodec_free_context(&ctx);
            return str;
        }
        avcodec_string(_infoBuf, sizeof(_infoBuf), ctx, 0);
        avcodec_free_context(&ctx);

        str += QString(" %1").arg(_infoBuf);
        return str;
    }
}

Demux::Demux(QObject *parent)
    : QObject{parent} {}

Demux::~Demux() {
    uninit();
}

bool Demux::init(const std::string URL, bool isMainDemux) {
    if (m_initialized) {
        uninit();
    }

    // 打开文件并初始化FormatContext
    int ret = avformat_open_input(&m_formatCtx, URL.c_str(), nullptr, nullptr);
    if (ret != 0) {
        av_strerror(ret, errBuf, 512);
        qDebug() << errBuf;
        return false;
    }

    double maxFrameDuration = (m_formatCtx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    GlobalClock::instance().setMaxFrameDuration(maxFrameDuration);

    // 读取媒体文件的流信息
    ret = avformat_find_stream_info(m_formatCtx, nullptr);
    if (ret < 0) {
        av_strerror(ret, errBuf, 512);
        qDebug() << errBuf;
        avformat_close_input(&m_formatCtx);
        return false;
    }

    // 获取各种流的idx
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; ++i) {
        AVMediaType sType = m_formatCtx->streams[i]->codecpar->codec_type;
        if (sType == AVMEDIA_TYPE_VIDEO)
            m_videoIdx.push_back(i);
        else if (sType == AVMEDIA_TYPE_AUDIO)
            m_audioIdx.push_back(i);
        else if (sType == AVMEDIA_TYPE_SUBTITLE)
            m_subtitleIdx.push_back(i);
    }

    fillStreamInfo();   // 填充流的描述信息
    fillChaptersInfo(); // 填充章节的描述信息

    m_usedVIdx.store(-1, std::memory_order_relaxed);
    m_usedAIdx.store(-1, std::memory_order_relaxed);
    m_usedSIdx.store(-1, std::memory_order_relaxed);

    m_URL = URL;
    m_initialized = true;
    m_isEOF = false;
    m_isMainDemux = isMainDemux;

    return true;
}

bool Demux::uninit() {
    stop();
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
    }

    m_URL.clear();

    m_videoIdx.clear();
    m_audioIdx.clear();
    m_subtitleIdx.clear();

    for (auto &v : m_stringInfo) {
        v.clear();
    }

    m_chaptersInfo.clear();

    m_usedVIdx.store(-1, std::memory_order_relaxed);
    m_usedAIdx.store(-1, std::memory_order_relaxed);
    m_usedSIdx.store(-1, std::memory_order_relaxed);

    m_initialized = false;

    m_audioPktBuf.reset();
    m_videoPktBuf.reset();
    m_subtitlePktBuf.reset();
    m_audioFrmBuf.reset();
    m_videoFrmBuf.reset();
    m_subtitleFrmBuf.reset();

    return true;
}

void Demux::start() {
    if (m_thread.joinable()) {
        return; // 已经在运行了
    }
    m_stop.store(false, std::memory_order_relaxed);
    m_thread = std::thread([this]() {
        demuxLoop();
    });
}

void Demux::stop() {
    if (!m_thread.joinable()) {
        return; // 已经退出
    }
    m_stop.store(true, std::memory_order_relaxed);
    m_thread.join();
}

void Demux::seekBySec(double ts, double rel) {
    if (!m_initialized || m_stop.load(std::memory_order_relaxed)) {
        return;
    }
    m_seekTs = ts;
    m_seekRel = rel;
    m_needSeek.store(true, std::memory_order_release);
}

bool Demux::switchVideoStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    return switchStream(MediaType::Video, streamIdx, wpq, wfq);
}

bool Demux::switchSubtitleStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq, bool &isAssSub) {
    if (m_usedSIdx != -1) {
        closeStream(MediaType::Subtitle);
    }

    // ASS初始化用的demux全局的流ID
    isAssSub = ASSRender::instance().init(m_URL, m_subtitleIdx[streamIdx]);
    if (isAssSub) {
        std::lock_guard<std::mutex> mtx(m_mutex);
        m_subtitlePktBuf.reset(), m_subtitleFrmBuf.reset();
        m_usedSIdx.store(-1, std::memory_order_release);
        return true;
    }

    return switchStream(MediaType::Subtitle, streamIdx, wpq, wfq);
}

bool Demux::switchAudioStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    return switchStream(MediaType::Audio, streamIdx, wpq, wfq);
}

void Demux::closeStream(MediaType type) {
    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        if (type == MediaType::Video) {
            m_videoPktBuf.reset(), m_videoFrmBuf.reset(), m_usedVIdx = -1;
        } else if (type == MediaType::Subtitle) {
            m_subtitlePktBuf.reset(), m_subtitleFrmBuf.reset(), m_usedSIdx = -1;
        } else if (type == MediaType::Audio) {
            m_audioPktBuf.reset(), m_audioFrmBuf.reset(), m_usedAIdx = -1;
        }
    }

    if (m_usedAIdx == -1 && m_usedVIdx == -1 && m_usedSIdx == -1) {
        stop();
    }
}

int Demux::getDuration() {
    if (m_formatCtx) {
        return m_formatCtx->duration / AV_TIME_BASE;
    }
    return 0;
}

AVStream *Demux::getStream(MediaType type) {
    int idx = -1;

    switch (type) {
    case MediaType::Video:
        idx = m_usedVIdx.load(std::memory_order_relaxed);
        break;
    case MediaType::Audio:
        idx = m_usedAIdx.load(std::memory_order_relaxed);
        break;
    case MediaType::Subtitle:
        idx = m_usedSIdx.load(std::memory_order_relaxed);
        break;
    default:
        return nullptr;
    }

    if (idx < 0 || !m_formatCtx)
        return nullptr;

    return m_formatCtx->streams[idx];
}

bool Demux::haveStream(MediaType type) {
    switch (type) {
    case MediaType::Video:
        return !m_videoIdx.empty();
    case MediaType::Audio:
        return !m_audioIdx.empty();
    case MediaType::Subtitle:
        return !m_subtitleIdx.empty();
    default:
        return false;
    }
}

std::array<size_t, Demux::kMediaIdxCount> Demux::getStreamsCount() const {
    return {m_videoIdx.size(), m_subtitleIdx.size(), m_audioIdx.size()};
}

AVFormatContext *Demux::formatCtx() {
    return m_formatCtx;
}

bool Demux::isEOF() {
    return m_isEOF;
}

void Demux::seekAllPktQueue() {
    if (auto q = m_audioPktBuf.lock()) {
        q->addSerial();
        q->clear();
    }
    if (auto q = m_videoPktBuf.lock()) {
        q->addSerial();
        q->clear();
    }
    if (auto q = m_subtitlePktBuf.lock()) {
        q->addSerial();
        q->clear();
    }

    if (auto q = m_audioFrmBuf.lock()) {
        q->addSerial();
    }
    if (auto q = m_videoFrmBuf.lock()) {
        q->addSerial();
    }
    if (auto q = m_subtitleFrmBuf.lock()) {
        q->addSerial();
    }
}

void Demux::demuxLoop() {
    if (!m_initialized) {
        return;
    }
    AVPacket *pkt = nullptr;
    while (!m_stop.load(std::memory_order_relaxed)) {

        bool emitRealSeekTs{false}; // 是否发射信号

        if (m_needSeek.load(std::memory_order_acquire)) {
            seekAllPktQueue();
            int streamIdx = m_isMainDemux ? -1 : (m_usedVIdx != -1) ? m_usedVIdx.load()
                                             : (m_usedAIdx != -1)   ? m_usedAIdx.load()
                                                                    : m_usedSIdx.load();
            double time_base = streamIdx == -1 ? 1.0 / AV_TIME_BASE : av_q2d(m_formatCtx->streams[streamIdx]->time_base);
            double target = m_seekTs / time_base;
            int64_t seekMin = m_seekRel > 0.0 ? static_cast<int64_t>(target - m_seekRel * AV_TIME_BASE + 2) : INT64_MIN;
            int64_t seekMax = m_seekRel < 0.0 ? static_cast<int64_t>(target - m_seekRel * AV_TIME_BASE - 2) : INT64_MAX;
            int ret = avformat_seek_file(m_formatCtx, streamIdx, seekMin, target, seekMax,
                                         m_usedVIdx == -1 ? AVSEEK_FLAG_ANY : 0);
            if (ret < 0) {
                qDebug() << "seek出错";
            }
            emitRealSeekTs = m_isMainDemux;
            m_needSeek.store(false, std::memory_order_release);
        }

        pkt = av_packet_alloc();
        int ret = av_read_frame(m_formatCtx, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF && !m_isEOF) { // EOF
                Q_ASSERT(pkt->data == NULL && pkt->size == 0);
                pushVideoPkt(pkt);
                pkt = av_packet_alloc();
                pushSubtitlePkt(pkt);
                pkt = av_packet_alloc();
                pushAudioPkt(pkt);
                pkt = nullptr;
                qDebug() << "解复用EOF";
                m_isEOF = true;
            } else if (ret != AVERROR_EOF) { // error
                qDebug() << "解复用出错";
                goto end;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else {
            m_isEOF = false;
        }

        if (emitRealSeekTs) {
            emit seeked(pkt->pts * av_q2d(m_formatCtx->streams[pkt->stream_index]->time_base));
            emitRealSeekTs = false;
        }

        // ret == 0
        if (pkt->stream_index == m_usedAIdx.load(std::memory_order_acquire)) {
            pushAudioPkt(pkt);
        } else if (pkt->stream_index == m_usedVIdx.load(std::memory_order_acquire)) {
            pushVideoPkt(pkt);
        } else if (pkt->stream_index == m_usedSIdx.load(std::memory_order_acquire)) {
            pushSubtitlePkt(pkt);
        } else {
            av_packet_free(&pkt);
        }
        pkt = nullptr;
    }

end:
    if (pkt) {
        av_packet_free(&pkt);
    }
}

void Demux::pushVideoPkt(AVPacket *pkt) {
    pushPkt(m_videoPktBuf, pkt);
}

void Demux::pushAudioPkt(AVPacket *pkt) {
    pushPkt(m_audioPktBuf, pkt);
}

void Demux::pushSubtitlePkt(AVPacket *pkt) {
    pushPkt(m_subtitlePktBuf, pkt);
}

void Demux::pushPkt(weakPktQueue wq, AVPacket *pkt) {
    while (auto q = wq.lock()) {
        bool ok = q->push({pkt, q->serial()});
        if (ok)
            return;
        if (m_needSeek.load(std::memory_order_acquire) || m_stop.load(std::memory_order_relaxed)) {
            av_packet_free(&pkt);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    av_packet_free(&pkt);
}

void Demux::fillStreamInfo() {
    // Video
    // Subtitle
    auto &subArr = m_stringInfo[to_index(MediaIdx::Subtitle)];
    for (auto v : m_subtitleIdx) {
        AVStream *st = m_formatCtx->streams[v];
        subArr.emplace_back(getStringInfo(st));
    }

    // Audio
    auto &audioArr = m_stringInfo[to_index(MediaIdx::Audio)];
    for (auto v : m_audioIdx) {
        AVStream *st = m_formatCtx->streams[v];
        audioArr.emplace_back(getStringInfo(st));
    }
}

void Demux::fillChaptersInfo() {
    for (unsigned int i = 0; i < m_formatCtx->nb_chapters; ++i) {
        AVChapter *ch = m_formatCtx->chapters[i];
        m_chaptersInfo.push_back(ChapterInfo());

        m_chaptersInfo.back().pts = ch->start != AV_NOPTS_VALUE ? ch->start * av_q2d(ch->time_base) : 0.0;

        AVDictionaryEntry *e = av_dict_get(ch->metadata, "title", NULL, 0);
        if (!e) {
            e = av_dict_get(ch->metadata, "NAME", NULL, 0);
        }

        if (e) {
            m_chaptersInfo.back().title = std::string(e->value);
        }
    }
}

bool Demux::switchStream(MediaType type, int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    std::vector<int> *idxVec = nullptr;
    std::atomic<int> *usedIdx = nullptr;
    weakPktQueue *pktBuf = nullptr;
    weakFrmQueue *frmBuf = nullptr;

    switch (type) {
    case MediaType::Video:
        idxVec = &m_videoIdx;
        usedIdx = &m_usedVIdx;
        pktBuf = &m_videoPktBuf;
        frmBuf = &m_videoFrmBuf;
        break;
    case MediaType::Audio:
        idxVec = &m_audioIdx;
        usedIdx = &m_usedAIdx;
        pktBuf = &m_audioPktBuf;
        frmBuf = &m_audioFrmBuf;
        break;
    case MediaType::Subtitle:
        idxVec = &m_subtitleIdx;
        usedIdx = &m_usedSIdx;
        pktBuf = &m_subtitlePktBuf;
        frmBuf = &m_subtitleFrmBuf;
        break;
    default:
        return false;
    }

    // 如果已有流，先关闭
    if (usedIdx->load(std::memory_order_relaxed) != -1) {
        closeStream(type);
    }

    Q_ASSERT(streamIdx < idxVec->size());

    // 更新队列和当前流
    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        *pktBuf = wpq, *frmBuf = wfq;
        usedIdx->store((*idxVec)[streamIdx], std::memory_order_release);
    }

    bool ok = true;
    // seek 或启动线程
    if (m_stop.load(std::memory_order_relaxed)) {
        double pts = GlobalClock::instance().getMainPts();
        if (!std::isnan(pts)) {
            int64_t target = pts / av_q2d(getStream(type)->time_base);
            int streamIndex = (*idxVec)[streamIdx];
            int flags = (m_usedVIdx.load(std::memory_order_relaxed) == -1) ? AVSEEK_FLAG_ANY : 0;
            ok = avformat_seek_file(m_formatCtx, streamIndex, INT64_MIN, target, INT64_MAX, flags);
            start();
        }
    } else {
        seekBySec(GlobalClock::instance().getMainPts(), 0);
    }

    return ok;
}

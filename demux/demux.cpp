#include "demux.h"
#include "clock/globalclock.h"
#include <QDebug>

Demux::Demux(QObject *parent)
    : QObject{parent} {}

Demux::~Demux() {
    uninit();
}

bool Demux::init(const std::string URL, sharedPktQueue audioQ, sharedPktQueue videoQ, sharedPktQueue subtitleQ) {
    if (m_initialized) {
        uninit();
    }

    m_audioPktBuf = audioQ;
    m_videoPktBuf = videoQ;
    m_subtitlePktBuf = subtitleQ;

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
        return -1;
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

    m_usedVIdx = m_videoIdx.empty() ? -1 : m_videoIdx.front();
    m_usedAIdx = m_audioIdx.empty() ? -1 : m_audioIdx.front();
    m_usedSIdx = m_subtitleIdx.empty() ? -1 : m_subtitleIdx.front();

    m_initialized = true;

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

    m_usedVIdx = m_usedAIdx = m_usedSIdx = -1;
    m_initialized = false;

    m_audioPktBuf.reset();
    m_videoPktBuf.reset();
    m_subtitlePktBuf.reset();

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
    GlobalClock::instance().setSeekTs(ts);
    m_seekRel = rel;
    m_needSeek.store(true, std::memory_order_release);
}

int Demux::getDuration() {
    if (m_formatCtx) {
        return m_formatCtx->duration / AV_TIME_BASE;
    }
    return 0;
}

AVStream *Demux::getVideoStream() {
    if (m_usedVIdx == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedVIdx];
}

AVStream *Demux::getAudioStream() {
    if (m_usedAIdx == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedAIdx];
}

AVStream *Demux::getSubtitleStream() {
    if (m_usedSIdx == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedSIdx];
}

AVFormatContext *Demux::formatCtx() {
    return m_formatCtx;
}

void Demux::demuxLoop() {
    if (!m_initialized) {
        return;
    }
    AVPacket *pkt = nullptr;
    while (!m_stop.load(std::memory_order_relaxed)) {

        if (m_needSeek.load(std::memory_order_acquire)) {
            GlobalClock::instance().addSeekCnt();
            double target = GlobalClock::instance().seekTs() * AV_TIME_BASE;
            int64_t seekMin = m_seekRel > 0 ? target - m_seekRel * AV_TIME_BASE + 2 : INT64_MIN;
            int64_t seekMax = m_seekRel < 0 ? target - m_seekRel * AV_TIME_BASE - 2 : INT64_MAX;
            int ret = avformat_seek_file(m_formatCtx, -1, seekMin, target, seekMax, 0);
            if (ret < 0) {
                qDebug() << "seek出错";
            }
            m_needSeek.store(false, std::memory_order_release);
        }

        pkt = av_packet_alloc();
        int ret = av_read_frame(m_formatCtx, pkt);
        if (pkt == nullptr || ret > 0) { // error
            qDebug() << "解复用出错";
            goto end;
        }
        if (ret < 0) { // EOF
            qDebug() << "文件解复用完毕";
            goto end;
        }

        // ret == 0
        if (pkt->stream_index == m_usedAIdx) {
            pushAudioPkt(pkt);
        } else if (pkt->stream_index == m_usedVIdx) {
            pushVideoPkt(pkt);
        } else if (pkt->stream_index == m_usedSIdx) {
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

void Demux::pushPkt(sharedPktQueue q, AVPacket *pkt) {
    if (!q) {
        av_packet_free(&pkt);
        return;
    }
    while (!q->push({pkt, GlobalClock::instance().seekCnt()})) {
        if (m_needSeek.load(std::memory_order_acquire) || m_stop.load(std::memory_order_relaxed)) {
            av_packet_free(&pkt);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

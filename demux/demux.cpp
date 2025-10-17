#include "demux.h"
#include "clock/globalclock.h"
#include <QDebug>
namespace {
    char _infoBuf[512];

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

bool Demux::init(const std::string URL) {
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

    fillStreamInfo(); // 填充流的描述信息

    m_usedVIdx.store(-1, std::memory_order_relaxed);
    m_usedAIdx.store(-1, std::memory_order_relaxed);
    m_usedSIdx.store(-1, std::memory_order_relaxed);

    m_initialized = true;
    m_isEOF = false;

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
    GlobalClock::instance().setSeekTs(ts);
    m_seekRel = rel;
    m_needSeek.store(true, std::memory_order_release);
}

bool Demux::switchVideoStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    if (m_usedVIdx != -1) {
        closeStream(0);
    }
    Q_ASSERT(streamIdx < m_videoIdx.size());
    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        m_videoPktBuf = wpq, m_videoFrmBuf = wfq;
        m_usedVIdx.store(m_videoIdx[streamIdx], std::memory_order_release);
    }

    bool ok = 1;
    if (m_stop.load(std::memory_order_relaxed)) {
        double pts = GlobalClock::instance().getMainPts();
        if (!std::isnan(pts)) {
            int64_t target = pts / av_q2d(getVideoStream()->time_base);
            ok = avformat_seek_file(m_formatCtx, m_videoIdx[streamIdx], target, target, target, 0);
            start();
        }
    }

    return ok;
}

bool Demux::switchSubtitleStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    if (m_usedSIdx != -1) {
        closeStream(1);
    }
    Q_ASSERT(streamIdx < m_subtitleIdx.size());
    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        m_subtitlePktBuf = wpq, m_subtitleFrmBuf = wfq;
        m_usedSIdx.store(m_subtitleIdx[streamIdx], std::memory_order_release);
    }

    bool ok = 1;
    if (m_stop.load(std::memory_order_relaxed)) {
        double pts = GlobalClock::instance().getMainPts();
        if (!std::isnan(pts)) {
            int64_t target = pts / av_q2d(getSubtitleStream()->time_base);
            ok = avformat_seek_file(m_formatCtx, m_subtitleIdx[streamIdx], target, target, target,
                                    m_usedVIdx.load(std::memory_order_relaxed) == -1 ? AVSEEK_FLAG_ANY : 0);
            start();
        }
    }

    return ok;
}

bool Demux::switchAudioStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq) {
    if (m_usedAIdx != -1) {
        closeStream(2);
    }
    Q_ASSERT(streamIdx < m_audioIdx.size());

    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        m_audioPktBuf = wpq, m_audioFrmBuf = wfq;
        m_usedAIdx.store(m_audioIdx[streamIdx], std::memory_order_release);
    }
    bool ok = 1;
    if (m_stop.load(std::memory_order_relaxed)) {
        double pts = GlobalClock::instance().getMainPts();
        if (!std::isnan(pts)) {
            int64_t target = pts / av_q2d(getAudioStream()->time_base);
            ok = avformat_seek_file(m_formatCtx, m_usedAIdx.load(std::memory_order_relaxed), target, target, target,
                                    m_usedVIdx.load(std::memory_order_relaxed) == -1 ? AVSEEK_FLAG_ANY : 0);
            start();
        }
    }

    return ok;
}

void Demux::closeStream(int streamType) {
    {
        std::lock_guard<std::mutex> mtx(m_mutex);
        if (streamType == 0) {
            m_videoPktBuf.reset(), m_videoFrmBuf.reset(), m_usedVIdx = -1;
        } else if (streamType == 1) {
            m_subtitlePktBuf.reset(), m_subtitleFrmBuf.reset(), m_usedSIdx = -1;
        } else if (streamType == 2) {
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

AVStream *Demux::getVideoStream() {
    if (m_usedVIdx.load(std::memory_order_relaxed) == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedVIdx.load(std::memory_order_relaxed)];
}

AVStream *Demux::getAudioStream() {
    if (m_usedAIdx.load(std::memory_order_relaxed) == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedAIdx.load(std::memory_order_relaxed)];
}

AVStream *Demux::getSubtitleStream() {
    if (m_usedSIdx.load(std::memory_order_relaxed) == -1)
        return nullptr;
    return m_formatCtx->streams[m_usedSIdx.load(std::memory_order_relaxed)];
}

AVFormatContext *Demux::formatCtx() {
    return m_formatCtx;
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

        if (m_needSeek.load(std::memory_order_acquire)) {
            GlobalClock::instance().addSeekCnt();
            seekAllPktQueue();
            double target = GlobalClock::instance().seekTs() * AV_TIME_BASE;
            int64_t seekMin = m_seekRel > 0.0 ? static_cast<int64_t>(target - m_seekRel * AV_TIME_BASE + 2) : INT64_MIN;
            int64_t seekMax = m_seekRel < 0.0 ? static_cast<int64_t>(target - m_seekRel * AV_TIME_BASE - 2) : INT64_MAX;
            int ret = avformat_seek_file(m_formatCtx, -1, seekMin, target, seekMax, 0);
            if (ret < 0) {
                qDebug() << "seek出错";
            }
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
    std::lock_guard<std::mutex> mtx(m_mutex);
    if (auto q = wq.lock()) {
        while (!q->push({pkt, q->serial()})) {
            if (m_needSeek.load(std::memory_order_acquire) || m_stop.load(std::memory_order_relaxed)) {
                av_packet_free(&pkt);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } else {
        av_packet_free(&pkt);
    }
}

void Demux::fillStreamInfo() {
    // Video
    // Subtitle
    auto &subArr = m_stringInfo[MediaIdx::SUBTITLE];
    for (auto v : m_subtitleIdx) {
        AVStream *st = m_formatCtx->streams[v];
        subArr.emplace_back(getStringInfo(st));
    }

    // Audio
    auto &audioArr = m_stringInfo[MediaIdx::AUDIO];
    for (auto v : m_audioIdx) {
        AVStream *st = m_formatCtx->streams[v];
        audioArr.emplace_back(getStringInfo(st));
    }
}

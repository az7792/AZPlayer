#include "decodebase.h"
#include "clock/globalclock.h"
#include <QDebug>

DecodeBase::DecodeBase(QObject *parent)
    : QObject{parent} {}

DecodeBase::~DecodeBase() {
    uninit();
}

bool DecodeBase::init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    if (stream == nullptr) {
        return false;
    }
    if (m_initialized) {
        uninit();
    }

    m_codecCtx = avcodec_alloc_context3(nullptr);
    if (!m_codecCtx) {
        qDebug() << "解码器上下文分配失败";
        return false;
    }

    int ret = avcodec_parameters_to_context(m_codecCtx, stream->codecpar);
    if (ret < 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "解码器参数设置失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    m_codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!m_codec) {
        qDebug() << "无合适的解码器";
        return false;
    }

    m_codecCtx->pkt_timebase = stream->time_base;
    m_codecCtx->codec_id = m_codec->id;

    ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret != 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "打开编码器失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    if (!pktBuf) {
        qDebug() << "无效pkt队列";
        return false;
    }
    if (!frmBuf) {
        qDebug() << "无效frm队列";
        return false;
    }

    m_pktBuf = pktBuf;
    m_frmBuf = frmBuf;
    m_time_base = stream->time_base;
    return true;
}

bool DecodeBase::uninit() {
    if (!m_initialized) {
        return true;
    }
    stop();
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
    m_initialized = false;
    m_pktBuf.reset();
    m_frmBuf.reset();
    return true;
}

void DecodeBase::start() {
    if (!m_initialized || m_thread.joinable()) {
        return; // 已经在运行了
    }
    m_stop.store(false, std::memory_order_relaxed);
    m_thread = std::thread([this]() {
        decodingLoop();
    });
}

void DecodeBase::stop() {
    if (!m_thread.joinable()) {
        return; // 已经退出
    }
    m_stop.store(true, std::memory_order_relaxed);
    m_thread.join();
}

bool DecodeBase::initialized() {
    return m_initialized;
}

bool DecodeBase::getPkt(AVPktItem &pktItem) {
    if (pktItem.pkt && pktItem.pkt->data && pktItem.pkt->size != 0) {
        return true;
    }
    // 进行seek
    int seekCnt = GlobalClock::instance().seekCnt();
    bool need_flush_buffers = false;
    while (m_pktBuf->peekFirst(pktItem) && pktItem.seekCnt != seekCnt) {
        need_flush_buffers = true;
        m_pktBuf->pop(pktItem);
        av_packet_free(&pktItem.pkt);
    }
    if (need_flush_buffers) {
        avcodec_flush_buffers(m_codecCtx);
    }

    // 从缓冲区取数据
    if (pktItem.pkt && pktItem.pkt->data && pktItem.pkt->size != 0) {
        m_pktBuf->pop(pktItem);
        return true;
    }
    return false;
}

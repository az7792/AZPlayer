// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "decodebase.h"
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
    m_streamIdx = stream->index;
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

    // HACK: 使用多线程会加剧部分带封面音频无法显示的问题
    AVDictionary *opts = nullptr;
    int cores = std::thread::hardware_concurrency();             // 逻辑核心数
    av_dict_set(&opts, "threads", cores >= 6 ? "6" : "auto", 0); // auto 为自动

    ret = avcodec_open2(m_codecCtx, m_codec, &opts);
    av_dict_free(&opts);

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
    m_isEOF = false;
    m_serial = 0;
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
    m_streamIdx = -1;
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

bool DecodeBase::initialized() const {
    return m_initialized;
}

bool DecodeBase::getPkt(AVPktItem &pktItem, bool &needFlushBuffers) {
    // *流ID不同直接丢弃，不用清解码器缓存

    if (pktItem.pkt && pktItem.pkt->data && pktItem.pkt->size != 0) {
        if (pktItem.serial == m_pktBuf->serial() && pktItem.pkt->stream_index == m_streamIdx) {
            return true;
        }
        av_packet_free(&pktItem.pkt);
        if (pktItem.serial != m_pktBuf->serial())
            needFlushBuffers = true;
    }

    if (!m_pktBuf->pop(pktItem)) { // 空
        return false;
    } else if (pktItem.serial != m_pktBuf->serial()) { // 非空 但是序号不同
        av_packet_free(&pktItem.pkt);
        needFlushBuffers = true;
        return false;
    } else if (pktItem.pkt->stream_index != m_streamIdx) { // 非空 但是流idx不同
        av_packet_free(&pktItem.pkt);
        // needFlushBuffers = true;
        return false;
    }

    return true;
}

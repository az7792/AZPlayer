#include "decodebase.h"
#include <QDebug>

DecodeBase::DecodeBase() {}

DecodeBase::~DecodeBase() {
}

bool DecodeBase::init(AVCodecID id, const AVCodecParameters *par, AVRational time_base, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    m_codec = avcodec_find_decoder(id);
    if (!m_codec) {
        qDebug() << "无合适的解码器";
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        qDebug() << "解码器上下文分配失败";
        return false;
    }

    int ret = avcodec_parameters_to_context(m_codecCtx, par);
    if (ret < 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "解码器参数设置失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
        return false;
    }

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
    m_time_base = time_base;
    return true;
}

bool DecodeBase::initialized() {
    return m_initialized;
}

void DecodeBase::stopDecoding() {
    m_stop.store(true, std::memory_order_relaxed);
}

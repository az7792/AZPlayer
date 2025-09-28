#include "decodeaudio.h"
#include <QDebug>
#include <QThread>

DecodeAudio::DecodeAudio() {}

bool DecodeAudio::init(AVCodecID id, const AVCodecParameters *par, AVRational time_base, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    bool intok = DecodeBase::init(id, par, time_base, pktBuf, frmBuf);
    if (!intok) {
        return false;
    }

    m_oldPar.sampleFormat = static_cast<AVSampleFormat>(par->format);
    m_oldPar.sampleRate = par->sample_rate;
    int ret = av_channel_layout_copy(&m_oldPar.ch_layout, &par->ch_layout);
    if (ret != 0) {
        return false;
    }
    m_initialized = true;
    return true;
}

void DecodeAudio::startDecoding() {
    if (!m_initialized) {
        emit finished();
        return;
    }

    m_stop.store(false, std::memory_order_relaxed);

    AVPktItem pktItem;
    AVFrmItem frmItem;
    bool sendEAGAIN = false;
    while (!m_stop.load(std::memory_order_relaxed)) {
        // 从缓冲区取数据
        while (sendEAGAIN == false && !m_pktBuf->pop(pktItem)) {
            if (m_stop.load(std::memory_order_relaxed)) {
                goto end;
            }
            QThread::msleep(5);
        }

        int ret = avcodec_send_packet(m_codecCtx, pktItem.pkt);

        if (ret == 0) {
            av_packet_free(&pktItem.pkt);
            sendEAGAIN = false;
        } else if (ret == AVERROR_EOF) {
            // TODO : 处理音频时钟
            qDebug() << "pkt EOP";
            av_packet_free(&pktItem.pkt);
            sendEAGAIN = false;
            continue;
        } else if (ret == AVERROR(EAGAIN)) {
            sendEAGAIN = true;
        } else if (ret < 0) {
            qDebug() << "发送audiopkt错误:" << ret;
            goto end;
        }

        while (true) {
            frmItem.frm = av_frame_alloc();
            frmItem.seekCnt = pktItem.seekCnt;
            ret = avcodec_receive_frame(m_codecCtx, frmItem.frm);
            // 完全消耗完解码后的帧
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frmItem.frm);
                break;
            }

            // 写入缓冲区
            if (ret == 0) {
                frmItem.pts = (frmItem.frm->pts == AV_NOPTS_VALUE) ? std::numeric_limits<double>::quiet_NaN() : frmItem.frm->pts * av_q2d(m_time_base);
                while (!m_frmBuf->push(frmItem)) {
                    if (m_stop.load(std::memory_order_relaxed)) {
                        goto end;
                    }
                    QThread::msleep(5);
                }
                frmItem.frm = nullptr;
            } else {
                qDebug() << "读取audiofrm错误:" << ret;
                goto end;
            }
        }
    }

end:
    av_packet_free(&pktItem.pkt);
    av_frame_free(&frmItem.frm);
    emit finished();
}

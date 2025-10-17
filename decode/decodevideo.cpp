#include "decodevideo.h"
#include <QDebug>

bool DecodeVideo::init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    bool initok = DecodeBase::init(stream, pktBuf, frmBuf);
    if (!initok) {
        return false;
    }

    m_initialized = true;
    return true;
}

void DecodeVideo::decodingLoop() {
    if (!m_initialized) {
        return;
    }

    AVPktItem pktItem;
    AVFrmItem frmItem;
    bool needFlushBuffers = false;
    while (!m_stop.load(std::memory_order_relaxed)) {
        bool ok = getPkt(pktItem, needFlushBuffers);
        if (!ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (needFlushBuffers || m_serial != m_pktBuf->serial()) {
            m_serial = m_pktBuf->serial();
            avcodec_flush_buffers(m_codecCtx);
            needFlushBuffers = false;
        }

        int ret = avcodec_send_packet(m_codecCtx, pktItem.pkt);

        if (ret == 0) {
            av_packet_free(&pktItem.pkt);
        } else if (ret == AVERROR_EOF) {
            av_packet_free(&pktItem.pkt);
            m_isEOF = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else if (ret == AVERROR(EAGAIN)) {
            // nothing
        } else if (ret < 0) {
            av_strerror(ret, errBuf, sizeof(errBuf));
            qDebug() << "Audio发送audiopkt错误:" << errBuf << pktItem.pkt->stream_index;
            goto end;
        }

        m_isEOF = false;
        while (true) {
            frmItem.frm = av_frame_alloc();
            frmItem.serial = pktItem.serial;
            ret = avcodec_receive_frame(m_codecCtx, frmItem.frm);
            // 完全消耗完解码后的帧
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frmItem.frm);
                break;
            }

            // 写入缓冲区
            if (ret == 0) {
                double timeBase = av_q2d(m_time_base);
                frmItem.pts = (frmItem.frm->best_effort_timestamp == AV_NOPTS_VALUE) ? frmItem.frm->pts * timeBase : frmItem.frm->best_effort_timestamp * timeBase;
                frmItem.duration = frmItem.frm->duration * timeBase;
                while (!m_frmBuf->push(frmItem)) {
                    if (m_stop.load(std::memory_order_relaxed)) {
                        goto end;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                frmItem.frm = nullptr;
            } else {
                qDebug() << "读取videofrm错误:" << ret;
                goto end;
            }
        }
    }

end:
    av_packet_free(&pktItem.pkt);
    av_frame_free(&frmItem.frm);
}

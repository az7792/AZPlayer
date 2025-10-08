#include "decodeaudio.h"
#include "clock/globalclock.h"
#include <QDebug>

bool DecodeAudio::init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    bool intok = DecodeBase::init(stream, pktBuf, frmBuf);
    if (!intok) {
        return false;
    }

    auto par = stream->codecpar;

    m_oldPar.sampleFormat = static_cast<AVSampleFormat>(par->format);
    m_oldPar.sampleRate = par->sample_rate;
    int ret = av_channel_layout_copy(&m_oldPar.ch_layout, &par->ch_layout);
    if (ret != 0) {
        return false;
    }
    m_initialized = true;
    return true;
}

void DecodeAudio::decodingLoop() {
    if (!m_initialized) {
        return;
    }

    AVPktItem pktItem;
    AVFrmItem frmItem;
    while (!m_stop.load(std::memory_order_relaxed)) {
        // 进行seek
        int seekCnt = GlobalClock::instance().seekCnt();
        AVPktItem refPkt;
        bool need_flush_buffers = false;
        while (m_pktBuf->peekFirst(refPkt) && refPkt.seekCnt != seekCnt) {
            need_flush_buffers = true;
            m_pktBuf->pop(pktItem);
            av_packet_free(&pktItem.pkt);
        }
        if (need_flush_buffers) {
            avcodec_flush_buffers(m_codecCtx);
            need_flush_buffers = false;
        }

        // 从缓冲区取数据
        while (!pktItem.pkt && !m_pktBuf->pop(pktItem)) {
            if (m_stop.load(std::memory_order_relaxed)) {
                goto end;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        int ret = avcodec_send_packet(m_codecCtx, pktItem.pkt);

        if (ret == 0) {
            av_packet_free(&pktItem.pkt);
        } else if (ret == AVERROR_EOF) {
            // TODO : 处理音频时钟
            qDebug() << "pkt EOP";
            av_packet_free(&pktItem.pkt);
            continue;
        } else if (ret == AVERROR(EAGAIN)) {
            ;
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
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
}

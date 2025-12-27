// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "decodesubtitle.h"
#include "renderer/assrender.h"
#include <QDebug>

namespace {
    void handleTextSub(AVFrmItem &frmItem) {
        for (unsigned i = 0; i < frmItem.sub.num_rects; ++i) {
            AVSubtitleRect *subRect = frmItem.sub.rects[i];
            if (subRect->type == SUBTITLE_ASS) {
                ASSRender::instance().addEvent(subRect->ass, strlen(subRect->ass), frmItem.pts, frmItem.duration);
            } else if (subRect->type == SUBTITLE_TEXT) {
                // TODO
            }
        }
        avsubtitle_free(&frmItem.sub);
    }
}

bool DecodeSubtitle::init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf) {
    bool initok = DecodeBase::init(stream, pktBuf, frmBuf);
    if (!initok) {
        return false;
    }

    m_initialized = true;

    std::string subtitleHeader((char *)m_codecCtx->subtitle_header, m_codecCtx->subtitle_header_size);
    if (!subtitleHeader.empty())
        ASSRender::instance().init(subtitleHeader);

    return true;
}

void DecodeSubtitle::decodingLoop() {
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

        while (true) {
            int got_frame = 0;
            int ret = avcodec_decode_subtitle2(m_codecCtx, &frmItem.sub, &got_frame, pktItem.pkt);
            if (ret < 0) {
                qDebug() << "字幕解码出错";
                goto end;
            }

            if (got_frame) {
                const AVSubtitle &sub = frmItem.sub;
                frmItem.width = m_codecCtx->width;
                frmItem.height = m_codecCtx->height;
                frmItem.serial = pktItem.serial;
                frmItem.pts = sub.pts == AV_NOPTS_VALUE ? 0.0
                                                        : sub.pts / (double)AV_TIME_BASE + sub.start_display_time / 1000.0;
                frmItem.duration = (sub.end_display_time - sub.start_display_time) / 1000.0;

                if (sub.format == 0) { // 图形字幕
                    while (!m_frmBuf->push(frmItem)) {
                        if (m_stop.load(std::memory_order_relaxed)) {
                            goto end;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }                    
                }
                else {
                    handleTextSub(frmItem);
                }
            } else {
                av_packet_unref(pktItem.pkt);
                break;
            }
            av_packet_unref(pktItem.pkt);
        }
    }
end:
    av_packet_free(&pktItem.pkt);
    avsubtitle_free(&frmItem.sub);
}

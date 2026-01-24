// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later
#include "assrender.h"
#include <QDebug>

namespace {
    bool isAssFile(const std::string &s) {
        return QString::fromStdString(s).endsWith(".ass", Qt::CaseInsensitive);
    }
}

ASSRender::ASSRender(QObject *parent)
    : QObject{parent} {}

ASSRender &ASSRender::instance() {
    static ASSRender assr;
    return assr;
}

ASSRender::~ASSRender() {
    uninit();
}

bool ASSRender::init(const std::string &subFile) { // 通过字幕文件加载
    uninit();

    m_assLibrary = ass_library_init();
    if (!m_assLibrary)
        return false;

    ass_set_extract_fonts(m_assLibrary, 1); // 开启从 ASS 字幕文件中提取内嵌字体的功能

    if (isAssFile(subFile)) {
        m_track = ass_read_file(m_assLibrary, subFile.c_str(), NULL);
        if (!m_track)
            return false;
        m_initialized.store(true, std::memory_order_relaxed);
        return true;
    }

    // 不是ASS字幕，自动选择一条最佳字幕流
    return init(subFile, -1);
}

bool ASSRender::init(const std::string &mediaFile, int subStreamIdx) {
    uninit();

    // init ass
    m_assLibrary = ass_library_init();
    if (!m_assLibrary)
        return false;

    ass_set_extract_fonts(m_assLibrary, 1); // 开启从 ASS 字幕文件中提取内嵌字体的功能

    m_track = ass_new_track(m_assLibrary);
    if (!m_track)
        return false;

    // open file
    AVFormatContext *fmt = nullptr;
    int ret = avformat_open_input(&fmt, mediaFile.c_str(), nullptr, nullptr);
    if (ret < 0)
        goto fail;

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0 || subStreamIdx >= static_cast<int>(fmt->nb_streams))
        goto fail;

    if (subStreamIdx < 0) { // auto
        subStreamIdx = av_find_best_stream(fmt, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
        if (subStreamIdx < 0)
            goto fail;
    }
    if (!initFromDemux(fmt, subStreamIdx))
        goto fail;

    avformat_close_input(&fmt);
    m_initialized.store(true, std::memory_order_relaxed);
    return true;
fail:
    avformat_close_input(&fmt);
    return false;
}

bool ASSRender::uninit() {
    m_initialized.store(false, std::memory_order_relaxed);
    ass_free_track(m_track);
    m_track = nullptr;
    ass_renderer_done(m_assRenderer);
    m_assRenderer = nullptr;
    ass_library_done(m_assLibrary);
    m_assLibrary = nullptr;
    return true;
}

bool ASSRender::initialized() {
    return m_initialized.load(std::memory_order_relaxed);
}

bool ASSRender::addEvent(const char *data, int size, double startTime, double duration) {
    if (!m_initialized.load(std::memory_order_relaxed))
        return false;

    long long st = startTime * 1000;
    long long dur = duration * 1000;
    ass_process_chunk(m_track, data, size, st, dur);
    return true;
}

int ASSRender::renderFrame(std::vector<std::vector<uint8_t>> &dataArr, std::vector<QRect> &rects, const QSize &videoSize, double pts) {
    if (!m_initialized.load(std::memory_order_relaxed))
        return 0;

    if (!m_assRenderer) { // 初始化渲染器
        m_assRenderer = ass_renderer_init(m_assLibrary);
        if (!m_assRenderer) {
            qDebug() << "ASS字幕渲染器创建失败";
            return 0;
        }
        // TODO: size不一致是否要重新初始化(在正确调用init与uninit的情况下应该不会出现不一致的问题)
        ass_set_storage_size(m_assRenderer, videoSize.width(), videoSize.height());
        ass_set_frame_size(m_assRenderer, videoSize.width(), videoSize.height());
        ass_set_fonts(m_assRenderer, NULL, "sans-serif", ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
    }

    const ASS_Image *img = ass_render_frame(m_assRenderer, m_track, (int)(pts * 1000), NULL);
    const ASS_Image *ptr = img;

    m_dirtyRectManager.init();
    while (ptr) {
        m_dirtyRectManager.addRect(QRect(ptr->dst_x, ptr->dst_y, ptr->w, ptr->h));
        ptr = ptr->next;
    }

    // 清空 / 重置
    rects = m_dirtyRectManager.getRects();
    dataArr.resize(m_dirtyRectManager.size());

    if (m_dirtyRectManager.size() <= 0) {
        return 0;
    }

    for (size_t i = 0; i < dataArr.size(); ++i) {
        const QRect &rect = rects[i];
        rect.size();
        dataArr[i].assign(rect.width() * rect.height() * 4, 0);
    }

    while (img) {
        blendSingleOnly(dataArr, rects, img);
        img = img->next;
    }

    for (auto &buffer : dataArr) {
        unpremultiplyAlpha(buffer); // 反预乘
    }

    return rects.size();
}

bool ASSRender::initFromDemux(AVFormatContext *fmt, int subStreamIdx) {
    int ret;
    AVStream *st = fmt->streams[subStreamIdx];
    const AVCodecDescriptor *dec_desc = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVPacket pkt;

    const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec)
        goto fail;

    dec_desc = avcodec_descriptor_get(st->codecpar->codec_id);
    if (dec_desc && !(dec_desc->props & AV_CODEC_PROP_TEXT_SUB))
        goto fail;

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        goto fail;

    ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
    if (ret < 0)
        goto fail;

    dec_ctx->pkt_timebase = st->time_base;
    ret = avcodec_open2(dec_ctx, nullptr, nullptr);
    if (ret < 0)
        goto fail;

    if (dec_ctx->subtitle_header) {
        ass_process_codec_private(m_track,
                                  reinterpret_cast<const char *>(dec_ctx->subtitle_header),
                                  dec_ctx->subtitle_header_size);
    }
    // MAYBE: else goto fail?

    // decode
    while (av_read_frame(fmt, &pkt) >= 0) {
        int got_subtitle;
        AVSubtitle sub{};

        if (pkt.stream_index == subStreamIdx) {
            ret = avcodec_decode_subtitle2(dec_ctx, &sub, &got_subtitle, &pkt);
            if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
                av_strerror(ret, errbuf, sizeof(errbuf));
                qDebug() << "Error decoding:" << errbuf << "(ignored)\n";
            } else if (got_subtitle) {
                const int64_t start_time = av_rescale_q(sub.pts, AV_TIME_BASE_Q, av_make_q(1, 1000));
                for (unsigned int i = 0; i < sub.num_rects; ++i) {
                    char *ass_line = sub.rects[i]->ass;
                    if (!ass_line)
                        break;
                    ass_process_chunk(m_track, ass_line, strlen(ass_line),
                                      start_time, sub.end_display_time);
                }
            }
        }
        av_packet_unref(&pkt);
        avsubtitle_free(&sub);
    }

    avcodec_free_context(&dec_ctx);
    return true;
fail:
    avcodec_free_context(&dec_ctx);
    return false;
}

// from https://github.com/libass/libass/blob/master/test/test.c

void ASSRender::unpremultiplyAlpha(std::vector<uint8_t> &buffer) {
    unsigned char *data = buffer.data();
    size_t pixelCount = buffer.size() / 4;
    Q_ASSERT(buffer.size() % 4 == 0);
    for (size_t i = 0; i < pixelCount; ++i) {
        size_t idx = i * 4;
        const unsigned char alpha = data[idx + 3];
        if (alpha && alpha < 255) {
            // For each color channel c:
            //   c = c / (255.0 / alpha)
            // but only using integers and a biased rounding offset
            const uint32_t offs = (uint32_t)1 << 15;
            uint32_t inv = ((uint32_t)255 << 16) / alpha + 1;
            data[idx + 0] = (data[idx + 0] * inv + offs) >> 16;
            data[idx + 1] = (data[idx + 1] * inv + offs) >> 16;
            data[idx + 2] = (data[idx + 2] * inv + offs) >> 16;
        }
    }
}

void ASSRender::blendSingleOnly(std::vector<std::vector<uint8_t>> &dataArr, std::vector<QRect> &rects, const ASS_Image *img) {
    QRect rect(img->dst_x, img->dst_y, img->w, img->h);
    int rect_idx = m_dirtyRectManager.findFirstIntersect(rect);
    if (rect_idx < 0)
        return;
    unsigned char r = img->color >> 24;
    unsigned char g = (img->color >> 16) & 0xFF;
    unsigned char b = (img->color >> 8) & 0xFF;
    unsigned char a = 255 - (img->color & 0xFF);

    const QRect &targetRect = rects[rect_idx];
    Q_ASSERT(targetRect.contains(rect, false)); // img 位于targetRect内，包括边缘

    // 计算局部偏移
    // img 在 targetRect 内部的起点坐标
    int offsetX = img->dst_x - targetRect.x();
    int offsetY = img->dst_y - targetRect.y();

    unsigned char *src = img->bitmap;
    unsigned char *dst = dataArr[rect_idx].data();
    int targetW = targetRect.width();

    for (int y = 0; y < img->h; ++y) {
        // 计算当前行在目标 buffer 中的起始位置
        // (y + offsetY) 是行索引，乘以 targetW 得到行首，再加上 offsetX 得到像素起点
        int dst_row_start = ((y + offsetY) * targetW + offsetX) * 4;

        for (int x = 0; x < img->w; ++x) {
            int idx = dst_row_start + (x * 4);

            unsigned k = ((unsigned)src[x]) * a;
            // For high-quality output consider using dithering instead;
            // this static offset results in biased rounding but is faster
            unsigned rounding_offset = 255 * 255 / 2;
            // If the original frame is not in premultiplied alpha, convert it beforehand or adjust
            // the blending code. For fully-opaque output frames there's no difference either way.
            dst[idx + 0] = (k * r + (255 * 255 - k) * dst[idx + 0] + rounding_offset) / (255 * 255);
            dst[idx + 1] = (k * g + (255 * 255 - k) * dst[idx + 1] + rounding_offset) / (255 * 255);
            dst[idx + 2] = (k * b + (255 * 255 - k) * dst[idx + 2] + rounding_offset) / (255 * 255);
            dst[idx + 3] = (k * 255 + (255 * 255 - k) * dst[idx + 3] + rounding_offset) / (255 * 255);
        }
        src += img->stride;
    }
}

// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later
#include "assrender.h"
#include <QDebug>

ASSRender::ASSRender(QObject *parent)
    : QObject{parent} {}

ASSRender &ASSRender::instance() {
    static ASSRender assr;
    return assr;
}

ASSRender::~ASSRender() {
    uninit();
}

bool ASSRender::init(const std::string &subtitleHeader) {
    uninit();

    m_assLibrary = ass_library_init();
    if (!m_assLibrary) {
        qDebug() << "ASS字幕创建失败";
        return false;
    }

    ass_set_extract_fonts(m_assLibrary, 1); // 开启从 ASS 字幕文件中提取内嵌字体的功能

    m_track = ass_read_memory(m_assLibrary, const_cast<char *>(subtitleHeader.c_str()), subtitleHeader.size(), nullptr);
    if (!m_track) {
        qDebug() << "ASS字幕轨道创建失败";
        return false;
    }

    m_initialized.store(true, std::memory_order_relaxed);
    return true;
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

int ASSRender::renderFrame(image_t &image, double pts) {
    if (!m_initialized.load(std::memory_order_relaxed))
        return 0;

    if (!m_assRenderer) { // 初始化渲染器
        m_assRenderer = ass_renderer_init(m_assLibrary);
        if (!m_assRenderer) {
            qDebug() << "ASS字幕渲染器创建失败";
            return 0;
        }

        ass_set_storage_size(m_assRenderer, image.width, image.height);
        ass_set_frame_size(m_assRenderer, image.width, image.height);
        ass_set_fonts(m_assRenderer, NULL, "sans-serif", ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
    }
    memset(image.buffer, 0, image.height * image.stride);// HACK: ass也是多个小矩形叠加在一个图上的，这儿先全部清空了
    ASS_Image *img = ass_render_frame(m_assRenderer, m_track, (int)(pts * 1000), NULL);
    int blendedNum = blend(&image, img);

    return blendedNum;
}

//=====================image_t=====================
// for https://github.com/libass/libass/blob/master/test/test.c

int ASSRender::blend(image_t *frame, ASS_Image *img) {
    int cnt = 0;
    while (img) {
        blend_single(frame, img);
        ++cnt;
        img = img->next;
    }
    // printf("%d images blended\n", cnt);
    if (cnt == 0) {
        return 0;
    }

    // Convert from pre-multiplied to straight alpha
    // (not needed for fully-opaque output)
    for (int y = 0; y < frame->height; y++) {
        unsigned char *row = frame->buffer + y * frame->stride;
        for (int x = 0; x < frame->width; x++) {
            const unsigned char alpha = row[4 * x + 3];
            if (alpha) {
                // For each color channel c:
                //   c = c / (255.0 / alpha)
                // but only using integers and a biased rounding offset
                const uint32_t offs = (uint32_t)1 << 15;
                uint32_t inv = ((uint32_t)255 << 16) / alpha + 1;
                row[x * 4 + 0] = (row[x * 4 + 0] * inv + offs) >> 16;
                row[x * 4 + 1] = (row[x * 4 + 1] * inv + offs) >> 16;
                row[x * 4 + 2] = (row[x * 4 + 2] * inv + offs) >> 16;
            }
        }
    }
    return cnt;
}

void ASSRender::blend_single(image_t *frame, ASS_Image *img) {
    unsigned char r = img->color >> 24;
    unsigned char g = (img->color >> 16) & 0xFF;
    unsigned char b = (img->color >> 8) & 0xFF;
    unsigned char a = 255 - (img->color & 0xFF);

    unsigned char *src = img->bitmap;
    unsigned char *dst = frame->buffer + img->dst_y * frame->stride + img->dst_x * 4;

    for (int y = 0; y < img->h; ++y) {
        for (int x = 0; x < img->w; ++x) {
            unsigned k = ((unsigned)src[x]) * a;
            // For high-quality output consider using dithering instead;
            // this static offset results in biased rounding but is faster
            unsigned rounding_offset = 255 * 255 / 2;
            // If the original frame is not in premultiplied alpha, convert it beforehand or adjust
            // the blending code. For fully-opaque output frames there's no difference either way.
            dst[x * 4 + 0] = (k * r + (255 * 255 - k) * dst[x * 4 + 0] + rounding_offset) / (255 * 255);
            dst[x * 4 + 1] = (k * g + (255 * 255 - k) * dst[x * 4 + 1] + rounding_offset) / (255 * 255);
            dst[x * 4 + 2] = (k * b + (255 * 255 - k) * dst[x * 4 + 2] + rounding_offset) / (255 * 255);
            dst[x * 4 + 3] = (k * 255 + (255 * 255 - k) * dst[x * 4 + 3] + rounding_offset) / (255 * 255);
        }
        src += img->stride;
        dst += frame->stride;
    }
}

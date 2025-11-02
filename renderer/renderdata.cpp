// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "renderdata.h"

#include <QDateTime>

namespace {
    std::array<unsigned int, 3> bitSize2GLPara(int size) {
        if (size == 8)
            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
        return {GL_R16, GL_RED, GL_UNSIGNED_SHORT};
    }

    // 该函数用于辅助updateFrm函数，使用时确保排除了RGB_PACKED,RGBA_PACKED，每个平面都是独立的
    RenderData::PixFormat desc2PixFormat(const AVPixFmtDescriptor *desc) {
        if (desc->flags & AV_PIX_FMT_FLAG_RGB) {
            return (desc->flags & AV_PIX_FMT_FLAG_ALPHA) ? RenderData::RGBA_PLANAR : RenderData::RGB_PLANAR;
        }

        if (desc->flags & AV_PIX_FMT_FLAG_ALPHA) {
            return desc->nb_components == 2 ? RenderData::YA : RenderData::YUVA;
        }

        return desc->nb_components == 1 ? RenderData::Y : RenderData::YUV;
    }
    /**
     * @param data 数据首地址
     * @param originLen 不考虑对齐时的数据长度
     * @param nowLen 对齐后的数据长度
     */
    int getAlignment(const void *data, size_t originLen, size_t nowLen) {
        // alignment 只能是 1,2,4,8 之一
        int possibleAlignments[] = {8, 4, 2, 1};
        uintptr_t dataVal = reinterpret_cast<uintptr_t>(data);
        for (int align : possibleAlignments) {
            if (dataVal % align != 0 || nowLen % align != 0)
                continue;
            if (((originLen + align - 1) / align * align) == nowLen)
                return align;
        }
        return 1; // fallback
    }

    template <typename T>
    void ensure_size(std::vector<T>& v, size_t n) {
        if (v.size() < n) v.resize(n);
    }

    template <typename T>
    void ensure_size(std::vector<T>& v, size_t n, T val) {
        if (v.size() < n) v.assign(n, val);
    }
}

bool RenderData::isEachComponentInSeparatePlane(const AVPixFmtDescriptor *desc) {
    if (!desc)
        return false;

    std::array<uint8_t, 4> planes = {0, 0, 0, 0};
    for (int i = 0; i < desc->nb_components; ++i) {
        planes[desc->comp[i].plane]++;
    }

    return std::count(planes.begin(), planes.end(), 1) == desc->nb_components;
}

bool RenderData::isComponentInSeparatePlane(int c, const AVPixFmtDescriptor *desc) {
    if (!desc)
        return false;

    std::array<uint8_t, 4> planes = {0, 0, 0, 0};
    for (int i = 0; i < desc->nb_components; ++i) {
        planes[desc->comp[i].plane]++;
    }

    return planes[desc->comp[c].plane] == 1;
}

void RenderData::splitFrameComponentsToPlanes(const AVPixFmtDescriptor *desc) {
    for (int c = 0; c < desc->nb_components; ++c) {
        // TODO ： 多线程
        splitComponentToPlane(c, desc);
    }
}

void RenderData::splitComponentToPlane(int c, const AVPixFmtDescriptor *desc) {
    if (!desc || !frmItem.frm) {
        return;
    }

    AVFrame *frm = frmItem.frm;
    int width = frm->width;
    int height = frm->height;

    // UV的大小需要重新即使
    if (!(desc->flags & AV_PIX_FMT_FLAG_RGB) && (c == 1 || c == 2)) { // 在两个分量YA的情况下，log2_chroma_x=0,所有对A大小重新计算也没问题
        width = AV_CEIL_RSHIFT(frm->width, desc->log2_chroma_w);
        height = AV_CEIL_RSHIFT(frm->height, desc->log2_chroma_h);
    }

    void *plane_ptr;
    const uint8_t *frmData[4];
    int linesize[4];
    int read_pal_component = (desc->flags & AV_PIX_FMT_FLAG_PAL);
    int dst_element_size = 2;

    std::copy(frm->linesize, frm->linesize + 4, linesize);
    std::copy(frm->data, frm->data + 4, frmData);

    Q_ASSERT(desc->comp[c].depth <= 16);

    dst_element_size = 2;
    componentBitSize[c] = 16;
    componentSizeArr[c] = {width, height};
    linesizeArr[c] = width;
    dst16[c].resize(width * height);
    dataArr[c] = (uint8_t *)dst16[c].data();

    plane_ptr = (void *)dst16[c].data();
    // av_read_image_line2(plane_ptr, frmData, linesize, desc,
    //                     0, 0, c, width * height,
    //                     read_pal_component, dst_element_size);

    // 每行末尾可能存在填充，不能连续读
    // TODO ： 多线程
    for (int y = 0; y < height; ++y) {
        // 目标行首地址：dst16[c].data() + y * width * dst_element_size
        void *line_ptr = (uint8_t *)plane_ptr + y * width * dst_element_size;

        av_read_image_line2(line_ptr, frmData, linesize, desc,
                            0, // x 起点
                            y, // y 行号
                            c, // 要读的分量
                            width,
                            read_pal_component,
                            dst_element_size);
    }
}

// TODO ： 只在类型不一样时重新初始化参数，否则只初始化必要参数
void RenderData::updateFormat(AVFrmItem &newItem) {
    if (!newItem.frm)
        return;
    if (frmItem.frm != nullptr)
        av_frame_free(&frmItem.frm);

    frmItem = newItem;
    newItem.frm = nullptr;
    AVFrame *frm = frmItem.frm;

    AVPixelFormat avFmt = (AVPixelFormat)frm->format;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(avFmt);
    uint64_t flags = desc->flags;

    // 可以直接上传到opengl
    if (GLParaMap.find(avFmt) != GLParaMap.end()) {
        pixFormat = (flags & AV_PIX_FMT_FLAG_ALPHA) ? PixFormat::RGBA_PACKED : PixFormat::RGB_PACKED;
        GLParaArr[0] = GLParaMap[avFmt];
        componentSizeArr[0] = {frm->width, frm->height};
        dataArr[0] = frm->data[0]; // or frm->data[desc->comp[0|1|2|3].plane]?
        int bytes_per_pixel = (av_get_padded_bits_per_pixel(desc) / 8);
        linesizeArr[0] = frm->linesize[0] / bytes_per_pixel;
        alignment = getAlignment(dataArr[0], linesizeArr[0] * bytes_per_pixel, frm->linesize[0]);
        return;
    }

    alignment = 1; // HACK: 剩下的情况用1也行，会损失一些性能，暂时先这样了

    int maxDepth = 0;
    for (int i = 0; i < desc->nb_components; ++i) {
        maxDepth = std::max(maxDepth, desc->comp[i].depth);
    }

    if (flags & AV_PIX_FMT_FLAG_FLOAT || maxDepth > 16) { // TODO : sws
        qDebug() << "Float 和 分量大于16bit暂未处理";
        return;
    }

    pixFormat = desc2PixFormat(desc);

    // splitFrameComponentsToPlanes(desc);
    // updateGLParaArr(pixFormat);
    // return;

    // 强行将每个分量拆分到独立的平面上
    if (flags & AV_PIX_FMT_FLAG_BE || flags & AV_PIX_FMT_FLAG_BAYER ||
        flags & AV_PIX_FMT_FLAG_BITSTREAM || flags & AV_PIX_FMT_FLAG_PAL ||
        flags & AV_PIX_FMT_FLAG_XYZ) {
        splitFrameComponentsToPlanes(desc);
        updateGLParaArr(pixFormat);
        return;
    }

    // 非完整类型
    // qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < desc->nb_components; ++i) {
        int tmp = desc->comp[i].depth;
        if (tmp != 8 && tmp != 16) {
            splitFrameComponentsToPlanes(desc);
            updateGLParaArr(pixFormat);

            // TODO : 多线程
            for (int j = 0; j < desc->nb_components; ++j) {
                for (auto &x : dst16[j])
                    x <<= (16 - desc->comp[j].depth); // 从[0,2^bits-1)映射到[0,2^16-1),精度比纯数学方法稍差
                // x = (x * 65535 + 511) / 1023;
            }
            // qDebug() << QDateTime::currentMSecsSinceEpoch() - now;
            return;
        }
    }

    // 将每个分量拆分到独立的平面上，已经在单独平面上的不需要重新拆，直接从frm->data里获取
    for (int i = 0; i < desc->nb_components; ++i) {
        if (isComponentInSeparatePlane(i, desc)) {
            componentBitSize[i] = desc->comp[i].depth;
            if (!(desc->flags & AV_PIX_FMT_FLAG_RGB) && (i == 1 || i == 2)) {
                componentSizeArr[i] = {AV_CEIL_RSHIFT(frm->width, desc->log2_chroma_w), AV_CEIL_RSHIFT(frm->height, desc->log2_chroma_h)};
            } else {
                componentSizeArr[i] = {frm->width, frm->height};
            }
            dataArr[i] = frm->data[desc->comp[i].plane];
            linesizeArr[i] = frm->linesize[desc->comp[i].plane] / (desc->comp[i].depth / 8);
        } else {
            splitComponentToPlane(i, desc);
        }
    }
    updateGLParaArr(pixFormat);
}

void RenderData::reset() {
    mutex.lock();
    if (frmItem.frm)
        av_frame_free(&frmItem.frm);
    frmItem.pts = renderedTime = std::numeric_limits<double>::quiet_NaN();
    frmItem.duration = renderedTime = std::numeric_limits<double>::quiet_NaN();
    mutex.unlock();
}

void RenderData::updateGLParaArr(PixFormat fmt) {
    if (fmt == RenderData::Y || fmt == RenderData::YA) {
        GLParaArr[0] = bitSize2GLPara(componentBitSize[0]); // Y
        if (fmt == RenderData::YA) {
            GLParaArr[1] = bitSize2GLPara(componentBitSize[1]); // A
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            GLParaArr[i] = bitSize2GLPara(componentBitSize[i]); // RGB | YUV
        }
        if (fmt == RenderData::RGBA_PLANAR || fmt == RenderData::YUVA) {
            GLParaArr[3] = bitSize2GLPara(componentBitSize[3]); // A
        }
    }
}

//======SubRenderData===========//
void SubRenderData::reset() {
    mutex.lock();
    if (subSwsCtx) { // MABEY: 需要free吗
        sws_freeContext(subSwsCtx);
        subSwsCtx = nullptr;
    }
    avsubtitle_free(&frmItem.sub);
    subtitleType = SUBTITLE_NONE;
    frmItem.pts = std::numeric_limits<double>::quiet_NaN();
    frmItem.duration = std::numeric_limits<double>::quiet_NaN();
    x.clear();
    y.clear();
    w.clear();
    h.clear();
    linesizeArr.clear();
    dataArr.clear();
    forceRefresh = false;
    uploaded = false;
    mutex.unlock();
}

void SubRenderData::updateFormat(AVFrmItem &newItem, int videoWidth, int videoHeight) {
    avsubtitle_free(&frmItem.sub);

    if (subtitleType == SUBTITLE_BITMAP) { // 一些蓝光字幕会使用空白字幕来结束显示当前字幕
        uploaded = false;
    }

    AVSubtitle &sub = newItem.sub;
    frmItem = newItem;

    linesizeArr.resize(sub.num_rects);
    x.resize(sub.num_rects);
    y.resize(sub.num_rects);
    w.resize(sub.num_rects);
    h.resize(sub.num_rects);
    dataArr.resize(sub.num_rects);

    for (unsigned i = 0; i < sub.num_rects; ++i) {
        AVSubtitleRect *subRect = sub.rects[i];
        subtitleType = subRect->type;
        if (subRect->type == SUBTITLE_BITMAP) {
            subRect->x = std::clamp(subRect->x, 0, videoWidth);
            subRect->y = std::clamp(subRect->y, 0, videoHeight);
            subRect->w = std::clamp(subRect->w, 0, videoWidth - subRect->x);
            subRect->h = std::clamp(subRect->h, 0, videoHeight - subRect->y);

            x[i] = subRect->x;
            y[i] = subRect->y;
            w[i] = subRect->w;
            h[i] = subRect->h;
            if (subRect->h == 0 || subRect->w == 0) {
                continue;
            }
            linesizeArr[i] = subRect->w;
            dataArr[i].resize(subRect->h * subRect->w * 4);

            subSwsCtx = sws_getCachedContext(subSwsCtx,
                                             subRect->w, subRect->h, AV_PIX_FMT_PAL8,
                                             subRect->w, subRect->h, AV_PIX_FMT_RGBA,
                                             0, NULL, NULL, NULL);

            int dstStride[1] = {linesizeArr[i] * 4};
            uint8_t *dst[1] = {dataArr[i].data()};
            sws_scale(subSwsCtx, (const uint8_t *const *)subRect->data, subRect->linesize, 0, subRect->h, dst, dstStride);
        } else if (subRect->type == SUBTITLE_TEXT) {
            qDebug() << "TEXT:" << newItem.pts << newItem.duration << subRect->text;
        } else if (subRect->type == SUBTITLE_ASS) {
            ASSRender::instance().addEvent(subRect->ass, strlen(subRect->ass), frmItem.pts, frmItem.duration);
            assImage.height = videoHeight;
            assImage.width = videoWidth;
            assImage.stride = videoWidth * 4;
        }
    }
}

void SubRenderData::updateASSImage(double pts) {
    if (subtitleType != SUBTITLE_ASS)
        return;

    ensure_size(dataArr, 1);
    ensure_size(linesizeArr, 1);
    ensure_size(x, 1);
    ensure_size(y, 1);
    ensure_size(w, 1);
    ensure_size(h, 1);

    ensure_size(dataArr.front(), assImage.height * assImage.stride, (uint8_t)0);

    assImage.buffer = dataArr.front().data();

    if (ASSRender::instance().renderFrame(assImage, pts) >= 1) {
        linesizeArr[0] = assImage.width; // 在OpenGL中linesizeArr是像素个数
        x[0] = y[0] = 0;
        w[0] = assImage.width, h[0] = assImage.height;
    }
    uploaded = false;
}

// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RENDERDATA_H
#define RENDERDATA_H
#include "assrender.h"
#include "compat/compat.h"
#include "types/types.h"
#include <QMutex>
#include <QOpenGLFunctions_3_3_Core>
#include <QSize>
#include <array>
#include <map>
#include <qopenglext.h>
#include <vector>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
AZ_EXTERN_C_END

struct RenderData {
    enum PixFormat {
        // 可直接上传opengl，通过GLPara获取参数
        NONE = -1,
        RGB_PACKED,
        RGBA_PACKED,

        // 以下类型每个分量独占一个平面
        RGB_PLANAR,
        RGBA_PLANAR,
        Y,
        YA,
        YUV,
        YUVA,
    };

    /**
     * 用于直接上传OpengGL的参数
     * 依次为：internalformat，format，type
     */
    inline static std::map<AVPixelFormat, std::array<unsigned int, 3>> GLParaMap{
        {AV_PIX_FMT_RGB24, {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE}},                        ///< packed RGB 8:8:8, 24bpp, RGBRGB...
        {AV_PIX_FMT_BGR24, {GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE}},                        ///< packed RGB 8:8:8, 24bpp, BGRBGR...
        {AV_PIX_FMT_BGR8, {GL_R3_G3_B2, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV}},           ///< packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
        {AV_PIX_FMT_RGB8, {GL_R3_G3_B2, GL_RGB, GL_UNSIGNED_BYTE_3_3_2}},               ///< packed RGB 3:3:2,  8bpp, (msb)3R 3G 2B(lsb)
        {AV_PIX_FMT_ARGB, {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8}},                ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
        {AV_PIX_FMT_RGBA, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}},                       ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        {AV_PIX_FMT_ABGR, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8}},                ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
        {AV_PIX_FMT_BGRA, {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE}},                       ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
        {AV_PIX_FMT_RGB48LE, {GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT}},                    ///< packed RGB 16:16:16, 48bpp, 16R, 16G, 16B, the 2-byte value for each R/G/B component is stored as little-endian
        {AV_PIX_FMT_RGB565LE, {GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5}},              ///< packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), little-endian
        {AV_PIX_FMT_RGB555LE, {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV}},    ///< packed RGB 5:5:5, 16bpp, (msb)1X 5R 5G 5B(lsb), little-endian, X=unused/undefined
        {AV_PIX_FMT_BGR565LE, {GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV}},          ///< packed BGR 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), little-endian
        {AV_PIX_FMT_BGR555LE, {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV}},    ///< packed BGR 5:5:5, 16bpp, (msb)1X 5B 5G 5R(lsb), little-endian, X=unused/undefined
        {AV_PIX_FMT_RGB444LE, {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV}},      ///< packed RGB 4:4:4, 16bpp, (msb)4X 4R 4G 4B(lsb), little-endian, X=unused/undefined
        {AV_PIX_FMT_BGR444LE, {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV}},      ///< packed BGR 4:4:4, 16bpp, (msb)4X 4B 4G 4R(lsb), little-endian, X=unused/undefined
        {AV_PIX_FMT_BGR48LE, {GL_RGB16, GL_BGR, GL_UNSIGNED_SHORT}},                    ///< packed RGB 16:16:16, 48bpp, 16B, 16G, 16R, the 2-byte value for each R/G/B component is stored as little-endian
        {AV_PIX_FMT_RGBA64LE, {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT}},                 ///< packed RGBA 16:16:16:16, 64bpp, 16R, 16G, 16B, 16A, the 2-byte value for each R/G/B/A component is stored as little-endian
        {AV_PIX_FMT_BGRA64LE, {GL_RGBA16, GL_BGRA, GL_UNSIGNED_SHORT}},                 ///< packed RGBA 16:16:16:16, 64bpp, 16B, 16G, 16R, 16A, the 2-byte value for each R/G/B/A component is stored as little-endian
        {AV_PIX_FMT_0RGB, {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8}},                ///< packed RGB 8:8:8, 32bpp, XRGBXRGB...   X=unused/undefined
        {AV_PIX_FMT_RGB0, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}},                       ///< packed RGB 8:8:8, 32bpp, RGBXRGBX...   X=unused/undefined
        {AV_PIX_FMT_0BGR, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8}},                ///< packed BGR 8:8:8, 32bpp, XBGRXBGR...   X=unused/undefined
        {AV_PIX_FMT_BGR0, {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE}},                       ///< packed BGR 8:8:8, 32bpp, BGRXBGRX...   X=unused/undefined
        {AV_PIX_FMT_X2RGB10LE, {GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}}, ///< packed RGB 10:10:10, 30bpp, (msb)2X 10R 10G 10B(lsb), little-endian, X=unused/undefined
        {AV_PIX_FMT_X2BGR10LE, {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}}, ///< packed BGR 10:10:10, 30bpp, (msb)2X 10B 10G 10R(lsb), little-endian, X=unused/undefined
    };

    QMutex mutex; // 锁

    AVFrmItem frmItem;
    double renderedTime; // 实际渲染到FBO的时间(相对现实时间，秒)

    PixFormat pixFormat = PixFormat::NONE;

    // 每个分量按Y|YA|YUV|YUVA|RGB|RGBA的顺序依次排列
    std::vector<std::vector<uint16_t>> dst16{4};
    uint8_t componentBitSize[4]{0, 0, 0, 0}; // 分量的大小(bit)
    // 以下三个数组为OpenGL初始化和更新纹理使用
    std::array<unsigned int, 3> GLParaArr[4]{};
    QSize componentSizeArr[4]{};
    uint8_t *dataArr[4]{};
    int linesizeArr[4]{}; // 每行实际存储的像素数 = [有效 + 填充]
    int alignment = 1;    // 内存中每个像素行起始处的对齐要求(1,2,4,8)

    // 每个分量是否都单独在一个平面上
    bool isEachComponentInSeparatePlane(const AVPixFmtDescriptor *desc);

    // 指定分量是否单独在一个平面上
    bool isComponentInSeparatePlane(int c, const AVPixFmtDescriptor *desc);

    // 将frm的每个分量拆分到单独平面
    void splitFrameComponentsToPlanes(const AVPixFmtDescriptor *desc);

    // 将frm的单个分量拆分到单独平面
    void splitComponentToPlane(int c, const AVPixFmtDescriptor *desc);

    // 根据frm重新更新格式
    void updateFormat(AVFrmItem &newItem);
    void reset();

    void updateGLParaArr(RenderData::PixFormat fmt);

    RenderData() : mutex() { reset(); }
    ~RenderData() {
        if (!frmItem.frm) {
            av_frame_free(&frmItem.frm);
        }
    }
};

struct SubRenderData {
    AVFrmItem frmItem;
    AVSubtitleType subtitleType = SUBTITLE_NONE;
    QMutex mutex; // 锁
    SwsContext *subSwsCtx = nullptr;

    std::vector<std::vector<uint8_t>> dataArr;
    std::vector<int> linesizeArr; // 每行实际存储的像素数 = [有效 + 填充]
    std::vector<int> x;
    std::vector<int> y;
    std::vector<int> w;
    std::vector<int> h;
    bool uploaded = false;
    bool forceRefresh = false; // 用于通知videoRender强制清理旧数据

    image_t assImage{};

    void reset();
    void clear();
    // 更新图形字幕
    void updateBitmapImage(AVFrmItem &newItem, int videoWidth, int videoHeight);

    // 更新ASS字幕
    void updateASSImage(double pts, int videoWidth, int videoHeight);

    SubRenderData() : mutex() { reset(); }
    ~SubRenderData() {
        avsubtitle_free(&frmItem.sub);
    }
};

#endif // RENDERDATA_H

// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef ASSRENDER_H
#define ASSRENDER_H

#include "ass/ass.h"
#include "compat/compat.h"
#include "utils/dirtyrectmanager.h"
#include <QObject>
#include <QRect>
#include <QSize>
#include <atomic>
#include <string>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
AZ_EXTERN_C_END

struct image_t {
    int width, height, stride; // 宽度，高度，每行字节数(注意不是每行像素数)
    unsigned char *buffer;     // RGBA32
};

class ASSRender : public QObject {

    Q_OBJECT
    explicit ASSRender(QObject *parent = nullptr);
    ~ASSRender();

    ASSRender(const ASSRender &) = delete;
    ASSRender &operator=(const ASSRender &) = delete;

public:
    static ASSRender &instance();

    bool init(const std::string &subFile); // 通过字幕文件加载
    /**
     * @param subStreamIdx 流ID，-1表示自动选中最佳字幕流
     * @note subStreamIdx 指 Demux全局的流ID
     */
    bool init(const std::string &mediaFile, int subStreamIdx);
    bool uninit();

    bool initialized();

    /**
     * 添加一条事件
     * @note data的格式为：ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text
     * @param startTime 开始时间 秒
     * @param duration 持续时间 秒
     */
    bool addEvent(const char *data, int size, double startTime, double duration);

    // 渲染一帧并获取矩形个数
    const ASS_Image *getASSImage(size_t &size, const QSize &videoSize, double pts);

    /**
     * 渲染一帧到dataArr里
     * @warning 请确保assImg是最后一次通过getASSImage()获取的
     * @return 实际渲染的矩形个数,>=1为有效帧
     */
    int renderFrame(std::vector<std::vector<uint8_t>> &dataArr, std::vector<QRect> &rects, const ASS_Image *assImg);
signals:

private:
    ASS_Library *m_assLibrary{nullptr};
    ASS_Renderer *m_assRenderer{nullptr};
    ASS_Track *m_track{nullptr};
    std::atomic<bool> m_initialized{false};
    DirtyRectManager m_dirtyRectManager; // 用于脏矩阵合成

private:
    bool initFromDemux(AVFormatContext *fmt, int subStreamIdx);
    void unpremultiplyAlpha(std::vector<uint8_t> &buffer);
    void blendSingleOnly(std::vector<std::vector<uint8_t>> &dataArr, const ASS_Image *img);
};

#endif // ASSRENDER_H

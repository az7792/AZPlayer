// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef ASSRENDER_H
#define ASSRENDER_H

#include "ass/ass.h"
#include <QObject>
#include <atomic>
#include <string>

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

    bool init(const std::string &subtitleHeader);
    bool uninit();

    bool initialized();

    /**
     * 添加一条事件
     * @note data的格式为：ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text
     * @param startTime 开始时间 秒
     * @param duration 持续时间 秒
     */
    bool addEvent(const char *data, int size, double startTime, double duration);

    /**
     * 渲染一帧到image里
     * 请确保image内的参数设置正确
     * @return 实际渲染的帧数,>=1为有效帧
     */
    int renderFrame(image_t &image, double pts);
signals:

private:
    ASS_Library *m_assLibrary{nullptr};
    ASS_Renderer *m_assRenderer{nullptr};
    ASS_Track *m_track{nullptr};
    std::atomic<bool> m_initialized{false};

private:
    static int blend(image_t *frame, ASS_Image *img);
    static void blend_single(image_t *frame, ASS_Image *img);
};

#endif // ASSRENDER_H

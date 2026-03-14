// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "compat/compat.h"
#include "types/ptrs.h"
#include "utils/spscbuffer.h"
#include <QObject>
#include <thread>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
AZ_EXTERN_C_END

struct ma_device;

class AudioPlayer : public QObject {
    Q_OBJECT
private:
    using ma_uint32 = uint32_t;

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    // 初始化
    bool init(const AVCodecParameters *codecParams, sharedFrmQueue frmBuf);
    // 回到未初始化状态
    bool uninit();

    // 启动
    void start();
    // 退出
    void stop();

    void togglePaused();

    double volume() const;
    void setVolume(double newVolume);

signals:
    void seeked();
    void playedOneFrame(); // 播放了一帧(仅没有视频流 或是 seek 时发射该信号)

private:
    sharedFrmQueue m_frmBuf;

    ma_device *m_audioDevice = nullptr; // 音频设备 NOTE: 生命周期与AudioPlayer一致，不要在init或者uninit里重新构造/析构
    SwrContext *m_swrContext = nullptr; // 用于重采样的上下文
    SPSCBuffer *m_pcmBuffer = nullptr;  // PCM buffer，用于在音频回调时使用

    // 重采样缓冲区
    uint8_t **m_swrBuffer = nullptr;
    int m_swrBufferSize = 0;

    // 一个完整AVFrame 的 PCM 数据
    AVFrmItem m_frmItem{};
    int m_pcmDataSize;
    uint8_t *m_pcmDataPtr = nullptr;
    int m_pcmDataIndex;
    int m_pcmFrameSize; // 一个PCM帧的字节大小

    // ==== FFmmpeg的音频参数 ====
    AudioPar m_oldPar; // 原始音频参数
    AudioPar m_swrPar; // 重采样音频参数，未进行重采样时与m_oldPar一致

    bool m_initialized = false; // 是否已经初始化
    int m_serial = 0;
    std::thread m_thread;
    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_paused{false};
    bool m_forceRefresh{false};
    double m_volume = 1.0;

private:
    bool getFrm(AVFrmItem &item);

    void playerLoop();
    void writePCM();

    // 从队列获取一帧并更新 PCM 数据
    bool updatePcmFromFrameQueue();

    static void miniaudio_data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);
};

#endif // AUDIOPLAYER_H

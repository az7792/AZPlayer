#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "utils.h"
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QObject>
#include <QThread>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

class AudioPlayer : public QObject {
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    // 初始化
    bool init(const AVCodecParameters *codecParams, sharedFrmQueue frmBuf);
    // 回到未初始化状态
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    //  开始播放
    bool play();

    // 暂停播放
    bool pause();

private:
    sharedFrmQueue m_frmBuf;
    /**
     * 主要用于在seek时清零QAudioSink::processedUSecs()计数器
     * 因为不方便真正清零QAudioSink::processedUSecs()计时器，因此采用补偿的方式清零，
     * QAudioSink::processedUSecs() - m_offsetTime 即为seek后处理的数据量(微秒)
     */
    qint64 m_offsetTime = 0;
    qint64 m_totalTime = 0; // 表示从0播放到seek节点的音频帧所花费的时间(微秒)，seek时会设置该值

    QAudioSink *m_audioSink = nullptr;     // 向音频输出设备发送音频数据的接口
    QAudioDevice *m_audioDevice = nullptr; // 用于播放音频的设备
    QIODevice *m_audioIO = nullptr;        // 音频数据来源
    QAudioFormat m_format;                 // 播放时使用的音频格式
    SwrContext *m_swrContext = nullptr;    // 用于重采样的上下文

    // 缓冲区
    uint8_t **m_swrBuffer = nullptr;
    int m_swrBufferSize = 0;

    AudioPar m_oldPar; // 原始音频参数
    AudioPar m_swrPar; // 重采样音频参数，未进行重采样时与m_oldPar一致

    bool m_initialized = false;  // 是否已经初始化
    QThread *m_thread = nullptr; // AudioSink需要使用QTimer，这而不能用std::thread
    std::atomic<bool> m_stop{true};

private:
    void playerLoop();
    /**
     * 写入一帧数据
     * @warning 方法会阻塞线程
     */
    qint64 write(AVFrmItem *item);

    /**
     * FFmpeg通道布局 -> Qt通道格式
     * @param maskIsEqual 只要在使用mask匹配，且maske是超集但不相等时回传false
     */
    QAudioFormat::ChannelConfig FFmpegToQtChannelConfig(const AVChannelLayout &ch_layout, bool *maskIsEqual = nullptr);
    // Qt采样格式 -> FFmpeg采样格式
    AVSampleFormat QtToFFmpegSampleFormat(const QAudioFormat::SampleFormat &sampleFormat);
    // 通道数量 -> Qt通道格式
    QAudioFormat::ChannelConfig channelCountToQtChannelConfig(int nbChannels);
    // FFmpeg mask -> Qt ChannelConfig
    QAudioFormat::ChannelConfig FFmpegMaskToQtChannelConfig(uint64_t ffmpegMask, bool *maskIsEqual = nullptr);
    // Qt ChannelConfig -> FFmpeg mask
    uint64_t QtChannelConfigToFFmpegMask(QAudioFormat::ChannelConfig qtConfig);
};

#endif // AUDIOPLAYER_H

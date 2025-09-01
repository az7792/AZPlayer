#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QObject>

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
    bool init(const AVCodecParameters *codecParams);
    // 回到未初始化状态
    void uninit();
    /**
     * 写入一帧数据
     * @warning 方法会阻塞线程
     */
    qint64 write(AVFrame *frm);
public slots:

signals:

private:
    QAudioSink *m_audioSink = nullptr;     // 向音频输出设备发送音频数据的接口
    QAudioDevice *m_audioDevice = nullptr; // 用于播放音频的设备
    QIODevice *m_audioIO = nullptr;        // 音频数据来源
    QAudioFormat m_format;                 // 播放时使用的音频格式
    SwrContext *m_swrContext = nullptr;    // 用于重采样的上下文

    // 缓冲区
    uint8_t **m_swrBuffer = nullptr;
    int m_swrBufferSize = 0;

    // 原始音频参数
    int m_sampleRate = 0;                               // 采样率
    int m_nbChannels = 0;                               // 通道数
    AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE; // 采样格式

    bool m_initialized = false; // 是否已经初始化

private:
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

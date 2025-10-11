#include "audioplayer.h"
#include "clock/globalclock.h"
#include <QDebug>
#include <QMediaDevices>

namespace {
    inline bool maskHasAll(uint64_t mask, uint64_t bits) {
        return (mask & bits) == bits;
    }

    struct ChannelMapping {
        uint64_t ffmpegMask;
        QAudioFormat::ChannelConfig qtChannelConfig;
    };
    // 按优先级顺序排列（从高声道数到低声道数）
    static const ChannelMapping mappings[] = {
        // clang-format off
        {AV_CH_LAYOUT_7POINT1,      QAudioFormat::ChannelConfigSurround7Dot1}, // 8
        {AV_CH_LAYOUT_7POINT0,      QAudioFormat::ChannelConfigSurround7Dot0}, // 7
        {AV_CH_LAYOUT_5POINT1_BACK, QAudioFormat::ChannelConfigSurround5Dot1}, // 6
        {AV_CH_LAYOUT_5POINT0_BACK, QAudioFormat::ChannelConfigSurround5Dot0}, // 5
        {AV_CH_LAYOUT_3POINT1,      QAudioFormat::ChannelConfig3Dot1},         // 4
        {AV_CH_LAYOUT_SURROUND,     QAudioFormat::ChannelConfig3Dot0},         // 3
        {AV_CH_LAYOUT_2POINT1,      QAudioFormat::ChannelConfig2Dot1},         // 3
        {AV_CH_LAYOUT_STEREO,       QAudioFormat::ChannelConfigStereo},        // 2
        {AV_CH_LAYOUT_MONO,         QAudioFormat::ChannelConfigMono},          // 1
        // clang-format on
    };
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent} {}

AudioPlayer::~AudioPlayer() {
    uninit();
}

bool AudioPlayer::init(const AVCodecParameters *codecParams, sharedFrmQueue frmBuf) { // TODO ： initial_padding trailing_padding是否要做处理
    if (m_initialized) {
        uninit();
    }

    m_frmBuf = frmBuf;

    if (!codecParams || codecParams->ch_layout.nb_channels == 0) {
        qDebug() << "无效的编解码器参数或音频不含声道";
        return false;
    }

    // 检查音频设备是否可用
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        qDebug() << "没有可用的音频输出设备";
        return false;
    }

    // 同步音频数据
    m_oldPar.sampleFormat = m_swrPar.sampleFormat = static_cast<AVSampleFormat>(codecParams->format);
    m_oldPar.sampleRate = m_swrPar.sampleRate = codecParams->sample_rate;
    int ret = av_channel_layout_copy(&m_oldPar.ch_layout, &codecParams->ch_layout);
    if (ret != 0) {
        return false;
    }
    ret = av_channel_layout_copy(&m_swrPar.ch_layout, &codecParams->ch_layout);
    if (ret != 0) {
        return false;
    }

    // 选择系统默认音频设备
    m_audioDevice = new QAudioDevice(QMediaDevices::defaultAudioOutput());
    QAudioFormat preferredFormat = m_audioDevice->preferredFormat(); // 设备默认配置

    bool isResample = false; // 是否重采样

    // 调整采样率
    int minSampleRate = m_audioDevice->minimumSampleRate(); // 设备支持的最小采样率
    int maxSampleRate = m_audioDevice->maximumSampleRate(); // 设备支持的最大采样率
    if (m_oldPar.sampleRate < minSampleRate || m_oldPar.sampleRate > maxSampleRate) {
        isResample = true;
        m_format.setSampleRate(preferredFormat.sampleRate());
        m_swrPar.sampleRate = preferredFormat.sampleRate();
    } else {
        m_format.setSampleRate(m_oldPar.sampleRate);
    }

    // 调整采样格式
    // NOTE : 这儿planar格式直接重采样
    switch (m_oldPar.sampleFormat) { // clang-format off
    case AV_SAMPLE_FMT_NONE: qDebug() << "无效音频采样格式"; return false;
    case AV_SAMPLE_FMT_U8:   m_format.setSampleFormat(QAudioFormat::UInt8); break;
    case AV_SAMPLE_FMT_S16:  m_format.setSampleFormat(QAudioFormat::Int16); break;
    case AV_SAMPLE_FMT_S32:  m_format.setSampleFormat(QAudioFormat::Int32); break;
    case AV_SAMPLE_FMT_FLT:  m_format.setSampleFormat(QAudioFormat::Float); break;
    case AV_SAMPLE_FMT_DBL:  m_format.setSampleFormat(QAudioFormat::Float); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_U8P:  m_format.setSampleFormat(QAudioFormat::UInt8); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_U8;  break;
    case AV_SAMPLE_FMT_S16P: m_format.setSampleFormat(QAudioFormat::Int16); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S16; break;
    case AV_SAMPLE_FMT_S32P: m_format.setSampleFormat(QAudioFormat::Int32); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    case AV_SAMPLE_FMT_FLTP: m_format.setSampleFormat(QAudioFormat::Float); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_DBLP: m_format.setSampleFormat(QAudioFormat::Float); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_S64:  m_format.setSampleFormat(QAudioFormat::Int32); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    case AV_SAMPLE_FMT_S64P: m_format.setSampleFormat(QAudioFormat::Int32); isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    default:break;
    } // clang-format on
    auto supportedSampleFormats = m_audioDevice->supportedSampleFormats();
    if (!supportedSampleFormats.contains(m_format.sampleFormat())) { // 设备不支持此格式
        isResample = true;
        m_format.setSampleFormat(preferredFormat.sampleFormat());
        m_swrPar.sampleFormat = QtToFFmpegSampleFormat(preferredFormat.sampleFormat());
    }

    // 调整通道布局
    int minChannelCount = std::max(1, m_audioDevice->minimumChannelCount());                                    // 设备支持的最小通道数量
    int maxChannelCount = std::min(8, m_audioDevice->maximumChannelCount());                                    // 设备支持的最大通道数量
    if (m_oldPar.ch_layout.nb_channels < minChannelCount || m_oldPar.ch_layout.nb_channels > maxChannelCount) { // 数量不匹配
        isResample = true;
        int tmpChannelCount = m_oldPar.ch_layout.nb_channels < minChannelCount ? minChannelCount : maxChannelCount;
        m_format.setChannelConfig(channelCountToQtChannelConfig(tmpChannelCount)); // 根据通道数量重新配置通道布局

        av_channel_layout_uninit(&m_swrPar.ch_layout);
        av_channel_layout_from_mask(&m_swrPar.ch_layout, QtChannelConfigToFFmpegMask(m_format.channelConfig()));
    } else {
        bool maskIsEqual = true;
        m_format.setChannelConfig(FFmpegToQtChannelConfig(m_oldPar.ch_layout, &maskIsEqual));
        if (m_format.channelConfig() == QAudioFormat::ChannelConfigUnknown) { // 未知格式
            isResample = true;
            m_format.setChannelConfig(channelCountToQtChannelConfig(m_oldPar.ch_layout.nb_channels));

            av_channel_layout_uninit(&m_swrPar.ch_layout);
            av_channel_layout_from_mask(&m_swrPar.ch_layout, QtChannelConfigToFFmpegMask(m_format.channelConfig()));
        } else if (maskIsEqual == false) { // AV_CHANNEL_ORDER_NATIVE模式下匹配成功，但是原始数据声道数多于QT标准的声道数
            isResample = true;

            av_channel_layout_uninit(&m_swrPar.ch_layout);
            av_channel_layout_from_mask(&m_swrPar.ch_layout, QtChannelConfigToFFmpegMask(m_format.channelConfig())); // 多余部分做下采样
        }
    }

    // 配置重采样上下文
    if (isResample) {
        swr_alloc_set_opts2(&m_swrContext,
                            &m_swrPar.ch_layout,   // 输出声道布局
                            m_swrPar.sampleFormat, // 输出格式
                            m_swrPar.sampleRate,   // 输出采样率
                            &m_oldPar.ch_layout,   // 输入声道布局
                            m_oldPar.sampleFormat, // 输入格式
                            m_oldPar.sampleRate,   // 输入采样率
                            0, nullptr);

        if (!m_swrContext || swr_init(m_swrContext) < 0) {
            qDebug() << "采样上下文初始化失败";
            return false;
        }

        // 初始化缓冲区
        m_swrBuffer = static_cast<uint8_t **>(av_malloc(sizeof(uint8_t *)));
        m_swrBufferSize = 1024;
        *m_swrBuffer = static_cast<uint8_t *>(av_malloc(m_swrBufferSize));
    }

    m_forceRefresh = false;
    m_initialized = true;

    return true;
}

bool AudioPlayer::uninit() {
    stop();
    // 需要在工作线程中销毁
    //  if (m_audioSink) {
    //      m_audioSink->stop();
    //      delete m_audioSink;
    //      m_audioSink = nullptr;
    //  }
    Q_ASSERT(!m_audioSink);

    if (m_audioDevice) {
        delete m_audioDevice;
        m_audioDevice = nullptr;
    }

    m_format = QAudioFormat{};

    m_oldPar.reset();
    m_swrPar.reset();

    m_initialized = false;

    if (m_swrContext) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }

    if (m_swrBuffer) {
        if (*m_swrBuffer) {
            av_free(*m_swrBuffer);
        }
        av_free(m_swrBuffer);
        m_swrBuffer = nullptr;
    }

    m_swrBufferSize = 0;
    m_frmBuf.reset();

    return true;
}

void AudioPlayer::start() {
    if (m_thread && m_thread->isRunning()) {
        return; // 已经在运行
    }
    m_stop.store(false, std::memory_order_relaxed);
    m_paused.store(false, std::memory_order_relaxed);
    m_thread = QThread::create([this] {
        playerLoop();
    });
    m_thread->start();
}

void AudioPlayer::stop() {
    if (!m_thread) {
        return; // 已经退出
    }
    m_stop.store(true, std::memory_order_relaxed);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
}

void AudioPlayer::togglePaused() {
    if (m_stop.load(std::memory_order_relaxed)) {
        return;
    }
    bool paused = m_paused.load(std::memory_order_relaxed);
    m_paused.store(!paused, std::memory_order_release);
}

double AudioPlayer::volume() const {
    return m_volume;
}

void AudioPlayer::setVolume(double newVolume) {
    if (m_audioSink) {
        qreal linearVolume = QAudio::convertVolume(newVolume,
                                                   QAudio::LogarithmicVolumeScale,
                                                   QAudio::LinearVolumeScale);
        m_audioSink->setVolume(linearVolume);
    }
    m_volume = newVolume;
}

bool AudioPlayer::getFrm(AVFrmItem &item) {
    if (item.frm != nullptr) {
        return true;
    }

    if (!m_frmBuf->pop(item)) { // 空
        return false;
    } else if (item.serial != m_frmBuf->serial()) { // 非空 但是序号不同
        av_frame_free(&item.frm);
        m_forceRefresh = true;
        return false;
    }
    return true;
}

qint64 AudioPlayer::write(AVFrmItem *item) {
    if (!item || !item->frm || !m_initialized || !m_audioIO)
        return -1;

    AVFrame *frm = item->frm;
    int dataSize;
    uint8_t *dataPtr = nullptr;
    int nb_samples = frm->nb_samples;

    if (m_swrContext) {
        // 估算需要的输出样本数（带上延迟）
        int out_nb_samples = av_rescale_rnd(
            swr_get_delay(m_swrContext, m_oldPar.sampleRate) + frm->nb_samples,
            m_swrPar.sampleRate,
            m_oldPar.sampleRate,
            AV_ROUND_UP);

        // 确保输出缓冲区足够大（单位：字节）
        int out_channels = m_format.channelCount();                                                          // 通道数
        int out_bytes_per_sample = av_get_bytes_per_sample(QtToFFmpegSampleFormat(m_format.sampleFormat())); // 单个通道的单个采样大小
        int out_buf_size = out_nb_samples * out_channels * out_bytes_per_sample;

        if (out_buf_size > m_swrBufferSize) { // swrbuf大小不够，重新分配
            m_swrBuffer = (uint8_t **)av_realloc(m_swrBuffer, sizeof(uint8_t *));
            *m_swrBuffer = (uint8_t *)av_realloc(*m_swrBuffer, out_buf_size);
            m_swrBufferSize = out_buf_size;
        }

        // 执行重采样
        int nb_samples = swr_convert(
            m_swrContext,
            m_swrBuffer, out_nb_samples,                 // 输出
            (const uint8_t **)frm->data, frm->nb_samples // 输入
        );

        if (nb_samples < 0) {
            qDebug() << "重采样执行失败";
            return -1;
        }

        dataSize = nb_samples * out_channels * out_bytes_per_sample;
        dataPtr = *m_swrBuffer;
    } else { // 无需重采样
        dataSize = av_samples_get_buffer_size(nullptr, m_oldPar.ch_layout.nb_channels, frm->nb_samples, m_oldPar.sampleFormat, 1);
        assert(!av_sample_fmt_is_planar(m_oldPar.sampleFormat));
        assert(m_oldPar.sampleFormat == static_cast<AVSampleFormat>(frm->format));
        assert(dataSize == frm->linesize[0]);
        dataPtr = frm->data[0];
    }

    // 写入
    int writeCnt = 0;
    while (writeCnt < dataSize) {
        int bytesFree = m_audioSink->bytesFree();
        if (bytesFree <= 0) {
            QThread::msleep(1); // 等待缓冲区空出一点空间
            continue;
        }

        int written = m_audioIO->write(
            reinterpret_cast<const char *>(dataPtr + writeCnt),
            std::min(bytesFree, dataSize - writeCnt));

        if (written <= 0) {
            qWarning() << "音频写入失败";
            break;
        }

        writeCnt += written;
    }
    // 更新音频时钟
    double nextPts = item->pts + 1.0 * nb_samples / m_format.sampleRate(); // 当前帧全部播完的结束时间
    // 未播的时间
    double offsetPts = (m_audioSink->bufferSize() - m_audioSink->bytesFree()) / (double)m_format.bytesForDuration(1e6);

    GlobalClock::instance().setAudioClk(nextPts - offsetPts);
    GlobalClock::instance().syncExternalClk(ClockType::AUDIO);

    return writeCnt;
}

void AudioPlayer::playerLoop() {
    if (!m_initialized) {
        return;
    }

    // 创建音频输出
    m_audioSink = new QAudioSink(*m_audioDevice, m_format);
    m_audioSink->setBufferSize(m_format.bytesForFrames(1));
    m_audioIO = m_audioSink->start(); ///@note m_audioSink->start()启动非常慢
    m_audioSink->setVolume(m_volume);
    DeviceStatus::instance().setAudioInitialized(true);

    // 确保音视频设备都完成了基本初始化
    while (!DeviceStatus::instance().initialized() && !m_stop.load(std::memory_order_relaxed)) {
        QThread::msleep(5);
    }

    AVFrmItem frmItem;
    while (!m_stop.load(std::memory_order_relaxed)) {
        bool ok = getFrm(frmItem);
        if (!ok) {
            QThread::msleep(5);
            continue;
        }

        if (!m_forceRefresh && m_paused.load(std::memory_order_relaxed)) {
            if (m_audioSink->state() == QAudio::ActiveState || m_audioSink->state() == QAudio::IdleState) {
                m_audioSink->suspend();
            }
            QThread::msleep(5);
            continue;
        } else if (m_audioSink->state() == QAudio::SuspendedState) {
            m_audioSink->resume();
        }

        // 写如入设备
        write(&frmItem);
        av_frame_free(&frmItem.frm);

        if (m_forceRefresh && GlobalClock::instance().mainClockType() == ClockType::AUDIO) {
            emit seeked();
        }
        m_forceRefresh = false;
    }

    if (m_audioSink) { // 不能在其他线程销毁
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    DeviceStatus::instance().setAudioInitialized(false);
    return;
}

QAudioFormat::ChannelConfig AudioPlayer::FFmpegToQtChannelConfig(const AVChannelLayout &ch_layout, bool *maskIsEqual) {
    if (maskIsEqual) {
        *maskIsEqual = true;
    }
    switch (ch_layout.order) { // clang-format off
    case AV_CHANNEL_ORDER_UNSPEC:   return channelCountToQtChannelConfig(ch_layout.nb_channels);      // 仅指定声道数量，不提供声道顺序
    case AV_CHANNEL_ORDER_CUSTOM:   return channelCountToQtChannelConfig(ch_layout.nb_channels);      // 声道顺序自定义（TODO: 做个映射）
    case AV_CHANNEL_ORDER_NATIVE:   return FFmpegMaskToQtChannelConfig(ch_layout.u.mask, maskIsEqual);// 原生声道顺序，即 AVChannel 枚举顺序
    case AV_CHANNEL_ORDER_AMBISONIC:return QAudioFormat::ChannelConfigUnknown;                        // 不支持 Ambisonic
    default:                        return QAudioFormat::ChannelConfigUnknown;                        // 未知情况
    } // clang-format on
}

AVSampleFormat AudioPlayer::QtToFFmpegSampleFormat(const QAudioFormat::SampleFormat &sampleFormat) {
    switch (sampleFormat) { // clang-format off
    case QAudioFormat::Unknown: return AV_SAMPLE_FMT_NONE;
    case QAudioFormat::UInt8:   return AV_SAMPLE_FMT_U8;
    case QAudioFormat::Int16:   return AV_SAMPLE_FMT_S16;
    case QAudioFormat::Int32:   return AV_SAMPLE_FMT_S32;
    case QAudioFormat::Float:   return AV_SAMPLE_FMT_FLT;
    default:                    return AV_SAMPLE_FMT_S32;
    } // clang-format on
}

QAudioFormat::ChannelConfig AudioPlayer::channelCountToQtChannelConfig(int nbChannels) {
    // clang-format off
    switch (nbChannels) {
    case 0: return QAudioFormat::ChannelConfigUnknown;
    case 1: return QAudioFormat::ChannelConfigMono;
    case 2: return QAudioFormat::ChannelConfigStereo;
    //case 3: return QAudioFormat::ChannelConfig2Dot1; //2.1与3.0都是3通道布局，这儿选择3.0
    case 3: return QAudioFormat::ChannelConfig3Dot0;
    case 4: return QAudioFormat::ChannelConfig3Dot1;
    case 5: return QAudioFormat::ChannelConfigSurround5Dot0;
    case 6: return QAudioFormat::ChannelConfigSurround5Dot1;
    case 7: return QAudioFormat::ChannelConfigSurround7Dot0;
    case 8: return QAudioFormat::ChannelConfigSurround7Dot1;
    default: return QAudioFormat::ChannelConfigUnknown;
    }
    // clang-format on
}

QAudioFormat::ChannelConfig AudioPlayer::FFmpegMaskToQtChannelConfig(uint64_t ffmpegMask, bool *maskIsEqual) {
    for (const auto &m : mappings) {
        if (maskHasAll(ffmpegMask, m.ffmpegMask)) {
            if (maskIsEqual) {
                *maskIsEqual = ffmpegMask == m.ffmpegMask;
            }
            return m.qtChannelConfig;
        }
    }
    return QAudioFormat::ChannelConfigUnknown;
}

uint64_t AudioPlayer::QtChannelConfigToFFmpegMask(QAudioFormat::ChannelConfig qtConfig) {
    for (const auto &m : mappings) {
        if (m.qtChannelConfig == qtConfig) {
            return m.ffmpegMask;
        }
    }
    return 0; // 未知
}

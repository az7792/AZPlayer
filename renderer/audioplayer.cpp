// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audioplayer.h"
#include "3rd/miniaudio/miniaudio.h"
#include "clock/globalclock.h"
#include "stats/playbackstats.h"
#include <QDebug>

namespace {

    // 根据数量获取 miniaudio 与之对应的 FFmpeg 布局
    static void get_default_ffmpeg_layout(int nb_channels, AVChannelLayout *layout) {
        int count = (nb_channels > 8) ? 8 : nb_channels;
        // clang-format off
        switch (count) {
        case 1: *layout = AV_CHANNEL_LAYOUT_MONO;         break;
        case 2: *layout = AV_CHANNEL_LAYOUT_STEREO;       break;
        case 3: *layout = AV_CHANNEL_LAYOUT_SURROUND;     break;
        case 4: *layout = AV_CHANNEL_LAYOUT_4POINT0;      break;
        case 5: *layout = AV_CHANNEL_LAYOUT_5POINT0_BACK; break;
        case 6: *layout = AV_CHANNEL_LAYOUT_5POINT1;      break;
        case 7: *layout = AV_CHANNEL_LAYOUT_6POINT1;      break;
        case 8: *layout = AV_CHANNEL_LAYOUT_7POINT1;      break;
        default: *layout = AV_CHANNEL_LAYOUT_STEREO;      break;
        }
        // clang-format on
    }

    static ma_channel ffmpeg_channel_to_ma(enum AVChannel channel) {
        // clang-format off
        switch (channel) {
        // --- 基础声道 ---
        case AV_CHAN_FRONT_LEFT:            return MA_CHANNEL_FRONT_LEFT;
        case AV_CHAN_FRONT_RIGHT:           return MA_CHANNEL_FRONT_RIGHT;
        case AV_CHAN_FRONT_CENTER:          return MA_CHANNEL_FRONT_CENTER;
        case AV_CHAN_LOW_FREQUENCY:         return MA_CHANNEL_LFE;
        case AV_CHAN_BACK_LEFT:             return MA_CHANNEL_BACK_LEFT;
        case AV_CHAN_BACK_RIGHT:            return MA_CHANNEL_BACK_RIGHT;
        case AV_CHAN_FRONT_LEFT_OF_CENTER:  return MA_CHANNEL_FRONT_LEFT_CENTER;
        case AV_CHAN_FRONT_RIGHT_OF_CENTER: return MA_CHANNEL_FRONT_RIGHT_CENTER;
        case AV_CHAN_BACK_CENTER:           return MA_CHANNEL_BACK_CENTER;
        case AV_CHAN_SIDE_LEFT:             return MA_CHANNEL_SIDE_LEFT;
        case AV_CHAN_SIDE_RIGHT:            return MA_CHANNEL_SIDE_RIGHT;

        // --- 顶置声道 ---
        case AV_CHAN_TOP_CENTER:            return MA_CHANNEL_TOP_CENTER;
        case AV_CHAN_TOP_FRONT_LEFT:        return MA_CHANNEL_TOP_FRONT_LEFT;
        case AV_CHAN_TOP_FRONT_CENTER:      return MA_CHANNEL_TOP_FRONT_CENTER;
        case AV_CHAN_TOP_FRONT_RIGHT:       return MA_CHANNEL_TOP_FRONT_RIGHT;
        case AV_CHAN_TOP_BACK_LEFT:         return MA_CHANNEL_TOP_BACK_LEFT;
        case AV_CHAN_TOP_BACK_CENTER:       return MA_CHANNEL_TOP_BACK_CENTER;
        case AV_CHAN_TOP_BACK_RIGHT:        return MA_CHANNEL_TOP_BACK_RIGHT;

        default:
            return MA_CHANNEL_NONE;
        }
        // clang-format on
    }

    // 根据 oldPar 设置 deviceConfig 的通道参数，并设置 swrPar 的通道参数，返回是否重采样
    static bool initChannelConfig(const AudioPar &oldPar, ma_device_config &deviceConfig, ma_channel *channelMap, AudioPar &swrPar, int &initIsOk) {
        deviceConfig.playback.channels = oldPar.ch_layout.nb_channels;
        initIsOk = av_channel_layout_copy(&swrPar.ch_layout, &oldPar.ch_layout);
        if (initIsOk != 0) {
            return false;
        }

        // 声道顺序未知，直接用 miniaudio 的默认顺序
        if (oldPar.ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            return false;
        }

        if (oldPar.ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
            bool mappingFailed = false;

            for (int i = 0; i < oldPar.ch_layout.nb_channels; ++i) {
                AVChannel ffmpegChan = av_channel_layout_channel_from_index(&oldPar.ch_layout, i);
                ma_channel maChan = ffmpeg_channel_to_ma(ffmpegChan);

                if (maChan == MA_CHANNEL_NONE) {
                    mappingFailed = true;
                    break; // 只要有一个不支持，就重采样
                }
                channelMap[i] = maChan;
            }

            if (!mappingFailed) {
                deviceConfig.playback.pChannelMap = channelMap;
                return false;
            }

            // 重采样
            av_channel_layout_uninit(&swrPar.ch_layout);
            get_default_ffmpeg_layout(oldPar.ch_layout.nb_channels, &swrPar.ch_layout);

            for (int i = 0; i < swrPar.ch_layout.nb_channels; ++i) {
                AVChannel ffmpegChan = av_channel_layout_channel_from_index(&oldPar.ch_layout, i);
                channelMap[i] = ffmpeg_channel_to_ma(ffmpegChan);
                Q_ASSERT(channelMap[i] != MA_CHANNEL_NONE);
            }
            deviceConfig.playback.channels = swrPar.ch_layout.nb_channels;
            deviceConfig.playback.pChannelMap = channelMap;

            return true;
        }

        // HACK: 暂时不支持Ambisonic，先用声道数代替
        if (oldPar.ch_layout.order == AV_CHANNEL_ORDER_AMBISONIC) {
            return false;
        }

        // HACK: 暂时不支持自定义，先用声道数代替
        if (oldPar.ch_layout.order == AV_CHANNEL_ORDER_CUSTOM) {
            return false;
        }

        // 未知
        return false;
    }

    // 向上舍入到最近的2^n
    static uint64_t roundUpPow2(uint64_t v) {
        if (v <= 1)
            return 1;
        if (v > (1ull << 63))
            return 0; // 溢出
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        return v;
    }
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent} {
    m_audioDevice = new ma_device{};
}

AudioPlayer::~AudioPlayer() {
    uninit();
    Q_ASSERT(m_audioDevice != nullptr);
    delete m_audioDevice;
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

    // 设备配置
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    ma_channel channelMap[MA_MAX_CHANNELS];

    // 同步音频数据
    m_oldPar.reset(), m_swrPar.reset();
    m_oldPar.sampleFormat = m_swrPar.sampleFormat = static_cast<AVSampleFormat>(codecParams->format);
    m_oldPar.sampleRate = m_swrPar.sampleRate = codecParams->sample_rate;
    int ret = av_channel_layout_copy(&m_oldPar.ch_layout, &codecParams->ch_layout);
    if (ret != 0) {
        return false;
    }

    bool isResample = false; // 是否重采样

    // 调整采样率
    deviceConfig.sampleRate = m_oldPar.sampleRate;

    // 调整采样格式
    // NOTE : miniaudio只支持packet格式，这儿planar格式需要重采样
    switch (m_oldPar.sampleFormat) { // clang-format off
    case AV_SAMPLE_FMT_NONE: qDebug() << "无效音频采样格式"; return false;
    case AV_SAMPLE_FMT_U8:   deviceConfig.playback.format = ma_format_u8;  break;
    case AV_SAMPLE_FMT_S16:  deviceConfig.playback.format = ma_format_s16; break;
    case AV_SAMPLE_FMT_S32:  deviceConfig.playback.format = ma_format_s32; break;
    case AV_SAMPLE_FMT_FLT:  deviceConfig.playback.format = ma_format_f32; break;
    case AV_SAMPLE_FMT_DBL:  deviceConfig.playback.format = ma_format_f32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_U8P:  deviceConfig.playback.format = ma_format_u8;  isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_U8;  break;
    case AV_SAMPLE_FMT_S16P: deviceConfig.playback.format = ma_format_s16; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S16; break;
    case AV_SAMPLE_FMT_S32P: deviceConfig.playback.format = ma_format_s32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    case AV_SAMPLE_FMT_FLTP: deviceConfig.playback.format = ma_format_f32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_DBLP: deviceConfig.playback.format = ma_format_f32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_FLT; break;
    case AV_SAMPLE_FMT_S64:  deviceConfig.playback.format = ma_format_s32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    case AV_SAMPLE_FMT_S64P: deviceConfig.playback.format = ma_format_s32; isResample = true; m_swrPar.sampleFormat = AV_SAMPLE_FMT_S32; break;
    default:break;
    } // clang-format on

    // 调整通道布局
    int ChannelConfigInitIsOK;
    isResample |= initChannelConfig(m_oldPar, deviceConfig, channelMap, m_swrPar, ChannelConfigInitIsOK);
    if (ChannelConfigInitIsOK != 0) {
        return false;
    }

    Q_ASSERT(m_pcmBuffer == nullptr);
    m_pcmFrameSize = m_swrPar.ch_layout.nb_channels * av_get_bytes_per_sample(m_swrPar.sampleFormat);
    uint64_t pcmBufferSize = (uint64_t)m_swrPar.sampleRate * m_pcmFrameSize * 100 / 1000; // 100ms容量
    m_pcmBuffer = new SPSCBuffer(roundUpPow2(pcmBufferSize));
    qDebug() << "pcmBuffer capacity:" << m_pcmBuffer->capacity();

    deviceConfig.dataCallback = miniaudio_data_callback;
    deviceConfig.pUserData = this;

    if (ma_device_init(NULL, &deviceConfig, m_audioDevice) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return false;
    }
    setVolume(m_volume);

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

    m_frmItem = {};
    m_pcmDataSize = 0;
    m_pcmDataPtr = nullptr;
    m_pcmDataIndex = 0;

    m_forceRefresh = false;
    m_initialized = true;
    m_serial = 0;

    return true;
}

bool AudioPlayer::uninit() {
    stop();

    Q_ASSERT(m_audioDevice != nullptr);
    ma_device_uninit(m_audioDevice);

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

    if (m_pcmBuffer) {
        delete m_pcmBuffer;
        m_pcmBuffer = nullptr;
    }

    m_swrBufferSize = 0;
    m_frmBuf.reset();

    return true;
}

void AudioPlayer::start() {
    if (!m_initialized) {
        qDebug() << "AudioPlayer: 尝试启动未初始化的设备";
        return;
    }

    if (m_thread.joinable()) {
        return;
    }

    // 启动PCM线程
    m_stop.store(false, std::memory_order_relaxed);
    m_paused.store(false, std::memory_order_relaxed);
    m_thread = std::thread([this] { playerLoop(); });

    // 启动音频设备
    ma_device_state state = ma_device_get_state(m_audioDevice);
    Q_ASSERT(state != ma_device_state_uninitialized);
    if (state != ma_device_state_started && state != ma_device_state_starting) {
        if (ma_device_start(m_audioDevice) != MA_SUCCESS) {
            m_stop.store(true, std::memory_order_relaxed);
            qDebug() << "AudioPlayer: 无法启动 miniaudio 设备";
            return;
        }
    }

    DeviceStatus::instance().setAudioInitialized(true);
}

void AudioPlayer::stop() {
    if (!m_thread.joinable()) {
        return; // 已经退出
    }

    // 关闭设备
    m_stop.store(true, std::memory_order_relaxed);
    ma_device_uninit(m_audioDevice);

    // 关闭PCM线程
    m_thread.join(); // 阻塞直到 playerLoop 退出

    DeviceStatus::instance().setAudioInitialized(false);
}

void AudioPlayer::togglePaused() {
    if (m_stop.load(std::memory_order_relaxed)) {
        return;
    }
    bool paused = m_paused.load(std::memory_order_relaxed);
    m_paused.store(!paused, std::memory_order_release);
    if (paused) {
        ma_device_start(m_audioDevice);
    } else {
        ma_device_stop(m_audioDevice);
    }
}

double AudioPlayer::volume() const {
    return m_volume;
}

void AudioPlayer::setVolume(double newVolume) {
    ma_device_set_master_volume(m_audioDevice, newVolume);
    m_volume = newVolume;
}

bool AudioPlayer::getFrm(AVFrmItem &item) {
    Q_ASSERT(item.frm == nullptr);
    // 直到找到序号一致的帧或队列为空

    if (item.serial != m_frmBuf->serial())
        m_forceRefresh = true;

    while (m_frmBuf->pop(item)) {
        if (item.serial == m_frmBuf->serial()) {
            return true;
        }
        av_frame_free(&item.frm);
    }

    // 队列空
    return false;
}

void AudioPlayer::playerLoop() {
    if (!m_initialized) {
        return;
    }

    while (!m_stop.load(std::memory_order_relaxed)) {
        bool ok = updatePcmFromFrameQueue();
        if (!ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (m_serial != m_frmBuf->serial()) {
            m_serial = m_frmBuf->serial();
            m_pcmBuffer->requestClearOldData();
            if (m_swrContext) {
                swr_init(m_swrContext);
            }
            m_forceRefresh = true;
        }

        // 写入帧到PCMBuffer中
        writePCM();

        if (m_forceRefresh && GlobalClock::instance().mainClockType() == ClockType::AUDIO)
            emit seeked();

        if (m_forceRefresh || !DeviceStatus::instance().haveVideo())
            emit playedOneFrame();

        m_forceRefresh = false;
    }

    return;
}

void AudioPlayer::writePCM() {
    m_bufferedEndPts = m_frmItem.pts;
    double bytesPerSec = m_swrPar.sampleRate * m_pcmFrameSize;
    GlobalClock::instance().syncExternalClk(ClockType::AUDIO);

    while (m_pcmDataIndex < m_pcmDataSize) {
        uint64_t len = m_pcmDataSize - m_pcmDataIndex;
        uint64_t written = m_pcmBuffer->write(m_pcmDataPtr + m_pcmDataIndex, len);
        m_bufferedEndPts += written / bytesPerSec;
        m_pcmDataIndex += written;
        if (written != len) {
            if (m_stop.load(std::memory_order_relaxed))
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    PlaybackStats::instance().audioPTS = GlobalClock::instance().audioPts();
    return;
}

bool AudioPlayer::updatePcmFromFrameQueue() {
    av_frame_free(&m_frmItem.frm);
    m_pcmDataSize = 0;
    m_pcmDataPtr = nullptr;
    m_pcmDataIndex = 0;

    if (getFrm(m_frmItem) == false) {
        return false;
    }
    AVFrame *frm = m_frmItem.frm;

    if (m_swrContext) {
        int nb_samples;

        // 估算需要的输出样本数（带上延迟）
        int out_nb_samples = av_rescale_rnd(
            swr_get_delay(m_swrContext, m_oldPar.sampleRate) + frm->nb_samples,
            m_swrPar.sampleRate,
            m_oldPar.sampleRate,
            AV_ROUND_UP);

        // 确保输出缓冲区足够大（单位：字节）
        int out_channels = m_swrPar.ch_layout.nb_channels;                         // 通道数
        int out_bytes_per_sample = av_get_bytes_per_sample(m_swrPar.sampleFormat); // 单个通道的单个采样大小
        int out_buf_size = out_nb_samples * out_channels * out_bytes_per_sample;

        if (out_buf_size > m_swrBufferSize) { // swrbuf大小不够，重新分配
            *m_swrBuffer = (uint8_t *)av_realloc(*m_swrBuffer, out_buf_size);
            m_swrBufferSize = out_buf_size;
        }

        // 执行重采样
        nb_samples = swr_convert(
            m_swrContext,
            m_swrBuffer, out_nb_samples,                 // 输出
            (const uint8_t **)frm->data, frm->nb_samples // 输入
        );

        if (nb_samples < 0) {
            qDebug() << "重采样执行失败";
            return false;
        }

        m_pcmDataSize = nb_samples * out_channels * out_bytes_per_sample;
        m_pcmDataPtr = *m_swrBuffer;
    } else { // 无需重采样
        assert(!av_sample_fmt_is_planar(m_oldPar.sampleFormat));
        assert(m_oldPar.sampleFormat == static_cast<AVSampleFormat>(frm->format));
        m_pcmDataSize = frm->nb_samples * m_pcmFrameSize;
        m_pcmDataPtr = frm->data[0];
    }

    return true;
}

void AudioPlayer::miniaudio_data_callback(ma_device *pDevice, void *pOutput, const void * /*pInput*/, ma_uint32 frameCount) {
    AudioPlayer *audioPlayer = static_cast<AudioPlayer *>(pDevice->pUserData);
    SPSCBuffer *buffer = audioPlayer->m_pcmBuffer;
    double nextPtr = audioPlayer->m_bufferedEndPts;

    const uint32_t bytesPerSample = ma_get_bytes_per_sample(pDevice->playback.format);
    const uint32_t frameSize = pDevice->playback.channels * bytesPerSample;

    double bytesPerSec = frameSize * pDevice->sampleRate;
    double offsetPts = buffer->readAvailable() / bytesPerSec; // 未播的时间
    GlobalClock::instance().setAudioClk(nextPtr - offsetPts);

    uint8_t *pDst = static_cast<uint8_t *>(pOutput);
    int writeCnt = 0; // 已经写入的大小(字节)
    int bytesNeeded;  // 需要写入的字节数

    bytesNeeded = frameCount * frameSize;

    while (0 < bytesNeeded) {
        uint64_t len = buffer->read(pDst + writeCnt, bytesNeeded);
        bytesNeeded -= len;
        writeCnt += len;

        if (len == 0)
            return; // 无数据
    }
}

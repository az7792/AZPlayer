// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DEMUX_H
#define DEMUX_H
#include "compat/compat.h"
#include "types/ptrs.h"
#include <QObject>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
AZ_EXTERN_C_END

struct ChapterInfo {
    double pts;        // 秒
    std::string title; // 描述
};

class Demux : public QObject {
    Q_OBJECT
    static constexpr std::size_t kMediaIdxCount = to_index(MediaIdx::Count);

public:
    explicit Demux(QObject *parent = nullptr);
    ~Demux();

    // 初始化
    bool init(const std::string URL, bool isMainDemux = false);
    // 反初始化，恢复到未初始化之前的状态
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    // 基于秒进行seek
    void seekBySec(double ts, double rel);

    // 切换视频流
    bool switchVideoStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq);
    /**
     * 切换字幕流
     * 如果该字幕流是文本字幕则使用ASSRender初始化，后续请通过ASSRender::renderFrame获取字幕帧
     */
    bool switchSubtitleStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq, bool &isAssSub);
    // 切换音频流
    bool switchAudioStream(int streamIdx, weakPktQueue wpq, weakFrmQueue wfq);

    // 关闭流
    void closeStream(MediaType type);

    // 获取时长
    int getDuration();

    // 获取当前播放的流，没有返回nullptr
    AVStream *getStream(MediaType type);

    // 该媒体文件是否拥有某种类型的流
    bool haveStream(MediaType type);

    std::array<size_t, kMediaIdxCount> getStreamsCount() const;

    const std::array<std::vector<QString>, kMediaIdxCount> &streamInfo() { return m_stringInfo; }
    const std::vector<ChapterInfo> &chaptersInfo() { return m_chaptersInfo; }

    AVFormatContext *formatCtx();

    bool isEOF();
public slots:
signals:
    // 当前解复用器seek完成
    void seeked(double pts);

private:
    weakPktQueue m_audioPktBuf;
    weakPktQueue m_videoPktBuf;
    weakPktQueue m_subtitlePktBuf;
    weakFrmQueue m_audioFrmBuf;    // 只用与修改队列序号用
    weakFrmQueue m_videoFrmBuf;    // 只用与修改队列序号用
    weakFrmQueue m_subtitleFrmBuf; // 只用与修改队列序号用

    AVFormatContext *m_formatCtx = nullptr;
    std::string m_URL;                                               // 媒体URL
    std::vector<int> m_videoIdx, m_audioIdx, m_subtitleIdx;          // 各个流的ID
    std::atomic<int> m_usedVIdx{-1}, m_usedAIdx{-1}, m_usedSIdx{-1}; // 当前使用的流ID
    std::array<std::vector<QString>, kMediaIdxCount> m_stringInfo;   // 流描述
    std::vector<ChapterInfo> m_chaptersInfo;                         // 章节描述

    char errBuf[512];
    std::atomic<bool> m_stop{true};
    std::thread m_thread;
    bool m_initialized = false;
    std::mutex m_mutex; // 保护队列和流ID的更新

    std::atomic<bool> m_needSeek{false};
    double m_seekTs = 0.0;
    double m_seekRel = 0.0;
    bool m_isMainDemux = false;
    bool m_isEOF = false;

private:
    void seekAllPktQueue(); // 为所有pkyQueue增加序号

    void demuxLoop(); // 主循环

    void pushVideoPkt(AVPacket *pkt);
    void pushAudioPkt(AVPacket *pkt);
    void pushSubtitlePkt(AVPacket *pkt);
    void pushPkt(weakPktQueue wq, AVPacket *pkt);

    void fillStreamInfo();   // 填充流消息
    void fillChaptersInfo(); // 填充章节消息

    bool switchStream(MediaType type, int streamIdx, weakPktQueue wpq, weakFrmQueue wfq); // 切换流
};

#endif // DEMUX_H

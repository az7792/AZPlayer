// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include "decode/decodeaudio.h"
#include "decode/decodesubtitle.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/audioplayer.h"
#include "renderer/videoplayer.h"
#include "types/ptrs.h"
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <array>

class MediaController : public QObject {
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);

    bool paused() const;
    void setPaused(bool newPaused); // 仅用于修改数值产生信号

    double volume() const;

    bool muted() const;
    void setMuted(bool newMuted); // 仅用于修改数值产生信号

    int duration() const;
    void setDuration(int newDuration); // 仅用于修改数值产生信号

    bool opened() const;
    void setOpened(bool newOpened); // 仅用于修改数值产生信号

    Q_INVOKABLE int getCurrentTime() const;           // 获取当前播放时间（秒）
    Q_INVOKABLE QVariantList getSubtitleInfo() const; // 获取字幕流信息
    Q_INVOKABLE QVariantList getAudioInfo() const;    // 获取音频流信息
    Q_INVOKABLE QVariantList getChaptersInfo() const; // 获取章节信息
    Q_INVOKABLE int getSubtitleIdx() const;           // 获取当前使用的流在所有同类流中的索引，-1为未使用
    Q_INVOKABLE int getAudioIdx() const;              // 获取当前使用的流在所有同类流中的索引，-1为未使用

    bool loopOnEnd() const;
    Q_INVOKABLE void setLoopOnEnd(bool newLoopOnEnd);

    int progress() const;
    void setProgress(int newProgress); // 仅用于修改数值产生信号

public slots:
    bool setVideoWindow(QObject *videoWindow); // 设置用于显示画面的QML元素

    bool open(const QUrl &URL);               // 打开文件开始播放
    bool openSubtitleStream(const QUrl &URL); // 打开字幕并解析流
    bool openAudioStream(const QUrl &URL);    // 打开音频并解析流
    bool close();                             // 关闭当前播放的文件

    void togglePaused();              // 切换是否暂停
    void toggleMuted();               // 切换是否静音
    void setVolume(double newVolume); // 设置音量
    void addVolum();                  // 增加0.04音量
    void subVolum();                  // 减少0.04音量

    void seekBySec(double ts, double rel); // seek到指定位置(秒)
    void fastForward();                    // 快进
    void fastRewind();                     // 快退

    bool switchSubtitleStream(int demuxIdx, int streamIdx); // 切换字幕流
    bool switchAudioStream(int demuxIdx, int streamIdx);    // 切换音频流

signals:
    void pausedChanged();      // 开始/暂停
    void volumeChanged();      // 音量更新
    void mutedChanged();       // 静音/不静音
    void streamInfoUpdate();   // 流信息已更新
    void chaptersInfoUpdate(); // 章节信息已更新

    void durationChanged(); // 播放时长改变
    void seeked();          // seek完成

    void openedChanged(); // 打开/关闭文件

    void played(); // 播放完毕

    void progressChanged(); // 播放进度更新

private:
    QUrl m_URL{}; // 主复用器打开的文件

    // 音视频队列
    sharedPktQueue m_pktAudioBuf = std::make_shared<AVPktQueue>(1);               // max(1MB,16packets)
    sharedFrmQueue m_frmAudioBuf = std::make_shared<SPSCQueue<AVFrmItem>>(9);     // max(9frames)
    sharedPktQueue m_pktVideoBuf = std::make_shared<AVPktQueue>(3);               // max(3MB,16packets)
    sharedFrmQueue m_frmVideoBuf = std::make_shared<SPSCQueue<AVFrmItem>>(3);     // max(3frames)
    sharedPktQueue m_pktSubtitleBuf = std::make_shared<AVPktQueue>(1);            // max(1MB,16packets)
    sharedFrmQueue m_frmSubtitleBuf = std::make_shared<SPSCQueue<AVFrmItem>>(16); // max(16frames)

    // 解复用器
    std::array<Demux *, to_index(MediaIdx::Count)> m_demuxs{nullptr, nullptr, nullptr}; // 0文件 1字幕 2音轨
    // 当前的视频流/字幕流/音频流所使用的{demuxIdx,streamIdx}，streamIdx是指demux中同类型的流中的顺序，不是demux全局的
    std::array<std::pair<int, int>, to_index(MediaIdx::Count)> m_streams;
    // 音视频解码器
    DecodeAudio *m_decodeAudio = nullptr;
    DecodeVideo *m_decodeVideo = nullptr;
    DecodeSubtitle *m_decodeSubtitl = nullptr;
    // 音视频播放控制器
    AudioPlayer *m_audioPlayer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;

    QTimer m_checkPlayerFinishedTimer;
    QTimer m_updatePktAndFrmQueueSizeTimer;

    bool m_opened = false;   // 是否打开文件
    bool m_paused = true;    // 是否暂停
    bool m_muted = false;    // 是否静音
    double m_volume = 1.0;   // 表现音量，非静音状态下才等于实际音量
    int m_duration = 0;      // 总时长（秒）
    int m_progress = 0;      // 播放进度（秒）
    bool m_loopOnEnd = true; // true播完重播 | false播完暂停
    bool m_played = false;   // 是否播完
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged FINAL)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged FINAL)
    Q_PROPERTY(bool opened READ opened WRITE setOpened NOTIFY openedChanged FINAL)
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged FINAL)

private:
    QVariantList getStreamInfo(MediaIdx type) const;
    bool openStreamByFile(const QUrl &URL, MediaIdx idx);
    bool seekAudioAndSubtitleDemux(double pts);
    void checkPlayerFinished();

signals:
    void clearVideoFBOSubtitleTex();
};

#endif // MEDIACONTROLLER_H

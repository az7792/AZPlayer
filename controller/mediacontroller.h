#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include "decode/decodeaudio.h"
#include "decode/decodesubtitle.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/audioplayer.h"
#include "renderer/videoplayer.h"
#include "utils.h"
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <array>

class MediaController : public QObject {
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);

    bool paused() const;
    void setPaused(bool newPaused);

    double volume() const;

    bool muted() const;
    void setMuted(bool newMuted);

    int duration() const;
    void setDuration(int newDuration);

    Q_INVOKABLE int getCurrentTime() const;
public slots:
    // 设置用于显示画面的QML元素
    bool setVideoWindow(QObject *videoWindow);

    bool open(const QUrl &URL);
    bool close();

    void togglePaused();
    void toggleMuted();
    void setVolume(double newVolume);

    void seekBySec(double ts, double rel); // seek到指定位置(秒)
    void fastForward();                    // 快进
    void fastRewind();                     // 快退

    bool switchSubtitleStream(int demuxIdx, int streamIdx);
    bool switchAudioStream(int demuxIdx, int streamIdx);

signals:
    void pausedChanged();
    void volumeChanged();
    void mutedChanged();

    void durationChanged();
    void seeked(); // 通知前端seek完成，主要用于防止进度条鬼畜

private:
    QUrl m_URL{};

    // 音视频队列
    sharedPktQueue m_pktAudioBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue m_frmAudioBuf = std::make_shared<SPSCQueue<AVFrmItem>>(8);
    sharedPktQueue m_pktVideoBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue m_frmVideoBuf = std::make_shared<SPSCQueue<AVFrmItem>>(8);
    sharedPktQueue m_pktSubtitleBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue m_frmSubtitleBuf = std::make_shared<SPSCQueue<AVFrmItem>>(8);

    // 解复用器
    std::array<Demux *, 3> m_demuxs{nullptr, nullptr, nullptr};                          // 0文件 1字幕 2音轨
    std::array<std::array<std::vector<std::pair<int, std::string>>, 3>, 3> m_streamInfo; //[demuxIdx][streamIdx] 0视频流 1字幕流 2音频流
    std::array<std::pair<int, int>, 3> m_streams;                                        // 当前的视频流/字幕流/音频流所使用的{demuxIdx,streamIdx}
    // 音视频解码器
    DecodeAudio *m_decodeAudio = nullptr;
    DecodeVideo *m_decodeVideo = nullptr;
    DecodeSubtitle *m_decodeSubtitl = nullptr;
    // 音视频播放控制器
    AudioPlayer *m_audioPlayer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;

    bool m_opened = false;

    bool m_paused = true;
    bool m_muted = false;
    double m_volume = 1.0; // 表现音量，非静音状态下才等于实际音量
    int m_duration = 0;
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged FINAL)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged FINAL)
};

#endif // MEDIACONTROLLER_H

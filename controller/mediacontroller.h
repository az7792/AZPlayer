#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include "decode/decodeaudio.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/audioplayer.h"
#include "renderer/videoplayer.h"
#include "utils.h"
#include <QObject>
#include <QUrl>

class MediaController : public QObject {
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);

    bool paused() const;
    void setPaused(bool newPaused);

public slots:
    // 设置用于显示画面的QML元素
    bool setVideoWindow(QObject *videoWindow);

    bool open(const QUrl &URL);
    bool close();

    void togglePaused();

signals:
    void pausedChanged();

private:
    QUrl m_URL{};

    // 音视频队列
    sharedPktQueue m_pktAudioBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue m_frmAudioBuf = std::make_shared<SPSCQueue<AVFrmItem>>(8);
    sharedPktQueue m_pktVideoBuf = std::make_shared<SPSCQueue<AVPktItem>>(16);
    sharedFrmQueue m_frmVideoBuf = std::make_shared<SPSCQueue<AVFrmItem>>(8);

    // 解复用器
    Demux *m_demux = nullptr;
    // 音视频解码器
    DecodeAudio *m_decodeAudio = nullptr;
    DecodeVideo *m_decodeVideo = nullptr;
    // 音视频播放控制器
    AudioPlayer *m_audioPlayer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;

    bool m_opened = false;
    bool m_paused = true;
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged FINAL)
};

#endif // MEDIACONTROLLER_H

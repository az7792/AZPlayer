#include "mediacontroller.h"
#include "renderer/videorenderer.h"
#include <QFileInfo>
#include <QTimer>

namespace {
    void clearPktQ(sharedPktQueue pktq) {
        if (!pktq) {
            return;
        }
        AVPktItem tmp;
        while (pktq->pop(tmp)) {
            av_packet_free(&tmp.pkt);
        }
    }
    void clearFrmQ(sharedFrmQueue frmq) {
        if (!frmq) {
            return;
        }
        AVFrmItem tmp;
        while (frmq->pop(tmp)) {
            av_frame_free(&tmp.frm);
        }
    }
}

MediaController::MediaController(QObject *parent)
    : QObject{parent} {

    m_demux = new Demux(parent);
    m_decodeAudio = new DecodeAudio(parent);
    m_decodeVideo = new DecodeVideo(parent);

    m_audioPlayer = new AudioPlayer(parent);
    m_videoPlayer = new VideoPlayer(parent);

    // DEBUG:start
    static QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        qDebug() << "AudioPkt:" << m_pktAudioBuf->size() << "VideoPkt:" << m_pktVideoBuf->size();
        qDebug() << "AudioFrm:" << m_frmAudioBuf->size() << "VideoFrm:" << m_frmVideoBuf->size() << "\n";
    });
    timer.start(1000); // 每1000ms触发一次
    // DEBUG: end
}

bool MediaController::setVideoWindow(QObject *videoWindow) {
    VideoWindow *wd = static_cast<VideoWindow *>(videoWindow);
    QObject::connect(m_videoPlayer, &VideoPlayer::renderDataReady,
                     wd, &VideoWindow::updateRenderData,
                     Qt::QueuedConnection);
    return true;
}

bool MediaController::open(const QUrl &URL) {
    QString localFile = URL.toLocalFile();
    if (!QFileInfo::exists(localFile)) {
        qDebug() << "无效路径:" << localFile;
        return false;
    }

    if (m_opened) {
        close();
    }

    bool ok = true;
    ok &= m_demux->init(localFile.toUtf8().constData(), m_pktAudioBuf, m_pktVideoBuf, nullptr);
    if (!ok) {
        close();
        return false;
    }

    ok &= m_decodeAudio->init(m_demux->getAudioStream(), m_pktAudioBuf, m_frmAudioBuf);
    ok &= m_decodeVideo->init(m_demux->getVideoStream(), m_pktVideoBuf, m_frmVideoBuf);
    ok &= m_audioPlayer->init(m_demux->getAudioStream()->codecpar, m_frmAudioBuf);
    ok &= m_videoPlayer->init(m_frmVideoBuf);
    if (!ok) {
        close();
        return false;
    }

    m_demux->start();
    m_decodeAudio->start();
    m_decodeVideo->start();
    m_audioPlayer->start();
    m_videoPlayer->start();

    m_opened = true;
    return true;
}

bool MediaController::close() {
    if (!m_opened) {
        return true;
    }
    bool ok = true;
    ok &= m_demux->uninit();
    ok &= m_decodeAudio->uninit();
    ok &= m_decodeVideo->uninit();
    ok &= m_audioPlayer->uninit();
    ok &= m_videoPlayer->uninit();
    clearPktQ(m_pktAudioBuf);
    clearPktQ(m_pktVideoBuf);
    clearFrmQ(m_frmAudioBuf);
    clearFrmQ(m_frmVideoBuf);
    m_opened = false;
    return ok;
}

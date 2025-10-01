#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include "renderdata.h"
#include "utils.h"
#include <QObject>
#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

class VideoPlayer : public QObject {
    Q_OBJECT
public:
    explicit VideoPlayer(sharedFrmQueue frmBuf, QObject *parent = nullptr);
    ~VideoPlayer();

public:
    sharedFrmQueue m_frmBuf;
public slots:
    void startPlay();

signals:
    void finished(); // 视频播放线程退出信号
    void renderDataReady(RenderData *data);

private:
    // 双缓冲
    RenderData renderData1;
    RenderData renderData2;
    RenderData *renderData{nullptr}; // 用于给渲染线程发数据
    double renderTime;               // 实际渲染指令被发出的时间(相对现实时间，秒)

    std::atomic<bool> m_stop{true};
    /**
     * 写入一帧数据
     * @warning 方法会阻塞线程
     */
    bool write(AVFrmItem *item);
};

#endif // VIDEOPLAYER_H

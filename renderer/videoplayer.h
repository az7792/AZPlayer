#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include "renderdata.h"
#include "utils.h"
#include <QObject>
#include <atomic>
#include <thread>

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
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool init(sharedFrmQueue frmBuf);
    bool uninit();

    // 开启解复用线程
    void start();
    // 退出解复用线程
    void stop();

    void togglePaused();

public:
    sharedFrmQueue m_frmBuf;

signals:
    void renderDataReady(RenderData *data);

private:
    // 双缓冲
    RenderData renderData1;
    RenderData renderData2;
    RenderData *renderData{nullptr}; // 用于给渲染线程发数据
    double renderTime;               // 实际渲染指令被发出的时间(相对现实时间，秒)

    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_paused{false};
    std::thread m_thread;
    bool m_initialized = false; // 是否已经初始化
    /**
     * 写入一帧数据
     * @warning 方法会阻塞线程
     */
    bool write(const AVFrmItem &item);

    void playerLoop();
};

#endif // VIDEOPLAYER_H

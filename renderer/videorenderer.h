#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

#include "renderdata.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QQuickFramebufferObject>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/frame.h>
#ifdef __cplusplus
}
#endif

class VideoRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions_3_3_Core {
public:
    VideoRenderer();
    ~VideoRenderer();

    void init(RenderData *renderData);

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;

    void synchronize(QQuickFramebufferObject *item) override;

private:
    QOpenGLShaderProgram m_program;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    int m_width = 0;     // frm的宽
    int m_height = 0;    // frm的高
    int m_widthFBO = 0;  // FBO的宽
    int m_heightFBO = 0; // FBO的高
    AVPixelFormat m_AVPixelFormat = AV_PIX_FMT_NONE;
    /**
     * Y | R | RGB | RGBA
     * U | G
     * V | B
     * A
     * @warning 与RenderData的排列顺序不同，单分量A永远在3号栏位
     */
    GLuint m_texArr[4]{0, 0, 0, 0};
    std::array<unsigned int, 3> GLParaArr[4]{};
    QSize componentSizeArr[4]{};
    uint8_t *dataArr[4]{};
    int linesizeArr[4]{};
    /**
     * 是否需要重新初始化纹理
     * @warning 请确保请求初始化前旧纹理已清除
     */
    bool m_needInitTex = true;

    RenderData *m_renderData = nullptr; // 渲染需要的数据

    bool updateTex(RenderData::PixFormat fmt);

    void initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para);
};

// QML控件
class VideoWindow : public QQuickFramebufferObject {
    Q_OBJECT
public:
    Renderer *createRenderer() const override;
public slots:
    void updateRenderData(RenderData *renderData);

private:
    RenderData *m_renderData = nullptr;
    friend VideoRenderer;
};
#endif // VIDEORENDERER_H

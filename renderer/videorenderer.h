// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

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

    // 初始化视频纹理
    void initVideoTex(RenderData *renderData);
    // 初始化字幕纹理
    void initSubtitleTex(SubRenderData *subRenderData);

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
    int m_widthSub = 0;  // 字幕的宽
    int m_heightSub = 0; // 字幕的高
    AVPixelFormat m_AVPixelFormat = AV_PIX_FMT_NONE;
    /**
     * Y | R | RGB | RGBA
     * U | G
     * V | B
     * A
     * @warning 与RenderData的排列顺序不同，单分量A永远在3号栏位
     */
    GLuint m_texArr[4]{0, 0, 0, 0};
    GLuint m_subTex = 0;
    std::array<unsigned int, 3> GLParaArr[4]{};
    QSize componentSizeArr[4]{};
    uint8_t *dataArr[4]{};
    int linesizeArr[4]{};
    int alignment = 1;

    float m_tx{0.f}, m_ty{0.f}; // 移动(OpenGL的裁剪空间-1.0到1.0为可视范围)
    float m_angle{0.f};         // 顺时针旋转(角度)
    float m_scale{1.f};         // 缩放(1为不缩放)

    /**
     * 是否需要重新初始化纹理
     * @warning 请确保请求初始化前旧纹理已清除
     */
    bool m_needInitVideoTex = true;
    bool m_needInitSubtitleTex = true;

    RenderData *m_renderData = nullptr; // 渲染需要的数据
    SubRenderData *m_subRenderData = nullptr;

    bool updateTex(RenderData::PixFormat fmt);
    bool updateSubTex();

    void initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para, uint8_t *fill = nullptr);
};

// QML控件
class VideoWindow : public QQuickFramebufferObject {
    Q_OBJECT
public:
    Renderer *createRenderer() const override;
    Q_INVOKABLE void setXY(float newX, float newY) {
        float oldTx = m_tx, oldTy = m_ty; // 值的实际更新必须在发射信号前完成
        m_tx = newX;
        m_ty = newY;
        update();
        if (std::fabs(newX - oldTx) > 0.001f) { // 只在变化较大时发射信号
            emit videoXChanged();
        }
        if (std::fabs(newY - oldTy) > 0.001f) { // 只在变化较大时发射信号
            emit videoYChanged();
        }
    }

    Q_INVOKABLE void addXY(float deltaX, float deltaY) {
        setXY(m_tx + deltaX, m_ty + deltaY);
    }

    // 之后的X,Y,angle,scale的add，sub主要服务于滚轮调节
    // X
    Q_INVOKABLE void setX(float newX) {
        float oldTx = m_tx;
        m_tx = newX; // 值的实际更新必须在发射信号前完成
        update();
        if (std::fabs(newX - oldTx) > 0.001f) { // 只在变化较大时发射信号
            emit videoXChanged();
        }
    }
    Q_INVOKABLE void addX() { setX(m_tx + 1); }
    Q_INVOKABLE void subX() { setX(m_tx - 1); }

    // Y
    Q_INVOKABLE void setY(float newY) {
        float oldTy = m_ty;
        m_ty = newY; // 值的实际更新必须在发射信号前完成
        update();
        if (std::fabs(newY - oldTy) > 0.001f) { // 只在变化较大时发射信号
            emit videoYChanged();
        }
    }
    Q_INVOKABLE void addY() { setY(m_ty + 1); }
    Q_INVOKABLE void subY() { setY(m_ty - 1); }

    // 缩放
    Q_INVOKABLE void setScale(int newScale) {
        int tmpScale = std::clamp(newScale, 2, 600);
        if (tmpScale != m_scale) {
            m_scale = tmpScale;
            update();
            emit videoScaleChanged();
        }
    }
    Q_INVOKABLE void addScale() { setScale(m_scale + 2); }
    Q_INVOKABLE void subScale() { setScale(m_scale - 2); }

    // 角度
    Q_INVOKABLE void setAngle(float angle) {
        float tmpAngle = std::fmod(angle, 360.0f);
        if (tmpAngle < 0)
            tmpAngle += 360.0f;
        if (std::fabs(tmpAngle - m_angle) > 0.05) {
            m_angle = tmpAngle;
            update();
            emit videoAngleChanged();
        }
    }
    Q_INVOKABLE void addAngle() { setAngle(m_angle + 1.f); }
    Q_INVOKABLE void subAngle() { setAngle(m_angle - 1.f); }

    // Getter
    Q_INVOKABLE int scale() const { return m_scale; }
    Q_INVOKABLE float angle() const { return m_angle; }
    Q_INVOKABLE float tx() const { return m_tx; }
    Q_INVOKABLE float ty() const { return m_ty; }

public slots:
    void updateRenderData(RenderData *renderData, SubRenderData *subRenderData);

signals:
    void videoAngleChanged();
    void videoScaleChanged();
    void videoXChanged();
    void videoYChanged();

private:
    RenderData *m_renderData = nullptr;
    SubRenderData *m_subRenderData = nullptr;
    float m_tx{0.f}, m_ty{0.f}; // 移动(像素)
    float m_angle{0.f};         // 顺时针旋转(角度)
    int m_scale{100};           // 缩放(100为不缩放)
    friend VideoRenderer;
};
#endif // VIDEORENDERER_H

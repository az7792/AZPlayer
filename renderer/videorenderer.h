// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

#include "compat/compat.h"
#include "renderdata.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QQuickFramebufferObject>
#include <QSize>

AZ_EXTERN_C_BEGIN
#include <libavutil/frame.h>
AZ_EXTERN_C_END

class VideoRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions_3_3_Core {
public:
    VideoRenderer();
    ~VideoRenderer();

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;
    void synchronize(QQuickFramebufferObject *item) override;

private:
    QOpenGLShaderProgram m_program;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    QSize m_frameSize{};    // frm的宽高
    QSize m_FBOSize{};      // FBO的宽高
    QSize m_subtitleSize{}; // 字幕的宽高

    AVPixelFormat m_AVPixelFormat = AV_PIX_FMT_NONE;
    /**
     * Y | R | RGB | RGBA
     * U | G
     * V | B
     * A
     * @warning 与RenderData的排列顺序不同，单分量A永远在3号栏位
     */
    GLuint m_texArr[4]{0, 0, 0, 0}; // 视频纹理
    GLuint m_subTex = 0;            // 字幕纹理，固定RGBA格式
    std::array<unsigned int, 3> GLParaArr[4]{};
    QSize componentSizeArr[4]{};
    const uint8_t *dataArr[4]{};
    int linesizeArr[4]{};
    int alignment = 1;

    float m_tx{0.f}, m_ty{0.f}; // 移动(OpenGL的裁剪空间-1.0到1.0为可视范围)
    float m_angle{0.f};         // 顺时针旋转(角度)
    float m_scaleX{1.f};        // 缩放(1为不缩放)
    float m_scaleY{1.f};        // 缩放(1为不缩放)

    /**
     * 是否需要重新初始化纹理
     * @warning 请确保请求初始化前旧纹理已清除
     */
    bool m_needInitVideoTex = true;
    bool m_needInitSubtitleTex = true;

    bool m_lastShowSubtitle = true; // 上一次是否显示字幕，主要是为了防止频繁更新 shader的showSub变量
    bool m_showSubtitle = true;     // 是否显示字幕

    VideoDoubleBuf *m_vidData = nullptr;    // 渲染需要的视频数据
    SubtitleDoubleBuf *m_subData = nullptr; // 渲染需要的字幕数据

private:
    // 初始化视频纹理
    void initVideoTex(VideoRenderData *renderData);
    // 初始化字幕纹理
    void initSubtitleTex(SubRenderData *subRenderData);

    bool updateTex(VideoRenderData::PixFormat fmt);
    // 字幕纹理固定 RGBA_PACKED 格式
    bool updateSubTex(SubRenderData &renData);

    /**
     * @brief 初始化或重建一个 2D OpenGL 纹理
     *
     * @param[in,out] tex
     *        OpenGL 纹理对象 ID。
     *        - 若为 0：创建新的纹理对象
     *        - 若不为 0：先删除旧纹理，再重新创建
     *
     * @param[in] size 纹理尺寸（宽高，单位：像素）。
     *
     * @param[in] para
     *        纹理像素格式参数数组：
     *        - para[0]：纹理内部格式（internalFormat，例如 GL_RGB、GL_RGBA、GL_R8）
     *        - para[1]：外部像素格式（format，例如 GL_RGB、GL_RGBA、GL_RED）
     *        - para[2]：像素数据类型（type，例如 GL_UNSIGNED_BYTE）
     *
     * @param[in] fill
     *        - 非 nullptr：立即将数据上传到 GPU
     *        - nullptr：仅分配纹理存储空间，不上传数据
     */
    void initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para, uint8_t *fill = nullptr);

    QMatrix4x4 getTransformMat(); // 获取当前的变换矩阵

    // 把纹理单元和纹理对象绑定
    void bindAllTexturesForDraw();
};

// QML控件
class VideoWindow : public QQuickFramebufferObject {
    Q_OBJECT
public:
    Renderer *createRenderer() const override;

    Q_INVOKABLE void setShowSubtitle(bool val) { m_showSubtitle = val; }

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

    // 镜像
    Q_INVOKABLE void setHorizontalMirror(bool val) {
        m_horizontalMirror = val;
        update();
    }
    Q_INVOKABLE void setVerticalMirror(bool val) {
        m_verticalMirror = val;
        update();
    }

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
    void updateRenderData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData);

signals:
    void videoAngleChanged();
    void videoScaleChanged();
    void videoXChanged();
    void videoYChanged();

private:
    VideoDoubleBuf *m_vidData = nullptr;
    SubtitleDoubleBuf *m_subData = nullptr;
    float m_tx{0.f}, m_ty{0.f}; // 移动(像素)
    float m_angle{0.f};         // 顺时针旋转(角度)
    int m_scale{100};           // 缩放(100为不缩放)
    bool m_showSubtitle = true; // 是否显示字幕
    bool m_horizontalMirror{false};
    bool m_verticalMirror{false};
    friend VideoRenderer;
};
#endif // VIDEORENDERER_H

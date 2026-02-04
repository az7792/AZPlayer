// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videorenderer.h"
#include "clock/globalclock.h"
#include "utils/utils.h"
#include <QOpenGLFramebufferObjectFormat>
namespace {
    // 为了避免 非 POD 静态对象 导致的初始化顺序问题
    std::vector<uint8_t> &texFill() {
        static std::vector<uint8_t> instance;
        return instance;
    }
    std::vector<QRect> &lastSubTexRect() {
        static std::vector<QRect> instance;
        return instance;
    }
}

VideoRenderer::VideoRenderer() {
    initializeOpenGLFunctions();
    const QString vSrcPath = ":/shaderSource/shader.vert";
    const QString fSrcPath = ":/shaderSource/shader.frag";
    if (!m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, vSrcPath) ||
        !m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, fSrcPath) ||
        !m_program.link()) {
        qDebug() << "着色器程序编译失败";
        return;
    }
    qDebug() << "着色器程序编译成功";
    float vertices[] = {
        // 位置      // 纹理
        -1.f, 1.f, 0.f, 1.f,  // 左上
        -1.f, -1.f, 0.f, 0.f, // 左下
        1.f, -1.f, 1.f, 0.f,  // 右下
        1.f, 1.f, 1.f, 1.f    // 右上
    };

    unsigned int indices[] = {
        0, 1, 2, // 第一个三角形
        0, 2, 3  // 第二个三角形
    };

    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // 绑定 VBO （顶点缓冲）
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 绑定 EBO（索引缓冲）
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置
    // layout(location = 0) in vec2 aPos; // 顶点坐标
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标
    // layout(location = 1) in vec2 aTexCoord; // 纹理坐标
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑
    glBindVertexArray(0);             // 解绑VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑VBO
    // 不要解绑EBO

    // 视频显示设备已准备就绪
    DeviceStatus::instance().setVideoInitialized(true);
}

VideoRenderer::~VideoRenderer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    if (m_subTex != 0) {
        glDeleteTextures(1, &m_subTex);
    }
    for (auto &v : m_texArr) {
        if (v != 0)
            glDeleteTextures(1, &v);
    }
}

void VideoRenderer::initVideoTex(VideoRenderData *renderData) {
    if (!renderData)
        return;

    AVFrame *frm = renderData->frmItem.frm;
    if (!m_needInitVideoTex || !frm)
        return;

    // 初始化像素格式与上传方式
    m_width = frm->width, m_height = frm->height;
    m_AVPixelFormat = (AVPixelFormat)frm->format; // FFmpeg 的像素格式

    m_program.bind();
    int loc = m_program.uniformLocation("pixFormat");
    m_program.setUniformValue(loc, (int)renderData->pixFormat);
    loc = m_program.uniformLocation("yTex");
    m_program.setUniformValue(loc, 0);
    loc = m_program.uniformLocation("uTex");
    m_program.setUniformValue(loc, 1);
    loc = m_program.uniformLocation("vTex");
    m_program.setUniformValue(loc, 2);
    loc = m_program.uniformLocation("aTex");
    m_program.setUniformValue(loc, 3);
    m_program.release();

    // 拷贝一些参数方便使用
    VideoRenderData::PixFormat tmpFmt = renderData->pixFormat;
    for (int i = 0; i < 4; ++i) {
        GLParaArr[i] = renderData->GLParaArr[i];
        linesizeArr[i] = renderData->linesizeArr[i];
        componentSizeArr[i] = renderData->componentSizeArr[i];
        alignment = renderData->alignment;
    }

    if (tmpFmt == VideoRenderData::RGB_PACKED || tmpFmt == VideoRenderData::RGBA_PACKED ||
        tmpFmt == VideoRenderData::Y || tmpFmt == VideoRenderData::YA) {
        initTex(m_texArr[0], componentSizeArr[0], GLParaArr[0]);
        if (tmpFmt == VideoRenderData::YA) {
            initTex(m_texArr[3], componentSizeArr[1], GLParaArr[1]); // A
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            initTex(m_texArr[i], componentSizeArr[i], GLParaArr[i]); // RGB | YUV
        }
        if (tmpFmt == VideoRenderData::RGBA_PLANAR || tmpFmt == VideoRenderData::YUVA) {
            initTex(m_texArr[3], componentSizeArr[3], GLParaArr[3]); // A
        }
    }
    m_needInitVideoTex = false;
}

void VideoRenderer::initSubtitleTex(SubRenderData *subRenderData) {
    if (m_needInitSubtitleTex && subRenderData) {
        m_program.bind();
        int loc = m_program.uniformLocation("subTex");
        m_program.setUniformValue(loc, 4);

        m_heightSub = subRenderData->frmItem.height;
        m_widthSub = subRenderData->frmItem.width;

        if (m_heightSub == 0 || m_widthSub == 0) {
            m_heightSub = m_height, m_widthSub = m_width;
        }

        // 字幕纹理
        if ((int)texFill().size() < m_widthSub * m_heightSub * 4) {
            texFill().assign(m_widthSub * m_heightSub * 4, 0);
        }
        initTex(m_subTex, {m_widthSub, m_heightSub}, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, texFill().data());

        loc = m_program.uniformLocation("haveSubTex"); // 主要用于标记有字幕纹理(即使当前播放的视频没有字幕)
        m_program.setUniformValue(loc, true);

        // 清空
        glActiveTexture(GL_TEXTURE0 + 4);
        glBindTexture(GL_TEXTURE_2D, m_subTex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_widthSub);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_widthSub, m_heightSub, GL_RGBA, GL_UNSIGNED_BYTE, texFill().data());

        m_needInitSubtitleTex = false;
        m_program.release();
    }
}

QOpenGLFramebufferObject *VideoRenderer::createFramebufferObject(const QSize &size) {
    m_widthFBO = size.width();
    m_heightFBO = size.height();

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4); // 抗锯齿4x MSAA
    // format.setAttachment(QOpenGLFramebufferObject::Depth);
    return new QOpenGLFramebufferObject(size, format);
}

void VideoRenderer::render() {
    if (!m_vidData) {
        glClearColor(0.0627f, 0.0627f, 0.0627f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    GLint prevAlign = 0;
    GLint prevRowLen = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prevRowLen);

    if (m_subData) {
        m_subData->read([&](SubRenderData &renData, int) -> bool {
            // 初始化字幕纹理
            if (renData.subtitleType != SUBTITLE_NONE && (renData.frmItem.width != m_widthSub || renData.frmItem.height != m_heightSub)) {
                m_needInitSubtitleTex = true;
                initSubtitleTex(&renData);
            }
            return updateSubTex(renData);
        });
    }

    static int ct0 = 0, ct1 = 0;
    [[maybe_unused]] bool ok = m_vidData->read([&](VideoRenderData &renData, int idx) -> bool {
        if (idx == 0)
            ct0++;
        else
            ct1++;
        // qDebug() << "video read: " << ct0 << ct1 << idx << GlobalClock::instance().getMainPts() * 1000;
        AVFrame *frm = renData.frmItem.frm;
        if (frm == nullptr)
            return false;

        // 更新视频纹理
        if (frm->width != m_width || frm->height != m_height || frm->format != m_AVPixelFormat) {
            m_needInitVideoTex = true;
            initVideoTex(&renData);
        }

        // 更新参数
        for (int i = 0; i < 4; ++i) {
            dataArr[i] = renData.dataArr[i];
        }
        // 上传视频纹理
        updateTex(renData.pixFormat);

        renData.renderedTime = getRelativeSeconds();
        return true;
    });

    // =======绘制==============

    // 灰底背景
    glClearColor(0.0627f, 0.0627f, 0.0627f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 绑定纹理单元和纹理对象
    bindAllTexturesForDraw();
    m_program.bind();

    if (m_showSubtitle != m_lastShowSubtitle) {
        m_lastShowSubtitle = m_showSubtitle;
        int loc = m_program.uniformLocation("showSub"); // 是否显示字幕
        m_program.setUniformValue(loc, m_showSubtitle);
    }

    m_program.setUniformValue(m_program.uniformLocation("transform"), getTransformMat());
    // 绘制视频画面
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, prevRowLen);

    m_program.release();
    // 绘制结束
}

void VideoRenderer::synchronize(QQuickFramebufferObject *item) {
    VideoWindow *videoWindow = static_cast<VideoWindow *>(item);
    m_vidData = videoWindow->m_vidData;
    m_subData = videoWindow->m_subData;
    m_tx = 2.f * videoWindow->m_tx / m_widthFBO;
    m_ty = 2.f * videoWindow->m_ty / m_heightFBO;

    m_angle = videoWindow->m_angle * (videoWindow->m_horizontalMirror ^ videoWindow->m_verticalMirror ? -1.f : 1.f);

    m_scaleX = m_scaleY = videoWindow->m_scale / 100.f;
    m_scaleX *= videoWindow->m_horizontalMirror ? -1.f : 1.f;
    m_scaleY *= videoWindow->m_verticalMirror ? -1.f : 1.f;

    m_showSubtitle = videoWindow->m_showSubtitle;
}

bool VideoRenderer::updateTex(VideoRenderData::PixFormat fmt) {
    if (fmt == VideoRenderData::NONE)
        return false;

    auto uploadTexture = [&](int pos, GLenum textureOffset) {
        glActiveTexture(GL_TEXTURE0 + textureOffset);
        glBindTexture(GL_TEXTURE_2D, m_texArr[textureOffset]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeArr[pos]); // 一行存储的像素个数 [用于显示的像素个数] + [用于填充的像素个数(这部分需要丢弃)]
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        componentSizeArr[pos].width(), componentSizeArr[pos].height(),
                        GLParaArr[pos][1], GLParaArr[pos][2],
                        dataArr[pos]);
    };

    if (fmt == VideoRenderData::RGB_PACKED || fmt == VideoRenderData::RGBA_PACKED ||
        fmt == VideoRenderData::Y || fmt == VideoRenderData::YA) {
        uploadTexture(0, 0); // RGB|RGBA|Y

        if (fmt == VideoRenderData::YA) {
            uploadTexture(1, 3); // A
        }
    } else {
        for (int pos = 0; pos < 3; ++pos) {
            uploadTexture(pos, pos); // R,G,B | Y,U,V
        }

        if (fmt == VideoRenderData::RGBA_PLANAR || fmt == VideoRenderData::YUVA) {
            uploadTexture(3, 3); // A
        }
    }
    return true;
}

bool VideoRenderer::updateSubTex(SubRenderData &renData) {
    if (renData.uploaded) {
        return true;
    }
    if (m_subTex == 0) {
        return true;
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, m_subTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // 清空字幕，和矩形列表
    auto clearSubTex = [&]() {
        if ((int)texFill().size() < m_widthSub * m_heightSub * 4) {
            texFill().assign(m_widthSub * m_heightSub * 4, 0);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_widthSub);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_widthSub, m_heightSub, GL_RGBA, GL_UNSIGNED_BYTE, texFill().data());
        lastSubTexRect().clear();
    };

    static AVSubtitleType lastSubType = SUBTITLE_NONE;

    // 清理旧数据
    if (renData.size == 0) {
        clearSubTex();
        renData.uploaded = true;
        return true;
    }

    if (renData.subtitleType != lastSubType) {
        clearSubTex();
        lastSubType = renData.subtitleType;
    }

    // 清理纹理上的旧字幕
    for (size_t i = 0; i < lastSubTexRect().size(); ++i) {
        const QRect &rect = lastSubTexRect()[i];
        int x = std::clamp(rect.x(), 0, (int)m_width);
        int y = std::clamp(rect.y(), 0, (int)m_height);
        int w = std::clamp(rect.width(), 0, (int)m_width - x);
        int h = std::clamp(rect.height(), 0, (int)m_height - y);

        if (w == 0 || h == 0)
            continue;

        if ((int)texFill().size() < h * w * 4) {
            texFill().assign(h * w * 4, 0);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, texFill().data());
    }

    // 保存当前字幕区域
    lastSubTexRect() = renData.rects;

    // 绘制新字幕
    for (size_t i = 0; i < renData.size; ++i) {
        int len = renData.linesizeArr[i];
        const QRect &rect = renData.rects[i];
        int x = rect.x();
        int y = rect.y();
        int h = rect.height();
        int w = rect.width();
        if (h == 0 || w == 0) {
            continue;
        }
        uint8_t *subtitleData = renData.dataArr[i].data();

        glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, subtitleData);
    }

    renData.uploaded = true;
    return true;
}

void VideoRenderer::initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para, uint8_t *fill) {
    if (tex != 0)
        glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // 为当前绑定的纹理对象设置环绕、过滤方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, size.width()); // 设置一行的像素个数
    glTexImage2D(GL_TEXTURE_2D, 0, para[0], size.width(), size.height(), 0, para[1], para[2], fill);
    glBindTexture(GL_TEXTURE_2D, 0);
}

QMatrix4x4 VideoRenderer::getTransformMat() {
    QMatrix4x4 mat;
    mat.setToIdentity();

    mat.translate(m_tx, m_ty, 0.0f); // 移动 #step5

    // 视频等比例缩放到[-1,1]的正方形里时视频右上角的坐标
    float scaleX0 = 1.0f, scaleY0 = 1.0f;
    if (m_width > m_height) {
        scaleY0 = 1.f * m_height / m_width;
    } else {
        scaleX0 = 1.f * m_width / m_height;
    }

    // 将视频等比例填充满FBO并将FBO缩放到[-1,1]时视频右上角的坐标
    float scaleX = 1.0f, scaleY = 1.0f;
    float videoAspect = 1.f * m_width / m_height, fboAspect = 1.f * m_widthFBO / m_heightFBO;
    if (videoAspect > fboAspect) {
        // 视频比FBO宽，宽撑满，高缩放
        scaleY = fboAspect / videoAspect;
    } else {
        // 视频比FBO高，高撑满，宽缩放
        scaleX = videoAspect / fboAspect;
    }
    mat.scale(scaleX / scaleX0, scaleY / scaleY0, 1.0f); // 调整画面比例，将step1的原始比例映射到新的比例 #step4

    mat.scale(m_scaleX, m_scaleY, 1.0f); // 缩放 #step3
    mat.rotate(m_angle, 0, 0, 1);        // 旋转 #step2

    // 按视频比例将顶点在[-1,1]的正方形里先保持比例正确 #step1
    mat.scale(scaleX0, scaleY0, 1.f);

    return mat;
}

void VideoRenderer::bindAllTexturesForDraw() {
    // 视频纹理
    for (int i = 0; i < 4; ++i) {
        // 这儿是本程序约定好的，具体查看：initVideoTex()
        glActiveTexture(GL_TEXTURE0 + i);

        if (m_texArr[i] != 0) {
            glBindTexture(GL_TEXTURE_2D, m_texArr[i]);
        } else {
            // 显式解绑，避免采样到 Qt / 其他 renderer 的残留
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // 字幕纹理（固定使用 unit 4）,具体查看：initSubtitleTex()
    glActiveTexture(GL_TEXTURE0 + 4);
    if (m_subTex != 0) {
        glBindTexture(GL_TEXTURE_2D, m_subTex);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE0);
}

//===========VideoWindow===========//
QQuickFramebufferObject::Renderer *VideoWindow::createRenderer() const {
    VideoRenderer *render = new VideoRenderer();
    return render;
}

void VideoWindow::updateRenderData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData) {
    m_vidData = vidData;
    m_subData = subData;
    update();
}

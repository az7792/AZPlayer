#include "videorenderer.h"
#include <QDateTime>
#include <QOpenGLFramebufferObjectFormat>

VideoRenderer::VideoRenderer() {
    initializeOpenGLFunctions();
}

VideoRenderer::~VideoRenderer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    for (auto &v : m_texArr) {
        if (v != 0)
            glDeleteTextures(1, &v);
    }
}

void VideoRenderer::init(RenderData *renderData) {
    if (!m_program.isLinked()) {
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
    }

    AVFrame *frm = renderData->frm;
    if (m_needInitTex && frm) {
        static int cct = 0;
        cct++;
        qDebug() << "cct:" << cct;
        // 初始化像素格式与上传方式
        m_width = frm->width, m_height = frm->height;
        m_AVPixelFormat = (AVPixelFormat)frm->format;

        m_program.bind();
        int loc = m_program.uniformLocation("pixFormat");
        m_program.setUniformValue(loc, (int)renderData->pixFormat);
        qDebug() << (int)renderData->pixFormat;
        loc = m_program.uniformLocation("yTex");
        m_program.setUniformValue(loc, 0);
        loc = m_program.uniformLocation("uTex");
        m_program.setUniformValue(loc, 1);
        loc = m_program.uniformLocation("vTex");
        m_program.setUniformValue(loc, 2);
        loc = m_program.uniformLocation("aTex");
        m_program.setUniformValue(loc, 3);

        // 拷贝一些参数方便使用
        RenderData::PixFormat tmpFmt = renderData->pixFormat;
        for (int i = 0; i < 4; ++i) {
            GLParaArr[i] = renderData->GLParaArr[i];
            dataArr[i] = renderData->dataArr[i];
            linesizeArr[i] = renderData->linesizeArr[i];
            componentSizeArr[i] = renderData->componentSizeArr[i];
        }

        if (tmpFmt == RenderData::RGB_PACKED || tmpFmt == RenderData::RGBA_PACKED ||
            tmpFmt == RenderData::Y || tmpFmt == RenderData::YA) {
            initTex(m_texArr[0], componentSizeArr[0], GLParaArr[0]);
            if (tmpFmt == RenderData::YA) {
                initTex(m_texArr[3], componentSizeArr[1], GLParaArr[1]); // A
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                initTex(m_texArr[i], componentSizeArr[i], GLParaArr[i]); // RGB | YUV
            }
            if (tmpFmt == RenderData::RGBA_PLANAR || tmpFmt == RenderData::YUVA) {
                initTex(m_texArr[3], componentSizeArr[3], GLParaArr[3]); // A
            }
        }
        m_needInitTex = false;
    }
}

QOpenGLFramebufferObject *VideoRenderer::createFramebufferObject(const QSize &size) {
    m_widthFBO = size.width();
    m_heightFBO = size.height();

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    return new QOpenGLFramebufferObject(size, format);
}

void VideoRenderer::render() {
    if (!m_renderData) {
        glClearColor(0.0392f, 0.0392f, 0.0392f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    m_renderData->mutex.lock();

    // // 更新纹理
    if (m_renderData->frm->width != m_width || m_renderData->frm->height != m_height || m_renderData->frm->format != m_AVPixelFormat) {
        m_needInitTex = true;
        init(m_renderData);
    }

    // 更新参数
    for (int i = 0; i < 4; ++i) {
        dataArr[i] = m_renderData->dataArr[i];
        linesizeArr[i] = m_renderData->linesizeArr[i];
    }
    // 上传纹理
    if (updateTex(m_renderData->pixFormat)) {
        // 纯色背景
        glClearColor(0.0392f, 0.0392f, 0.0392f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        m_program.bind();
        // 调整画面比例
        float scaleX = 1.0f, scaleY = 1.0f;
        float videoAspect = 1.f * m_width / m_height, fboAspect = 1.f * m_widthFBO / m_heightFBO;
        if (videoAspect > fboAspect) {
            // 视频比FBO宽，宽撑满，高缩放
            scaleY = fboAspect / videoAspect;
        } else {
            // 视频比FBO高，高撑满，宽缩放
            scaleX = videoAspect / fboAspect;
        }
        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.scale(scaleX, scaleY, 1.0f);
        m_program.setUniformValue(m_program.uniformLocation("transform"), mat);
        // 绘制视频画面
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        m_program.release();
    }

    // 绘制结束
    double pts = m_renderData->pts;
    m_renderData->renderedTime = getRelativeSeconds();
    m_renderData->mutex.unlock();

    // 更新视频时钟
    // qDebug() << "v:" << pts << GlobalClock::instance().videoPts();
    GlobalClock::instance().setVideoClk(pts);
    GlobalClock::instance().syncExternalClk(GlobalClock::instance().videoClk());
}

void VideoRenderer::synchronize(QQuickFramebufferObject *item) {
    VideoWindow *videoWindow = static_cast<VideoWindow *>(item);
    m_renderData = videoWindow->m_renderData;
}

bool VideoRenderer::updateTex(RenderData::PixFormat fmt) {
    if (fmt == RenderData::NONE)
        return false;

    auto uploadTexture = [&](int pos, GLenum textureOffset) {
        glActiveTexture(GL_TEXTURE0 + textureOffset);
        glBindTexture(GL_TEXTURE_2D, m_texArr[textureOffset]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);                 // 对齐为1字节 //TODO : 更具实际情况对齐
        glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeArr[pos]); // 一行存储的像素个数 [用于显示的像素个数] + [用于填充的像素个数(这部分需要丢弃)]
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        componentSizeArr[pos].width(), componentSizeArr[pos].height(),
                        GLParaArr[pos][1], GLParaArr[pos][2],
                        dataArr[pos]);
    };

    if (fmt == RenderData::RGB_PACKED || fmt == RenderData::RGBA_PACKED ||
        fmt == RenderData::Y || fmt == RenderData::YA) {
        uploadTexture(0, 0); // RGB|RGBA|Y

        if (fmt == RenderData::YA) {
            uploadTexture(1, 3); // A
        }
    } else {
        for (int pos = 0; pos < 3; ++pos) {
            uploadTexture(pos, pos); // R,G,B | Y,U,V
        }

        if (fmt == RenderData::RGBA_PLANAR || fmt == RenderData::YUVA) {
            uploadTexture(3, 3); // A
        }
    }
    return true;
}

void VideoRenderer::initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para) {
    if (tex != 0)
        glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // 为当前绑定的纹理对象设置环绕、过滤方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, para[0], size.width(), size.height(), 0, para[1], para[2], NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

//===========VideoWindow===========//
QQuickFramebufferObject::Renderer *VideoWindow::createRenderer() const {
    VideoRenderer *render = new VideoRenderer();
    return render;
}

void VideoWindow::updateRenderData(RenderData *renderData) {
    m_renderData = renderData;
    update();
}

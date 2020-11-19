/****************************************************************************
**
** Copyright (C) 2023- Paolo Angelelli <paolo.angelelli@gmail.com>
**
** Commercial License Usage
** Licensees holding a valid commercial QmlPanorama license may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement with the copyright holder. For licensing terms
** and conditions and further information contact the copyright holder.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3. The licenses are as published by
** the Free Software Foundation at https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "photosphere.h"
#include <QtQuick/QQuickWindow>
#include <QtGui/qopenglshaderprogram.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/QOpenGLTexture>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLFramebufferObjectFormat>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QEventLoop>
#include <QVector3D>
#include <QPointer>
#include <array>
#include <cmath>

namespace  {
const QMap<QString, CubeFace> nameToCubeFace {{
        {"PositiveZ" , CubeFace::PZ},
        {"PositiveX" , CubeFace::PX},
        {"PositiveY" , CubeFace::PY},
        {"NegativeZ" , CubeFace::MZ},
        {"NegativeY" , CubeFace::MY},
        {"NegativeX" , CubeFace::MX}
}};
#if 0
std::array<QColor, 6> faceColors {{ // for debugging purposes
        {255,0,0},
        {0,255,0},
        {0,0,255},
        {255,128,128},
        {128,255,128},
        {128,128,255}
    }};
#endif
static constexpr char vertexShaderSourceSphere[] =
"attribute highp vec4 vCoord;\n"
"attribute highp vec2 vTexCoord;\n"
"uniform highp mat4 matrix;\n"
"varying highp vec2 texCoord;\n"
"void main()\n"
"{\n"
"    texCoord = vTexCoord.xy;\n"
"    gl_Position = matrix * vCoord;\n"
"}\n"
"\n";

static constexpr char fragmentShaderSourceSphere[] =
"#define texture texture2D\n"
"varying highp vec2 texCoord;\n"
"uniform highp vec4 color;\n"
"uniform sampler2D samImage; \n"
"void main()\n"
"{\n"
"    lowp vec4 texColor = texture(samImage, texCoord.xy);\n"
"    gl_FragColor = vec4(texColor.rgb, color.a); \n"
"}\n"
"\n";

#if 0
static constexpr char vertexShaderSourceCubeDebug[] =
"attribute highp vec4 vCoord;\n"
"uniform highp mat4 matrix;\n"
"void main()\n"
"{\n"
"    gl_Position = matrix * gl_Vertex; //vCoord;\n"
"}\n"
"\n";

static constexpr char fragmentShaderSourceCubeDebug[] =
"uniform lowp vec4 color;\n"
"void main()\n"
"{\n"
"    gl_FragColor = color; \n"
"}\n"
"\n";
#endif
static constexpr char vertexShaderSourceCube[] =
"attribute highp vec4 vCoord;\n"
"attribute highp vec2 vTexCoord;\n"
"uniform highp mat4 matrix;\n"
"varying highp vec2 texCoord;\n"
"void main()\n"
"{\n"
"    texCoord = vTexCoord.xy;\n"
"    gl_Position = matrix * vCoord;\n"
"}\n"
"\n";

static constexpr char fragmentShaderSourceCube[] =
"#define texture texture2D\n"
"varying highp vec2 texCoord;\n"
"uniform highp vec4 color;\n"
"uniform sampler2D samImage; \n"
"void main()\n"
"{\n"
"    highp vec4 texColor = texture(samImage, texCoord.xy);\n"
"    gl_FragColor = vec4(texColor.rgb, color.a); \n"
"}\n"
"\n";
const char *cubeVertex = vertexShaderSourceCube;
const char *cubeFragment = fragmentShaderSourceCube;
}

/// PhotoSphereRenderState holds the state of the PhotoSphere (where is the user looking)
/// and it is isolated to a separate struct because it is exchanged between the GL renderer
/// and main application thread.
struct PhotoSphereRenderState {
    bool operator==(PhotoSphereRenderState const& o) const
    {
        return azimuth == o.azimuth
                && elevation == o.elevation
                && fov == o.fov
                && viewportHeight == o.viewportHeight
                && viewportWidth == o.viewportWidth
                && source.isSharedWith(o.source)
                && sourceCube.isSharedWith(o.sourceCube)
                && maxTexSize == o.maxTexSize;
    }

    float azimuth = 0;
    float elevation = 0;
    float fov = 90;
    int viewportWidth = 0;
    int viewportHeight = 0;
    QByteArray source;
    QMap<CubeFace, QByteArray> sourceCube;
    int maxTexSize = std::numeric_limits<int>::max();
};

/// This utility struct encapsulates the geometry of a sphere and
/// OpenGL code for rendering it. Assumes appropriate shader and
/// texture unit to be bound.

struct Sphere3D
{
    Sphere3D()
    {
        generateSphere();
    }

    void generateSphere()
    {
        static constexpr double step = 0.015625; // 32 stacks, 64 sectors

        static constexpr double pi = M_PI;
        static constexpr double pi2 =  2.0 * pi;
        static constexpr double di = step;
        static constexpr double dj = step * 2.0;
        static constexpr double du = di * 2.0 * pi;
        static constexpr double dv = dj * pi;

        for (double i = 0; i < 1.0; i += di)  //horizonal
        for (double j = 0; j < 1.0; j += dj)  //vertical
        {
            double u = (i * pi2) + (M_PI_2); // azimuth, rotated 90 degrees to make 0 point to north
            double v = (M_PI_2) - j * pi;         // elevation

            QVector3D bl(cos(u) * cos(v - dv),       sin(v - dv), -sin(u) * cos(v - dv));
            QVector3D br(cos(u + du) * cos(v - dv),  sin(v - dv), -sin(u + du) * cos(v - dv));
            QVector3D tr(cos(u + du) * cos(v),       sin(v),      -sin(u + du) * cos(v));
            QVector3D tl(cos(u) * cos(v),            sin(v),      -sin(u) * cos(v));

            // nullify zeroes
            QList<QVector3D *> vtx;
            vtx << &bl << &br << &tr << &tl;
            for(auto v: vtx) {
                for (auto c = 0 ; c < 3; ++c) {
                    if (qFuzzyIsNull((*v)[c]))
                        (*v)[c] = 0;
                }
            }

            QVector2D texBl(1.0 - i,        j + dj);
            QVector2D texBr(1.0 - i - di,   j + dj);
            QVector2D texTr(1.0 - i - di,   j);
            QVector2D texTl(1.0 - i,        j);

            m_sphereVertices << bl << tl << tr << bl << tr << br;
            m_texCoords << texBl << texTl << texTr << texBl << texTr << texBr;
        }
    }

    void init()
    {
        if (m_initialized)
            return;
        m_initialized = true;
        // vtx
        m_vertexDataBuffer.create();
        m_vertexDataBuffer.bind();
        m_vertexDataBuffer.allocate(&m_sphereVertices.front(), m_sphereVertices.size() * sizeof(QVector3D));
        m_vertexDataBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vertexDataBuffer.release();
        // texCoords
        m_texCoordBuffer.create();
        m_texCoordBuffer.bind();
        m_texCoordBuffer.allocate(&m_texCoords.front(), m_texCoords.size() * sizeof(QVector2D));
        m_texCoordBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_texCoordBuffer.release();

        QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao); // creates
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        m_vertexDataBuffer.bind();
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        m_vertexDataBuffer.release();
        m_texCoordBuffer.bind();
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        m_texCoordBuffer.release();
    }

    void drawSphere()
    {
        QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        f->glDrawArrays(GL_TRIANGLES, 0, m_sphereVertices.size());
    }

    QOpenGLVertexArrayObject m_vao;

    QOpenGLBuffer m_vertexDataBuffer;
    QOpenGLBuffer m_texCoordBuffer;
    QVector<QVector3D> m_sphereVertices;
    QVector<QVector2D> m_texCoords;

    bool m_initialized = false;
};

/// This utility struct encapsulates the geometry of a cube and
/// OpenGL code for rendering it. Assumes appropriate shader and
/// texture unit to be bound.

struct Cube3D
{
    Cube3D(float scale = 1.0f)
    : m_scale(scale),
      m_cubeVertices{{
        {-1.0f * m_scale,  1.0f * m_scale,  1.0f * m_scale}, // front
        {-1.0f * m_scale, -1.0f * m_scale,  1.0f * m_scale},
        { 1.0f * m_scale, -1.0f * m_scale,  1.0f * m_scale},
        { 1.0f * m_scale,  1.0f * m_scale,  1.0f * m_scale},
        {-1.0f * m_scale,  1.0f * m_scale, -1.0f * m_scale}, // back
        {-1.0f * m_scale, -1.0f * m_scale, -1.0f * m_scale},
        { 1.0f * m_scale, -1.0f * m_scale, -1.0f * m_scale},
        { 1.0f * m_scale,  1.0f * m_scale, -1.0f * m_scale}
      }},
      m_indices{{
        3, 2, 6, 7, // PX
        4, 0, 3, 7, // PY
        0, 1, 2, 3, // PZ
        4, 5, 1, 0, // MX
        1, 5, 6, 2, // MY
        7, 6, 5, 4, // MZ
      }},
     m_texCoordsFace{{ // invert x when looking from inside
       {1.0, 0.0},
       {1.0, 1.0},
       {0.0, 1.0},
       {0.0, 0.0}
     }}
    {
        for (int i = 0; i < int(m_indices.size()); ++i) {
            m_vertices[i] = m_cubeVertices[m_indices[i]];
            m_texCoords[i] = m_texCoordsFace[i % 4];
        }
    }

    void init()
    {
        if (m_initialized)
            return;
        m_initialized = true;
        // vtx
        m_vertexDataBuffer.create();
        m_vertexDataBuffer.bind();
        m_vertexDataBuffer.allocate(&m_vertices.front(), m_vertices.size() * sizeof(QVector3D));
        m_vertexDataBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vertexDataBuffer.release();
        // texCoords
        m_texCoordBuffer.create();
        m_texCoordBuffer.bind();
        m_texCoordBuffer.allocate(&m_texCoords.front(), m_texCoords.size() * sizeof(QVector2D));
        m_texCoordBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_texCoordBuffer.release();
        // VAOs
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        for (int i = CubeFace::PX; i != CubeFace::InvalidFace; i++ ) {
            QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao[i]); // creates
            m_vertexDataBuffer.bind();
            f->glEnableVertexAttribArray(0);
            f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *) (4 * i * sizeof (QVector3D)));
            m_vertexDataBuffer.release();
            m_texCoordBuffer.bind();
            f->glEnableVertexAttribArray(1);
            f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *) (4 * i * sizeof (QVector2D)));
            m_texCoordBuffer.release();
            // VertexAttribIPointer not available in GLES2
        }
    }

    /// Draw a face of the cube with OpenGL
    /// This method assumes texture data and relevant shader is bound
    void drawFace(CubeFace face)
    {
        int faceId(face);
        QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao[faceId]);
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        f->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // 6 VAOs, one per face
    std::array<QOpenGLVertexArrayObject, 6> m_vao;

    QOpenGLBuffer m_vertexDataBuffer;
    QOpenGLBuffer m_texCoordBuffer;
    float m_scale = 1.0f;
    std::array<QVector3D, 8> m_cubeVertices;
    std::array<quint16, 24> m_indices;
    std::array<QVector2D, 4> m_texCoordsFace;

    std::array<QVector3D, 24> m_vertices;
    std::array<QVector2D, 24> m_texCoords;

    bool m_initialized = false;
};

// -- QQuickFramebufferObject::Renderer implementations start -- //

/// Base class for QQuickFramebufferObject::Renderer encapsulating common code
class PhotoSphereRendererBase
{
protected:
    /// creates the framebuffer object with the desired format.
    QOpenGLFramebufferObject *createFbo(const QSize &size)
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(1);
        m_fbo =  new QOpenGLFramebufferObject(size, format);
        return m_fbo;
    }

    /// called in synchronize, pulls the state from QmlPhotoSphere into a PhotoSphereRenderState struct
    void updateState(QQuickFramebufferObject *item)
    {
        QmlPhotoSphere *itm = qobject_cast<QmlPhotoSphere *>(item);
        m_oldState = m_state;
        m_state.azimuth = itm->azimuth();
        m_state.elevation = itm->elevation();
        m_state.fov = itm->fieldOfView();
        m_state.viewportWidth = itm->width();
        m_state.viewportHeight = itm->height();
        m_state.source = itm->m_image;
        m_state.sourceCube = itm->m_cubeMap;
        m_state.maxTexSize = qMin(m_glMaxTexSize, itm->m_maximumTextureSize);

        if (itm->m_glMaxTexSize != m_glMaxTexSize) {
            // sadly init has no access to item, so we need to run this if at every update
            // just to initialize m_glMaxTexSize
            int oldValue = qMin(int(itm->m_glMaxTexSize), itm->m_maximumTextureSize);
            itm->m_glMaxTexSize = m_glMaxTexSize;
            int newValue = qMin(int(itm->m_glMaxTexSize), itm->m_maximumTextureSize);
            if (oldValue != newValue)
                QMetaObject::invokeMethod(itm, "signalUpdatedMaxSize", Qt::QueuedConnection);
        }
    }

    /// called in init of the subclasses
    void initBase(QOpenGLFunctions *f, QQuickWindow *w)
    {
        f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_glMaxTexSize);
        m_shader = new QOpenGLShaderProgram;
        m_window = w;
    }

    virtual void init(QOpenGLFunctions *f, QQuickWindow *w) = 0;

    /// encapsulate common code in synchronize. returns false if rest is to be skipped
    bool synchronize(QQuickFramebufferObject *item)
    {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        init(f, item->window());
        updateState(item);

        return !(m_state == m_oldState);
    }

    QOpenGLShaderProgram *m_shader = nullptr;
    QPointer<QQuickWindow> m_window;
    PhotoSphereRenderState m_state, m_oldState;
    QOpenGLFramebufferObject *m_fbo = nullptr;
    int m_glMaxTexSize = std::numeric_limits<int>::max();
    QMatrix4x4 m_mvp;
};


/// Subclass of QQuickFramebufferObject::Renderer to render equirectangular images
class PhotoSphereRenderer : public PhotoSphereRendererBase, public QQuickFramebufferObject::Renderer
{
public:
    PhotoSphereRenderer() { }

    ~PhotoSphereRenderer() override
    {
        delete m_texPhotoSphere;
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        return createFbo(size);
    }

    void render() override
    {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

        const bool texturing = m_texPhotoSphere->isStorageAllocated() && m_texPhotoSphere->width() > 1;

        f->glClearColor(0, 0, 0, 0);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LESS);
        f->glDepthMask(true);

        m_shader->bind();
        m_shader->setUniformValue("matrix", m_mvp);
        m_shader->setUniformValue("samImage", 0);
        m_shader->setUniformValue("color", QColor(0,0,0,255));

        if (texturing)
            m_texPhotoSphere->bind(0);
        m_sphere.drawSphere();
        if (texturing)
            m_texPhotoSphere->release();

        m_shader->release();

        if (m_window)
            m_window->resetOpenGLState();
    }

    // It is called in updatePaintNode, so to copy data over to the render thread while
    // it is locked.
    void synchronize(QQuickFramebufferObject *item) override
    {
        if (!PhotoSphereRendererBase::synchronize(item))
            return;

        if (m_state.viewportHeight != m_oldState.viewportHeight
                || m_state.viewportWidth != m_oldState.viewportWidth)
            invalidateFramebufferObject();

        const float ar = float(m_state.viewportWidth) / float(m_state.viewportHeight);

        QMatrix4x4 matProjection;
        matProjection.perspective(m_state.fov, ar, 0.001, 200);

        QMatrix4x4 matAzimuth;
        matAzimuth.rotate(m_state.azimuth, 0, 1, 0);

        QMatrix4x4 matElevation;
        matElevation.rotate(m_state.elevation, -1, 0, 0);


        m_mvp = QMatrix4x4()
                * matProjection
                * matElevation
                * matAzimuth
                ;

        if (m_oldState.source != m_state.source) {
            auto image = QImage::fromData(m_state.source);
            if (image.isNull() || !image.width() || !image.height())
                return;
            m_texPhotoSphere->destroy();
            if (image.width() > m_glMaxTexSize)
                image = image.scaledToWidth(m_glMaxTexSize, Qt::FastTransformation);

            m_texPhotoSphere->setData(image);
            m_texPhotoSphere->setAutoMipMapGenerationEnabled(true);
            m_texPhotoSphere->setMaximumAnisotropy(16.0f);
            m_texPhotoSphere->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            m_texPhotoSphere->setMagnificationFilter(QOpenGLTexture::Linear);
        }
    }

protected:
    void init(QOpenGLFunctions *f, QQuickWindow *w) override
    {
        if (!m_shader) {
            initBase(f, w);
            m_sphere.init();

            // Create shaders
            m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, QByteArray(vertexShaderSourceSphere));
            m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, QByteArray(fragmentShaderSourceSphere));
            m_shader->bindAttributeLocation("vCoord", 0);
            m_shader->link();

            m_texPhotoSphere = new QOpenGLTexture(QOpenGLTexture::Target2D);
        }
    }

    Sphere3D m_sphere;
    QOpenGLTexture *m_texPhotoSphere = nullptr;
};

/// Subclass of QQuickFramebufferObject::Renderer to render cube maps
class PhotoSphereRendererCube : public PhotoSphereRendererBase, public QQuickFramebufferObject::Renderer
{
public:
    PhotoSphereRendererCube() { }

    // called in the render thread
    ~PhotoSphereRendererCube() override
    {
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        return createFbo(size);
    }

    void render() override
    {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        f->glClearColor(0, 0, 0, 0);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LESS);
        f->glDepthMask(true);

        m_shader->bind();
        m_shader->setUniformValue("matrix", m_mvp);

        m_shader->setUniformValue("samImage", 0);

        for (int i = CubeFace::PX; i != CubeFace::InvalidFace; i++ ) {
            CubeFace face = CubeFace(i);
//            m_shader->setUniformValue("color", faceColors.at(i));
            const auto &tex = m_texFaces.value(face);
            const bool texturing = tex->isStorageAllocated() && tex->width() > 1;
            if (texturing)
                tex->bind(0);
            m_shader->setUniformValue("color", QColor(255,255,255));
            m_cube.drawFace(face);
            if (texturing)
                tex->release();
        }

        m_shader->release();

        if (m_window)
            m_window->resetOpenGLState();
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        if (!PhotoSphereRendererBase::synchronize(item))
            return;

        if (m_state.viewportHeight != m_oldState.viewportHeight
                || m_state.viewportWidth != m_oldState.viewportWidth)
            invalidateFramebufferObject();

        const float ar = float(m_state.viewportWidth) / float(m_state.viewportHeight);

        QMatrix4x4 matProjection;
        matProjection.perspective(m_state.fov, ar, 0.001, 200);

        QMatrix4x4 matAzimuth;
        matAzimuth.rotate(m_state.azimuth, 0, 1, 0);

        QMatrix4x4 matElevation;
        matElevation.rotate(m_state.elevation, -1, 0, 0);

        m_mvp = QMatrix4x4()
                * matProjection
                * matElevation
                * matAzimuth
                ;

#if 0
        // cube testing
        matProjection.setToIdentity();
        matProjection.ortho(-2,2,-2,2,-10,10);
        QMatrix4x4 matQuickize;
        matQuickize.setToIdentity();
        matQuickize.translate(0,0,-5); // repurposing
        // rotate it a bit, and traslate away from the center along the Z axis
        m_mvp = QMatrix4x4()
                * matProjection
                * matQuickize
                * matAzimuth // rotate around y
                * matElevation
                ;
#endif

        if (m_oldState.sourceCube != m_state.sourceCube
                || m_oldState.maxTexSize != m_state.maxTexSize) { // reload all
            // if here, sourceCube has been already validate in the setter
            for (int i = CubeFace::PX; i != CubeFace::InvalidFace; i++ ) {
                CubeFace face = CubeFace(i);
                auto &t = m_texFaces[face];
                t->destroy();
                auto image = QImage::fromData(m_state.sourceCube.value(face));
                int maxSz = qMin(m_state.maxTexSize, m_glMaxTexSize);
                if (image.width() > maxSz)
                    image = image.scaledToWidth(maxSz, Qt::FastTransformation);
                t->setData(image);
                t->setAutoMipMapGenerationEnabled(true);
                t->setMaximumAnisotropy(16.0f);
                t->setWrapMode(QOpenGLTexture::ClampToEdge);
                t->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
                t->setMagnificationFilter(QOpenGLTexture::Linear);
                // ToDo: consider adding some LOD bias for improved sharpness
            }
        }
    }

protected:
    void init(QOpenGLFunctions *f, QQuickWindow *w) override
    {
        if (!m_shader) {
            m_cube.init();
            initBase(f, w);

            m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                              QByteArray(cubeVertex));
            m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                              QByteArray(cubeFragment));
            m_shader->bindAttributeLocation("vCoord", 0);
            m_shader->bindAttributeLocation("vTexCoord", 1);
            m_shader->link();

            for (int i = CubeFace::PX; i != CubeFace::InvalidFace; i++ ) {
                CubeFace face = CubeFace(i);
                QSharedPointer<QOpenGLTexture> tex(
                            new QOpenGLTexture(QOpenGLTexture::Target2D));
                m_texFaces[face].swap(tex);
            }
        }
    }

    Cube3D m_cube;
    QMap<CubeFace, QSharedPointer<QOpenGLTexture>> m_texFaces;
};



/*
 *
 * QmlPhotoSphere Item implementation
 *
 */


/*!
    \qmltype PhotoSphere
    \instantiates QmlPhotoSphere
    \inqmlmodule QmlPanorama

    \brief The PhotoSphere type displays a spherical panorama, provided in
    form of an equirectangular image, or the six separate images of a cube map.

    \section2 Example Usage

    The following snippet shows a PhotoSphere containing elements to handle
    interaction

    \code
        PhotoSphere {
            id: sphere
            anchors.fill: parent
            maximumTextureSize: 8192
            source: <url to the source>

            PinchArea {
                id: pa
                anchors.fill: parent
                property real initialFov

                onPinchStarted: {
                    initialFov = sphere.fieldOfView
                }

                function diff(p1, p2) {
                    return Qt.point(p2.x - p1.x , p2.y - p1.y)
                }

                onPinchUpdated: {
                    var initialPointDiff = diff(pinch.startPoint2, pinch.startPoint1)
                    var initialLength = Math.sqrt(initialPointDiff.x * initialPointDiff.x +
                                                  initialPointDiff.y * initialPointDiff.y)

                    var curPointDiff = diff(pinch.point2, pinch.point1)
                    var curLength = Math.sqrt(curPointDiff.x * curPointDiff.x +
                                              curPointDiff.y * curPointDiff.y)

                    var rate = initialLength / curLength
                    sphere.fieldOfView = initialFov * rate
                }

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    property var clickedPos;
                    property var clickedAzimuth;
                    property var clickedElevation;
                    property var clickedFoV;
                    onPressed: {
                        clickedPos = Qt.point(mouseX, mouseY)
                        clickedAzimuth = sphere.azimuth
                        clickedElevation = sphere.elevation
                    }
                    onPositionChanged: {
                        var curpos = Qt.point(mouseX, mouseY)
                        var posDiff = pa.diff(ma.clickedPos, curpos)

                        var refTan = Math.tan(90.0 * 0.5 * Math.PI / 180);
                        var curTan = Math.tan(sphere.fieldOfView * 0.5 * Math.PI / 180);

                        sphere.azimuth = clickedAzimuth - ((768.0 / win.height) * posDiff.x / 6.0) * (curTan / refTan)
                        sphere.elevation = clickedElevation + ((768.0 / win.height) * posDiff.y / 6.0) * (curTan / refTan)
                    }
                    onWheel: {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            sphere.fieldOfView -=  wheel.angleDelta.y / 120;
                        }
                    }
                }
            }
        }
    \endcode
*/
QmlPhotoSphere::QmlPhotoSphere(QQuickItem *parent) : QQuickFramebufferObject(parent)
  , m_recreateRenderer(false)
{
  setFlag(ItemHasContents);
  setTextureFollowsItemSize(true);
  setMirrorVertically(true);
}

QmlPhotoSphere::~QmlPhotoSphere()
{

}

qreal QmlPhotoSphere::azimuth() const
{
    return m_azimuth;
}

void QmlPhotoSphere::setAzimuth(qreal azimuth)
{
    if (!qIsFinite(azimuth))
            return;

    azimuth = std::fmod(azimuth, qreal(360.0));
    if (azimuth < 0.0)
        azimuth += 360.0;

    if (azimuth == m_azimuth)
        return;

    m_azimuth = azimuth;
    updateSphere();
    emit azimuthChanged(azimuth);
}

qreal QmlPhotoSphere::elevation() const
{
    return m_elevation;
}

void QmlPhotoSphere::setElevation(qreal elevation)
{
    if (elevation == m_elevation || !qIsFinite(elevation))
        return;

    elevation = qBound<double>(-90.0, elevation, 90.0);
    m_elevation = elevation;
    updateSphere();
    emit elevationChanged(elevation);
}

qreal QmlPhotoSphere::fieldOfView() const
{
    return m_fieldOfView;
}

void QmlPhotoSphere::setFieldOfView(qreal fov)
{
    if (fov == m_fieldOfView || fov < 3.0 || fov > 150.0) // arbitrary selection of FOVs.
        return;                                           // >150 gets hard to look at

    m_fieldOfView = fov;
    updateSphere();
    emit fieldOfViewChanged(fov);
}

QVariant QmlPhotoSphere::source() const
{
    if (!m_cubeMapUrls.isEmpty())
        return m_cubeMapUrls;
    return m_imageUrl;
}

static QByteArray fetchUrl(const QUrl &url)
{
    QNetworkRequest request;
    request.setUrl(url);
    QEventLoop syncLoop;
    QScopedPointer<QNetworkAccessManager> nam(new QNetworkAccessManager);

    QNetworkReply *reply = nam->get(request);
    if (!reply->isFinished()) {
        QObject::connect(reply, SIGNAL(finished()), &syncLoop, SLOT(quit()));
        syncLoop.exec();

        if (!reply->isFinished()) {
            // This shouldn't happen
            qWarning() << "Unfinished reply";
            return QByteArray();
        }
    }
    reply->deleteLater();
    return reply->readAll();
}

bool QmlPhotoSphere::loadFromUrl(const QString &url)
{
    if (url.isEmpty())
        return false;

    if (url == m_imageUrl)
        return true;

    QUrl u(url);
    if (!u.isValid()) {
        qWarning() << "Attempting to load invalid URL: "<< u;
        return false;
    }

    /*
        Load the image
    */

    QByteArray data = fetchUrl(u);
    QImage image;
    if (!image.loadFromData(data)) {
        qWarning() << "QImage::loadFromData failed for "<< u;
        return false;
    }

    if (image.isNull() || !image.width() || !image.height()) {
        qWarning() << "Empty image at "<< u;
        return false;
    }

    m_imageUrl = url;
    m_image = data;

    updateSphere();
    emit sourceChanged();
    return true;
}

bool QmlPhotoSphere::loadFromCubeMap(const QVariantMap &cubeMap)
{
    if (!cubeMap.size())
        return false;
    if (m_cubeMapUrls == cubeMap)
        return true;
    QSet<QString> keys = cubeMap.keys().toSet();
    static const QList<QString> requiredKeys{
        "PositiveX", "PositiveY", "PositiveZ", "NegativeX", "NegativeY", "NegativeZ"
    };
    for (auto k: requiredKeys) {
        if (!keys.contains(k)) {
            qWarning() << "Missing required texture in cube map. required textures: "<< requiredKeys;
            Q_ASSERT(false);
            return false;
        } else if (!cubeMap.value(k).canConvert<QString>()) {
            qWarning() << "value for "<<k<< " not a valid url";
            return false;
        }
    }
    for (auto k: requiredKeys) {
        QUrl u(cubeMap.value(k).value<QString>());
        if (!u.isValid())
            return false;
    }

    QMap<CubeFace, QByteArray> cubeMapImages;
    for (auto k: requiredKeys) {
        QUrl u(cubeMap.value(k).value<QString>());
        QByteArray data = fetchUrl(u);
        QImage image;
        if (!image.loadFromData(data)) {
            qWarning() << "QImage::loadFromData failed for "<< u;
            return false;
        }

        if (image.isNull() || !image.width() || !image.height()) {
            qWarning() << "Empty image at "<< u;
            return false;
        }

        cubeMapImages[nameToCubeFace.value(k)] = data;

    }
    m_cubeMap = cubeMapImages;
    m_cubeMapUrls = cubeMap;

    updateSphere();
    emit sourceChanged();
    return true;
}

void QmlPhotoSphere::setSource(const QVariant &source)
{
    if (source.canConvert<QString>()) {
        QString url = source.toString();
        if (loadFromUrl(url)) {
            if (m_rendererType != RendererType::SphereRenderer)
                m_recreateRenderer = true;
            m_rendererType = RendererType::SphereRenderer;
            m_cubeMap.clear();
            m_cubeMapUrls.clear();
        } else {
            qWarning() << "Failed setting source property to invalid value: "<< url;
        }
    } else if (source.canConvert<QVariantMap>()) {
        const QVariantMap cubeMap = source.value<QVariantMap>();
        if (loadFromCubeMap(cubeMap)) {
            if (m_rendererType != RendererType::CubeRenderer)
                m_recreateRenderer = true;
            m_rendererType = RendererType::CubeRenderer;
            m_image.clear();
            m_imageUrl.clear();
        } else {
            qWarning() << "Failed setting source property to invalid value: "<< cubeMap;
        }
    }
}

int QmlPhotoSphere::maximumTextureSize() const
{
    return qMin(m_maximumTextureSize, int(m_glMaxTexSize));
}

void QmlPhotoSphere::signalUpdatedMaxSize()
{
    emit maximumTextureSizeChanged();
}

void QmlPhotoSphere::setMaximumTextureSize(int maxTexSize)
{
    if (maxTexSize == m_maximumTextureSize)
        return;

    int oldSz = maximumTextureSize();
    m_maximumTextureSize = maxTexSize;
    int newSz = maximumTextureSize();
    if (oldSz == newSz)
        return;
    updateSphere();
    emit maximumTextureSizeChanged();
}

QSGNode *QmlPhotoSphere::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    if (oldNode && m_recreateRenderer) {
        delete oldNode;
        oldNode = nullptr;
        releaseResources(); // nullifies d->node
        m_recreateRenderer = false;
    }
    return QQuickFramebufferObject::updatePaintNode(oldNode, data);
}

QQuickFramebufferObject::Renderer *QmlPhotoSphere::createRenderer() const
{
    if (m_rendererType == RendererType::CubeRenderer)
        return new PhotoSphereRendererCube;
    else
        return new PhotoSphereRenderer;
}

void QmlPhotoSphere::updateSphere()
{
    m_nodeDirty = true;
    polish();
    update();
}




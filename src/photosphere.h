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

#ifndef PHOTOSPHERE_H
#define PHOTOSPHERE_H

#include <QQuickItem>
#include <QImage>
#include <QVariantMap>
#include <QAtomicInt>
#include <QQuickFramebufferObject>

enum CubeFace {
    PX = 0,
    PY,
    PZ,
    MX,
    MY,
    MZ,
    InvalidFace
};

enum RendererType {
    CubeRenderer = 0,
    SphereRenderer
};

class QmlPhotoSphere : public QQuickFramebufferObject
{
    Q_OBJECT

    // Make implicitWidth and implicitHeight read-only. Equivalent of inheriting QQuickImplicitSizeItem
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)

    Q_PROPERTY(qreal azimuth READ azimuth WRITE setAzimuth NOTIFY azimuthChanged)
    Q_PROPERTY(qreal elevation READ elevation WRITE setElevation NOTIFY elevationChanged)
    Q_PROPERTY(qreal fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY fieldOfViewChanged)
    Q_PROPERTY(int maximumTextureSize READ maximumTextureSize WRITE setMaximumTextureSize NOTIFY maximumTextureSizeChanged)
    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY sourceChanged)


public:
    QmlPhotoSphere(QQuickItem *parent = nullptr);
    ~QmlPhotoSphere();

/*!
    \qmlproperty float PhotoSphere::azimuth

    The azimuth of the viewer, in degrees.
    The default value is 0 degrees. Values outside [0, 360] will be
    wrapped.
    A value of 0 is intended to face seams (the
    left or right most part of the equirectangular photo sphere), or
    the center of the back face of the cube map.
    Any other value is intended to be the amount of degrees away from the
    seams.
 */
    qreal azimuth() const;
    void setAzimuth(qreal azimuth);

/*!
    \qmlproperty float PhotoSphere::elevation

    The elevation of the viewer, in degrees, above (or below) the horizon.
    The default value is 0 degrees. Values outside [-90, -90] will be
    clamped.
    A value of 0 is intended to be parallel to the horizon,
    a value of 90 to be looking vertically upwards at the top of the sphere, and
    a value of -90 to be looking vertically downwards at the bottom of the sphere.
 */
    qreal elevation() const;
    void setElevation(qreal elevation);

/*!
    \qmlproperty float PhotoSphere::fieldOfView

    The field of view of the viewer, in degrees.
    The default value is 90 degrees.
    Values will be clamped outside the [3, 150] range.
    Modify this property to zoom in or out.
 */
    qreal fieldOfView() const;
    void setFieldOfView(qreal fov);

/*!
    \qmlproperty variant PhotoSphere::source

    This property holds the url of the photo sphere source.
    The url can be either a single image file, containing an equirectangular
    photo sphere, or a map of 6 image files, each representing a face of a
    cube map, using the keys documented by the snippet below.
    If a single image and the content is not an equirectangular projection of
    a full spherical panorama, the image will still be displayed as if it is,
    therefore distorted.

    \code
    source: {
        "PositiveX" : "scheme://path/to/positive_X.ext",
        "PositiveY" : "scheme://path/to/positive_Y.ext, // top
        "PositiveZ" : "scheme://path/to/positive_Z.ext, // front
        "NegativeX" : "scheme://path/to/negative_X.ext,
        "NegativeY" : "scheme://path/to/negative_Y.ext, // bottom
        "NegativeZ" : "scheme://path/to/negative_Z.ext, // back
    }
    \endcode
 */
    QVariant source() const;
    void setSource(const QVariant &source);

/*!
    \qmlproperty int PhotoSphere::maximumTextureSize

    The maximum size of textures used in the element. This property
    can be used to limit the max size of the graphics texture resources.
    Setting it to a value larger than the maximum available texture size
    in OpenGL will result in a clamped value.
    The default value is OpenGL device-specific maximum texture size.
 */
    int maximumTextureSize() const;
    void setMaximumTextureSize(int maxTexSize);

signals:
    void azimuthChanged(qreal azimuth);
    void elevationChanged(qreal elevation);
    void fieldOfViewChanged(qreal fov);
    void sourceChanged();
    void maximumTextureSizeChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    Renderer *createRenderer() const override;
    void updateSphere();
    bool loadFromUrl(const QString &url);
    bool loadFromCubeMap(const QVariantMap &map);

protected slots:
    void signalUpdatedMaxSize();

private:
    qreal m_azimuth = 0;
    qreal m_elevation = 0;
    qreal m_fieldOfView = 90;
    bool m_recreateRenderer = false;
    bool m_nodeDirty = false;
    int m_maximumTextureSize = 65536; // a value large enough to be clamped in any case
    QAtomicInt m_glMaxTexSize = -1;

    QByteArray m_image;
    QString m_imageUrl;

    QMap<CubeFace, QByteArray> m_cubeMap;
    QVariantMap m_cubeMapUrls;
    RendererType m_rendererType = RendererType::CubeRenderer;

    friend class PhotoSphereRendererBase;
    friend class PhotoSphereRenderer;
    friend class PhotoSphereRendererCube;
    Q_DISABLE_COPY(QmlPhotoSphere)
};

#endif // PHOTOSPHERE_H

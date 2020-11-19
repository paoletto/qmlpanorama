#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include "qmlpanorama.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Explicitly request a GLES2.0 context, for testing purposes.
    // This should normally not be necessary.
    QSurfaceFormat fmt;
    fmt.setVersion(2, 0);
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    QSurfaceFormat::setDefaultFormat(fmt);

    registerQmlPanorama();
    QQmlApplicationEngine engine;

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}

TEMPLATE = lib
TARGET = qmlpanorama
QT += qml quick
CONFIG += plugin

TARGET = $$qtLibraryTarget($$TARGET)
uri = QmlPanorama

include(qmlpanorama.pri)

SOURCES += $${PWD}/src/plugin.cpp

RESOURCES += $$files(*.qrc)

OTHER_FILES +=  $$files(src/js/*.js) \
        $$files(src/qml/*.qml) \
                *.pri \
                *.qrc \
                README \
                LICENSE \
                qmldir
DISTFILES = qmldir

!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
    copy_qmldir.target = $$OUT_PWD/qmldir
    copy_qmldir.depends = $$_PRO_FILE_PWD_/qmldir
    copy_qmldir.commands = $(COPY_FILE) "$$replace(copy_qmldir.depends, /, $$QMAKE_DIR_SEP)" "$$replace(copy_qmldir.target, /, $$QMAKE_DIR_SEP)"
    QMAKE_EXTRA_TARGETS += copy_qmldir
    PRE_TARGETDEPS += $$copy_qmldir.target
}

qmldir.files = qmldir
installPath = $$[QT_INSTALL_QML]/$$replace(uri, \., /)
qmldir.path = $$installPath
target.path = $$installPath
INSTALLS += target qmldir

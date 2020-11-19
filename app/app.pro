TEMPLATE = app

QT += qml quick
CONFIG += c++11

include(${PWD}/../../qmlpanorama.pri)
SOURCES += main.cpp

RESOURCES += qml.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

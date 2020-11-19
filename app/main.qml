import QtQuick 2.7
import QtQuick.Window 2.2
import QmlPanorama 1.0

Window {
    id: win
    visible: true
    width: 640
    height: 480
    title: qsTr("QmlPanorama Demo")

//    property string basePath : "file:///<path/to>/qmlpanorama/sample_data/"
//    property string basePath : "http://192.168.1.9:8080/"
    property var photo1: {
        "PositiveX" : basePath + "kloster_weltenburg_cube_face_1.jpg",
        "PositiveY" : basePath + "kloster_weltenburg_cube_face_4.jpg", // top
        "PositiveZ" : basePath + "kloster_weltenburg_cube_face_2.jpg", // front
        "NegativeX" : basePath + "kloster_weltenburg_cube_face_3.jpg",
        "NegativeY" : basePath + "kloster_weltenburg_cube_face_5.jpg", // bottom
        "NegativeZ" : basePath + "kloster_weltenburg_cube_face_0.jpg", // back
    }
    property string photo2 : basePath + "kloster_weltenburg.jpg"

    function mapEqual(m1, m2)
    {
        if (typeof m1 !== 'object' || typeof m2 !== 'object' )
            return false;
        if (!("PositiveX" in m1) || !("PositiveX" in m2)) return false;
        if (!("PositiveY" in m1) || !("PositiveY" in m2)) return false;
        if (!("PositiveZ" in m1) || !("PositiveZ" in m2)) return false;
        if (!("NegativeX" in m1) || !("NegativeX" in m2)) return false;
        if (!("NegativeY" in m1) || !("NegativeY" in m2)) return false;
        if (!("NegativeZ" in m1) || !("NegativeZ" in m2)) return false;
        return     m1["PositiveX"] ==  m2["PositiveX"]
                && m1["PositiveY"] ==  m2["PositiveY"]
                && m1["PositiveZ"] ==  m2["PositiveZ"]
                && m1["NegativeX"] ==  m2["NegativeX"]
                && m1["NegativeY"] ==  m2["NegativeY"]
                && m1["NegativeZ"] ==  m2["NegativeZ"];
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            console.log(sphere.source)
            if (sphere.source ===  photo1 || mapEqual(sphere.source, photo1)) {
                console.log("setting to photo2")
                sphere.source = photo2
            } else {
                console.log("setting to photo1")
                sphere.source =  photo1
            }
        }
    }

    Rectangle {
        id: container
        anchors.fill: parent
        visible: true
        color: "transparent"

        PhotoSphere {
            id: sphere
            visible: true
            anchors.fill: parent
            // -- anchors block to test compatibility with QtQuick scenegraph -- //
//            anchors {
//                left: parent.horizontalCenter
//                top: parent.verticalCenter
//                right: parent.right
//                bottom: parent.bottom
//            }

            maximumTextureSize: 8192
            onMaximumTextureSizeChanged: {
                console.log("maxtexsize: ",maximumTextureSize)
            }

            opacity: parent.opacity

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

                        // mouse displacement to azimuth delta will be affected by viewport size
                        var refTan = Math.tan(90.0 * 0.5 * Math.PI / 180);
                        var curTan = Math.tan(sphere.fieldOfView * 0.5 * Math.PI / 180);

                        sphere.azimuth = clickedAzimuth - ((768.0 / win.height) * posDiff.x / 6.0) * (curTan / refTan)
                        sphere.elevation = clickedElevation + ((768.0 / win.height) * posDiff.y / 6.0) * (curTan / refTan)
                    }

                    onWheel: {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            sphere.fieldOfView -=  wheel.angleDelta.y / 30;
                        } else { // panning with touchpad
                            sphere.azimuth +=  wheel.angleDelta.x / 10
                            sphere.elevation -=  wheel.angleDelta.y / 30
                        }
                    }
                }
            }
        }
    }
}

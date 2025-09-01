import QtQuick
import CustomItems 1.0
Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("AZPlayer")

    VideoItem {
            id: video
            objectName: "video"
            anchors.fill: parent
        }
}

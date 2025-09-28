import QtQuick
import QtQuick.Controls
import VideoWindow 1.0
Window {
    id: mainWin
    width: 640
    height: 480
    visible: true
    title: qsTr("AZPlayer")

    // 全屏状态属性
    property bool isFullScreen: false

    // 快捷键处理
    Shortcut {
        sequence: "F11"
        onActivated: {
            toggleFullScreen()
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (isFullScreen) {
                toggleFullScreen()
            } else {
                mainWin.close()
            }
        }
    }

    // 全屏切换
    function toggleFullScreen() {
        if (isFullScreen) {
            mainWin.showNormal()
            isFullScreen = false
        } else {
            mainWin.showFullScreen()
            isFullScreen = true
        }
    }


    Rectangle{
        anchors.fill: parent
        color: "blue"
        VideoWindow{
            anchors.fill: parent
            width: 500
            height: 500
            id:videoWindow
            objectName: "videoWindow"
        }
    }
}

import QtQuick
import QtQuick.Controls
//import Qt.labs.platform
import QtQuick.Dialogs
import VideoWindow 1.0
import QtQuick.Layouts
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

    ColumnLayout{
        anchors.fill: parent
        spacing: 0 //子项之间的间隙
        Rectangle{
            color: "#eef4f9"
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.maximumHeight: 32
            Layout.minimumHeight: 32

            ComboBox{
                displayText: "AZPlayer"
                height: parent.height
                anchors.left: parent.left
                width: 80
                model: ["打开文件", "选项二", "选项三"]
                onActivated: function(idx) {
                    if (idx === 0) fileDialog.open()
                }
            }
        }

        Rectangle{
            color: "#171717"
            Layout.fillWidth: true
            Layout.preferredHeight: 2
        }

        // 视频显示区域
        Rectangle{
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#101010"
            VideoWindow{
                anchors.fill: parent
                id:videoWindow
                objectName: "videoWindow"
                Component.onCompleted:{
                    MediaCtrl.setVideoWindow(videoWindow)
                }
            }
        }
    }



    FileDialog {
        id: fileDialog
        title: "选择一个视频文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFile
        nameFilters: ["视频文件 (*.mp4 *.mkv *.avi)", "所有文件 (*)"]
        onAccepted: {
            console.log("选中的文件:", selectedFile)
            var ok = MediaCtrl.open(selectedFile)
            if(ok){
                mainWin.title = selectedFile.toString().split(/[\\/]/).pop() + " - " + "AZPlayer"
            }
        }
        onRejected: console.log("取消选择")
    }
}

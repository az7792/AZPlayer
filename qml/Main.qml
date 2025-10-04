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
    color: "black"
    visibility: Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window| Qt.WindowSystemMenuHint|
           Qt.WindowMaximizeButtonHint | Qt.WindowMinimizeButtonHint

    property int resizeMargin: 8

    //用于显示提示信息
    AZTooltip {
        id: tooltip
        targetWindow: mainWin
    }

    //用于调整窗口大小
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents:true


        onPositionChanged:function(mouse) {
            // 改变鼠标样式
            if (mouse.x <= resizeMargin && mouse.y <= resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 左上角
            else if (mouse.x >= parent.width - resizeMargin && mouse.y <= resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 右上角
            else if (mouse.x <= resizeMargin && mouse.y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 左下角
            else if (mouse.x >= parent.width - resizeMargin && mouse.y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 右下角
            else if (mouse.x <= resizeMargin || mouse.x >= parent.width - resizeMargin)
                cursorShape = Qt.SizeHorCursor     // 左右边
            else if (mouse.y <= resizeMargin || mouse.y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeVerCursor     // 上下边
            else
                cursorShape = Qt.ArrowCursor
        }

        onPressed: function(mouse) {
            let edge = 0
            if (mouse.x <= resizeMargin && mouse.y <= resizeMargin)
                edge = Qt.LeftEdge | Qt.TopEdge          // 左上角
            else if (mouse.x >= parent.width - resizeMargin && mouse.y <= resizeMargin)
                edge = Qt.RightEdge | Qt.TopEdge         // 右上角
            else if (mouse.x <= resizeMargin && mouse.y >= parent.height - resizeMargin)
                edge = Qt.LeftEdge | Qt.BottomEdge       // 左下角
            else if (mouse.x >= parent.width - resizeMargin && mouse.y >= parent.height - resizeMargin)
                edge = Qt.RightEdge | Qt.BottomEdge      // 右下角
            else if (mouse.x <= resizeMargin)
                edge = Qt.LeftEdge                        // 左边
            else if (mouse.x >= parent.width - resizeMargin)
                edge = Qt.RightEdge                       // 右边
            else if (mouse.y <= resizeMargin)
                edge = Qt.TopEdge                         // 上边
            else if (mouse.y >= parent.height - resizeMargin)
                edge = Qt.BottomEdge                      // 下边

            if (edge !== 0) {
                mainWin.startSystemResize(edge)
            }
        }
    }

    ColumnLayout{
        anchors.fill: parent
        anchors.margins: 3
        spacing: 1
        RowLayout{
            spacing: 1
            Layout.fillWidth: true
            Layout.minimumHeight: 32
            Layout.maximumHeight: 32

            AZButton{
                Layout.fillHeight: true
                Layout.minimumWidth: 80
                Layout.maximumWidth: 80
                defaultColor: "#1c1c1c"
                hoverColor: "#252525"
                pressedColor:"#161616"
                text: "AZPlayer"
                tooltipText: "测试"
                iconHeight: 16
                iconWidth: 16
                onClicked: console.log("按下")
                onLeftClicked: console.log("L按下")
                onRightClicked: console.log("R按下")
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }

            Rectangle{
                id: textWrapper
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1d1d1d"
                clip: true
                property alias text: titleText.text
                MouseArea {
                    anchors.fill: parent
                    onPressed: mainWin.startSystemMove()
                    onDoubleClicked: mainWin.visibility = mainWin.visibility == Window.Windowed ? Window.Maximized : Window.Windowed
                }
                Text {
                    id: titleText
                    color: "white"
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                }
            }

            AZWindowControls{
                targetWindow: mainWin
                targetToolTip: tooltip
                Layout.fillHeight: true
                Layout.minimumWidth: 120
                Layout.maximumWidth: 120
            }
        }

        //视频显示区域
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

        Rectangle{
            Layout.fillWidth: true;
            Layout.minimumHeight: 20
            Layout.maximumHeight: 20
            color: "#1d1d1d"
        }

        RowLayout{
            spacing: 1
            Layout.fillWidth: true;
            Layout.minimumHeight: 40
            Layout.maximumHeight: 40

            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                iconWidth: 16
                iconHeight: 16
                iconSource: "qrc:/icon/play.png"
                tooltipText: "播放"
                onLeftClicked: ;
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }
            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                iconWidth: 16
                iconHeight: 16
                iconSource: "qrc:/icon/stop.png"
                tooltipText: "停止"
                onLeftClicked: MediaCtrl.close()
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }
            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                iconWidth: 16
                iconHeight: 16
                iconSource: "qrc:/icon/skip_previous.png"
                tooltipText: "L:快退，R:上一个"
                onLeftClicked: ;
                onRightClicked: ;
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }
            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                iconWidth: 16
                iconHeight: 16
                iconSource: "qrc:/icon/skip_next.png"
                tooltipText: "L:快进，R:下一个"
                onLeftClicked: ;
                onRightClicked: ;
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }
            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                iconWidth: 16
                iconHeight: 16
                iconSource: "qrc:/icon/open.png"
                tooltipText: "打开文件"
                onLeftClicked: fileDialog.open()
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
            }

            Rectangle{
                Layout.fillWidth: true;
                Layout.fillHeight: true
                color: "#1c1c1c"
            }

            AZButton{
                Layout.fillHeight: true
                Layout.preferredWidth: height
                text:"列表"
            }
        }
    }



    // // 全屏状态属性
    // property bool isFullScreen: false

    // // 快捷键处理
    // Shortcut {
    //     sequence: "F11"
    //     onActivated: {
    //         toggleFullScreen()
    //     }
    // }

    // Shortcut {
    //     sequence: "Esc"
    //     onActivated: {
    //         if (isFullScreen) {
    //             toggleFullScreen()
    //         } else {
    //             mainWin.close()
    //         }
    //     }
    // }

    // // 全屏切换
    // function toggleFullScreen() {
    //     if (isFullScreen) {
    //         mainWin.showNormal()
    //         isFullScreen = false
    //     } else {
    //         mainWin.showFullScreen()
    //         isFullScreen = true
    //     }
    // }

    // ColumnLayout{
    //     anchors.fill: parent
    //     spacing: 0 //子项之间的间隙
    //     Rectangle{
    //         color: "#eef4f9"
    //         Layout.fillWidth: true
    //         Layout.preferredHeight: 32
    //         Layout.maximumHeight: 32
    //         Layout.minimumHeight: 32

    //         ComboBox{
    //             displayText: "AZPlayer"
    //             height: parent.height
    //             anchors.left: parent.left
    //             width: 80
    //             model: ["打开文件", "选项二", "选项三"]
    //             onActivated: function(idx) {
    //                 if (idx === 0) fileDialog.open()
    //             }
    //         }
    //     }

    //     Rectangle{
    //         color: "#171717"
    //         Layout.fillWidth: true
    //         Layout.preferredHeight: 2
    //     }

    //     // 视频显示区域
    //     Rectangle{
    //         Layout.fillWidth: true
    //         Layout.fillHeight: true
    //         color: "#101010"
    //         VideoWindow{
    //             anchors.fill: parent
    //             id:videoWindow
    //             objectName: "videoWindow"
    //             Component.onCompleted:{
    //                 MediaCtrl.setVideoWindow(videoWindow)
    //             }
    //         }
    //     }

    //     Rectangle{
    //         color: "#eef4f9"
    //         Layout.fillWidth: true
    //         Layout.preferredHeight: 32
    //         Layout.maximumHeight: 32
    //         Layout.minimumHeight: 32

    //         ComboBox{
    //             displayText: "AZPlayer"
    //             height: parent.height
    //             anchors.left: parent.left
    //             width: 80
    //             model: ["打开文件", "选项二", "选项三"]
    //             onActivated: function(idx) {
    //                 if (idx === 0) fileDialog.open()
    //             }
    //         }
    //     }
    // }



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
                textWrapper.text = selectedFile.toString().split(/[\\/]/).pop()
            }
        }
        onRejected: console.log("取消选择")
    }
}

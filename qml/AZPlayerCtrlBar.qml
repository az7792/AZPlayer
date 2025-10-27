// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import VideoWindow 1.0
Item{
    id:playerCtrlBar
    height: 40

    property VideoWindow videoWindow: null
    property AZDial angleDial : null

    property AZListView listView: null
    property Item splitView: null
    property int currentTime: 0
    signal openFileBtnClicked()

    AZButton{
        id:playBtn
        height: parent.height
        width: height
        anchors.left: parent.left
        iconWidth: 16
        iconHeight: 16
        iconSource: MediaCtrl.paused ? "qrc:/icon/play.png" : "qrc:/icon/pause.png"
        tooltipText: MediaCtrl.paused ? "播放" : "暂停"
        onLeftClicked: MediaCtrl.togglePaused();
    }
    AZButton{
        id:stopBtn
        height: parent.height
        width: height
        anchors.left: playBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/stop.png"
        tooltipText: "停止"
        onLeftClicked: listView.stopActiveItem()
    }
    AZButton{
        id:skipPrevBtn
        height: parent.height
        width: height
        anchors.left:stopBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/skip_previous.png"
        tooltipText: "L:快退，R:上一个"
        onLeftClicked: MediaCtrl.fastRewind()
        onRightClicked: listView.openPrev()
    }
    AZButton{
        id:skipNextBtn
        height: parent.height
        width: height
        anchors.left:skipPrevBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/skip_next.png"
        tooltipText: "L:快进，R:下一个"
        onLeftClicked: MediaCtrl.fastForward()
        onRightClicked: listView.openNext()
    }
    AZButton{
        id:openFileBtn
        height: parent.height
        width: height
        anchors.left:skipNextBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/open.png"
        tooltipText: "打开文件"
        onLeftClicked: playerCtrlBar.openFileBtnClicked()
    }

    Rectangle{
        height: parent.height
        anchors.left: openFileBtn.right
        anchors.right: playbackModeBtn.left
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        color: "#1c1c1c"
        clip: true

        function secondsToHMS(seconds) {
            var h = Math.floor(seconds / 3600);
            var m = Math.floor((seconds % 3600) / 60);
            var s = seconds % 60;

            // 保证两位数字显示
            var hh = h < 10 ? "0" + h : "" + h;
            var mm = m < 10 ? "0" + m : "" + m;
            var ss = s < 10 ? "0" + s : "" + s;

            return hh + ":" + mm + ":" + ss;
        }

        Text {
            id: mediaDurationText1
            color: "#ebebeb"
            text: parent.secondsToHMS(playerCtrlBar.currentTime)
            font.family: "Segoe UI"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            wrapMode: Text.NoWrap
            elide: Text.ElideNone

        }
        Text {
            id: mediaDurationText2
            color: "#747474"
            text: "  /  " + parent.secondsToHMS(MediaCtrl.duration)
            font.family: "Segoe UI"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: mediaDurationText1.right
            wrapMode: Text.NoWrap
            elide: Text.ElideNone
        }


        AZTextInput{
            id: inputAngle
            width: 70
            height: 20
            text: "0"
            labelText:"角度:"
            validator: DoubleValidator{
                bottom: 0.0
                top: 360.0
                decimals : 1
                notation: DoubleValidator.StandardNotation
            }
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: mediaDurationText2.right
            anchors.leftMargin: 10
            onEditingFinished: angleDial.value = Number(text) //angleDial会自己调用setAngle
            onWheelUp: angleDial.value = (Number(text) + 1) % 360     //angleDial会自己调用setAngle
            onWheelDown: angleDial.value = (Number(text) - 1 + 360) % 360   //angleDial会自己调用setAngle
            Connections {
                target: videoWindow
                function onVideoAngleChanged() {
                    inputAngle.text = videoWindow.angle().toFixed(1)
                }
            }
        }

        AZTextInput{
            id: inputScale
            width: 60
            height: 20
            text: "100"
            labelText:"缩放:"
            validator: IntValidator{
                bottom: 2
                top: 600
            }
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: inputAngle.right
            anchors.leftMargin: 10
            onEditingFinished: videoWindow.setScale(Number(text))
            onWheelUp: videoWindow.addScale()
            onWheelDown: videoWindow.subScale()
            Connections {
                target: videoWindow
                function onVideoScaleChanged() {
                    inputScale.text = videoWindow.scale()
                }
            }
        }

        AZTextInput{
            id: inputX
            width: 60
            height: 20
            text: "0"
            labelText:"X:"
            validator: DoubleValidator{
                bottom: -10000
                top: 10000
                decimals : 1
                notation: DoubleValidator.StandardNotation
            }
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: inputScale.right
            anchors.leftMargin: 10
            onEditingFinished: {videoWindow.setX(Number(text))
            console.log(text)}
            onWheelUp: videoWindow.addX()
            onWheelDown: videoWindow.subX()
            Connections {
                target: videoWindow
                function onVideoXChanged() {
                    inputX.text = videoWindow.tx().toFixed(1)
                }
            }
        }

        AZTextInput{
            id: inputY
            width: 60
            height: 20
            text: "0"
            labelText:"Y:"
            validator: DoubleValidator{
                bottom: -10000
                top: 10000
                decimals : 1
                notation: DoubleValidator.StandardNotation
            }
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: inputX.right
            anchors.leftMargin: 10
            onEditingFinished: videoWindow.setY(-Number(text)) // QML坐标的Y轴与OpenGL的Y轴是相反的
            onWheelUp: videoWindow.subY() // QML坐标的Y轴与OpenGL的Y轴是相反的
            onWheelDown: videoWindow.addY() // QML坐标的Y轴与OpenGL的Y轴是相反的
            Connections {
                target: videoWindow
                function onVideoYChanged() {
                    inputY.text = -videoWindow.ty().toFixed(1) // QML坐标的Y轴与OpenGL的Y轴是相反的
                }
            }
        }

        AZTextButton{
            id: resetBtn
            width: 60
            height: 25
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: inputY.right
            anchors.leftMargin: 10
            text: "恢复屏幕"
            onClicked: {
                videoWindow.setXY(0,0)
                //videoWindow.setAngle(0)
                angleDial.value = 0
                videoWindow.setScale(100)
            }
        }

    }

    AZButton{
        id:playbackModeBtn
        //0播完重播 1列表顺序 2列表随机
        property int mode: 0

        height: parent.height
        width: height
        anchors.right: fileListBtn.left
        anchors.rightMargin: 1
        iconHeight: 20
        iconWidth: 20
        iconSource: {
            if(mode === 0)
                return "qrc:/icon/repeat_one.png"
            else if(mode === 1)
                return "qrc:/icon/repeat.png"
            else
                return "qrc:/icon/shuffle.png"
        }
        tooltipText:mode === 0? "播完重播" : (mode === 1 ? "顺序播放" : "随机播放")
        onLeftClicked: {
            mode = (mode + 1) % 3
            if(mode === 0){
                MediaCtrl.setLoopOnEnd(true)
            } else {//1 or 2
                MediaCtrl.setLoopOnEnd(false)
            }
        }
        Connections {
            target: MediaCtrl
            function onPlayed() {
                if(playbackModeBtn.mode === 1){
                    listView.openNext()
                } else if(playbackModeBtn.mode === 2){
                    listView.openRandom()
                }
            }
        }
    }

    AZButton{
        id:fileListBtn
        height: parent.height
        width: height
        anchors.right: parent.right
        onLeftClicked: playerCtrlBar.splitView.toggleSidebar()
        iconHeight: 20
        iconWidth: 20
        iconSource: "qrc:/icon/list.png"
        tooltipText: "打开/关闭列表"
    }
}

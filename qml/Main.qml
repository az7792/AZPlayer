// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import VideoWindow 1.0
import QtQuick.Layouts
import AZPlayer 1.0
Window {
    id: mainWin
    width: 800
    minimumWidth: splitView.leftMinWidth + splitView.rightMinWidth + 20
    height: 600
    minimumHeight: 360
    visible: true
    title: qsTr("AZPlayer")
    color: "black"
    visibility: Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window| Qt.WindowSystemMenuHint|
           Qt.WindowMaximizeButtonHint | Qt.WindowMinimizeButtonHint

    property int resizeMargin: 8
    property bool forceShowBottomBar: false
    property bool forceShowTopBar: false

    //用于显示提示信息
    Component.onCompleted: {
        AZTooltip.mainWindow = mainWin
    }


    // 鼠标滚轮控制旋转角度
    WheelHandler {
        target: videoArea
        onWheel:function(wheel) {
            if (wheel.modifiers & Qt.ControlModifier) {// 按住 Ctrl
                if (wheel.angleDelta.y > 0) videoWindow.addScale()
                else if (wheel.angleDelta.y < 0) videoWindow.subScale()
            }else{
                if (wheel.angleDelta.y > 0) MediaCtrl.addVolum()
                else if (wheel.angleDelta.y < 0) MediaCtrl.subVolum()
            }
            wheel.accepted = true
        }
    }

    //用于调整窗口大小
    MouseArea {
        id: mainMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents:true

        // 定时器，隐藏鼠标
        Timer {
            id: hideTimer
            interval: 2000
            repeat: false
            onTriggered: {
                parent.cursorShape = Qt.BlankCursor
            }
        }

        //切换调整窗口大小时的鼠标样式
        function toggleCursorShape(x,y){
            // 改变鼠标样式
            if(mainWin.visibility !== Window.Windowed){
                if(cursorShape === Qt.SizeFDiagCursor || cursorShape === Qt.SizeBDiagCursor ||
                   cursorShape === Qt.SizeHorCursor || cursorShape === Qt.SizeVerCursor){
                   cursorShape = Qt.ArrowCursor
                }
                return
            }

            if (x <= resizeMargin && y <= resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 左上角
            else if (x >= parent.width - resizeMargin && y <= resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 右上角
            else if (x <= resizeMargin && y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 左下角
            else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 右下角
            else if (x <= resizeMargin || x >= parent.width - resizeMargin)
                cursorShape = Qt.SizeHorCursor     // 左右边
            else if (y <= resizeMargin || y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeVerCursor     // 上下边
            else if(cursorShape === Qt.SizeFDiagCursor || cursorShape === Qt.SizeBDiagCursor ||
                cursorShape === Qt.SizeHorCursor || cursorShape === Qt.SizeVerCursor){
                cursorShape = Qt.ArrowCursor
            }
        }

        //调整窗口大小
        function toggleSystemResize(x,y){
            if(mainWin.visibility !== Window.Windowed){
                return
            }

            let edge = 0
            if (x <= resizeMargin && y <= resizeMargin)
                edge = Qt.LeftEdge | Qt.TopEdge          // 左上角
            else if (x >= parent.width - resizeMargin && y <= resizeMargin)
                edge = Qt.RightEdge | Qt.TopEdge         // 右上角
            else if (x <= resizeMargin && y >= parent.height - resizeMargin)
                edge = Qt.LeftEdge | Qt.BottomEdge       // 左下角
            else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
                edge = Qt.RightEdge | Qt.BottomEdge      // 右下角
            else if (x <= resizeMargin)
                edge = Qt.LeftEdge                        // 左边
            else if (x >= parent.width - resizeMargin)
                edge = Qt.RightEdge                       // 右边
            else if (y <= resizeMargin)
                edge = Qt.TopEdge                         // 上边
            else if (y >= parent.height - resizeMargin)
                edge = Qt.BottomEdge                      // 下边

            if (edge !== 0) {
                mainWin.startSystemResize(edge)
            }
        }

        // 控制鼠标在视频区域内是否隐藏
        function toggleVideoMouseHidden(x,y){
            let topY = videoArea.y + (mainWin.visibility === Window.FullScreen ? topBar.height : 0)
            let bottomY = videoArea.y + videoArea.height - (mainWin.visibility === Window.FullScreen ? bottomBar.height : 0)
            let pd = 10;
            if(x>=videoArea.x + pd && x<=videoArea.x + videoArea.width -pd && y>=topY+pd && y <= bottomY-pd){
                if(cursorShape === Qt.BlankCursor){
                    cursorShape = Qt.ArrowCursor
                }
                hideTimer.restart()
            }else{
                hideTimer.stop()
            }
        }

        // 点击视频区域获取焦点
        function toggleVideoGetForce(x,y){
            let topY = videoArea.y + (mainWin.visibility === Window.FullScreen ? topBar.height : 0)
            let bottomY = videoArea.y + videoArea.height - (mainWin.visibility === Window.FullScreen ? bottomBar.height : 0)
            let pd = 10;
            if(x>=videoArea.x + pd && x<=videoArea.x + videoArea.width -pd && y>=topY+pd && y <= bottomY-pd){
                videoArea.forceActiveFocus()
            }
        }

        // 处理视频移动
        function toggleVideoMove(x,y){
            if(!pressed || !videoArea.spacePressed) return
            let deltaX = (x - videoArea.lastMouseX) * Screen.devicePixelRatio
            let deltaY = (y - videoArea.lastMouseY) * Screen.devicePixelRatio

            videoWindow.addXY(deltaX, deltaY)

            videoArea.lastMouseX = x
            videoArea.lastMouseY = y
        }

        // 处理移动视频时的指针样式
        function toggleVideoMoveCursorShape(){
            if(!videoArea.spacePressed) {
                if(cursorShape === Qt.ClosedHandCursor || cursorShape === Qt.OpenHandCursor)
                    cursorShape = Qt.ArrowCursor
                return
            }

            cursorShape = pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        }

        //全屏时呼出topBar和bottomBar
        function toggleBarVisible(x,y){
            if(mainWin.visibility !== Window.FullScreen){
                forceShowTopBar = forceShowBottomBar = false
                return
            }
            forceShowTopBar = (y <= topBar.height && (!fileListBar.visible || x <= fileListBar.x))
            forceShowBottomBar = bottomBar.playerSettingOpened || (y >= mainWin.height - bottomBar.height && x <= bottomBar.width)
        }

        onPositionChanged:function(mouse) {
            toggleCursorShape(mouse.x,mouse.y)      // 切换调整窗口大小时的鼠标样式
            toggleBarVisible(mouse.x,mouse.y)       // 全屏时呼出topBar和bottomBar
            toggleVideoMouseHidden(mouse.x,mouse.y) // 控制鼠标在视频区域内是否隐藏
            toggleVideoMove(mouse.x,mouse.y)        // 处理视频移动
        }

        onPressed: function(mouse) {
            toggleSystemResize(mouse.x,mouse.y)     // 调整窗口大小
            toggleVideoGetForce(mouse.x,mouse.y)    // 点击视频区域使其获取焦点
            toggleVideoMoveCursorShape()            // 处理移动视频时的指针样式

            videoArea.lastMouseX = mouse.x
            videoArea.lastMouseY = mouse.y
        }

        onReleased: function(mouse) {
            toggleVideoMoveCursorShape()            // 处理移动视频时的指针样式
        }
    }

    AZTopBar{
        id: topBar
        mainWindow: mainWin
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        visible: mainWin.visibility !== Window.FullScreen || forceShowTopBar
        height: 32
        color: "black"
        z:1
    }

    Item{
        id: splitView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: mainWin.visibility === Window.FullScreen ? parent.top : topBar.bottom
        anchors.bottomMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.topMargin: mainWin.visibility === Window.FullScreen ? 0 : 1
        z:0

        property int leftMinWidth: 360      // 左侧最小宽度
        property int rightMinWidth: 140     // 右侧最小宽度
        property bool isLeftExpanded: (mainWin.visibility === Window.FullScreen || !showed) ? true : false
        property bool showed: false
        property int _rightRectWidth: 200

        function toggleSidebar(){
            showed = !showed
        }

        //视频显示区域
        Item{
            id: videoArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: splitView.isLeftExpanded ? parent.right : splitter.left
            anchors.bottom: mainWin.visibility === Window.FullScreen ? parent.bottom : bottomBar.top
            anchors.bottomMargin: mainWin.visibility === Window.FullScreen ? 0 : 1
            z:0

            VideoWindow{
                anchors.fill: parent
                id:videoWindow
                objectName: "videoWindow"
                Component.onCompleted:{
                    MediaCtrl.setVideoWindow(videoWindow)
                }
            }

            property bool spacePressed: false
            property real lastMouseX: 0
            property real lastMouseY: 0

            onSpacePressedChanged: mainMouseArea.toggleVideoMoveCursorShape()

            Keys.forwardTo : [angleDial]

            Keys.onPressed: function(event){
                if(event.isAutoRepeat) return
                if (event.key === Qt.Key_Space) spacePressed = true

                if (event.modifiers & Qt.AltModifier){
                    angleDial.visible = true
                }
            }

            Keys.onReleased:function(event) {
                if(event.isAutoRepeat) return
                if (event.key === Qt.Key_Space) spacePressed = false

                if (event.key === Qt.Key_Alt) {
                    angleDial.visible = false
                }
            }

            AZDial{
                id: angleDial
                anchors.centerIn: parent
                height: 150
                width: 150
                visible: false
                onValueChanged: videoWindow.setAngle(value)
            }
        }

        //底部控制栏
        AZBottomBar{
            id:bottomBar
            anchors.left: parent.left
            anchors.right: splitView.showed ? splitter.left : parent.right
            anchors.bottom: parent.bottom
            visible: mainWin.visibility !== Window.FullScreen || forceShowBottomBar
            z:1
            listView: fileListBar.fileTab
            splitView: splitView
            videoWindow: videoWindow
            angleDial: angleDial
            fileDialog: fileDialog
        }

        Rectangle {
            id: splitter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: splitView.showed
            width: 4
            color: "black"

            MouseArea {
                id: tmp
                anchors.fill: parent
                drag.target: splitter
                drag.axis: Drag.XAxis
                drag.minimumX: splitView.leftMinWidth
                drag.maximumX: splitView.width - splitView.rightMinWidth - width
                cursorShape: Qt.SplitHCursor
                drag.smoothed:false

                // 拖动时保存右侧宽度
                onPositionChanged: {
                    if (drag.active) {
                        splitView._rightRectWidth = splitView.width - (splitter.x + splitter.width);
                        // 确保右侧宽度不小于最小值
                        splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);
                    }
                }
            }
        }

        AZFileSideBar {
            id:fileListBar
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: parent.top
            anchors.left: splitter.right
            visible: splitView.showed
            spacing: 1
            fileDialog:fileDialog
            topBar: topBar
            videoWindow: videoWindow
        }

        onWidthChanged: {
            // 确保右侧宽度不小于最小值
            splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);

            // 检查是否需要调整右侧宽度以适应新的窗口大小
            var maxPossibleRightWidth = splitView.width - splitView.leftMinWidth - splitter.width;
            if (splitView._rightRectWidth > maxPossibleRightWidth) {
                splitView._rightRectWidth = maxPossibleRightWidth;
            }

            // 更新分割条位置
            splitter.x = splitView.width - splitView._rightRectWidth - splitter.width;
        }
    }

    AZFileDialog{
        id:fileDialog
    }
}

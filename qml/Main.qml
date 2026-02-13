// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import VideoWindow 1.0
import QtQuick.Layouts
import AZPlayer 1.0

AZWindow {
    id: mainWin
    width: 800
    minimumWidth: 480
    height: 600
    minimumHeight: 380
    visible: true
    title: qsTr("AZPlayer")
    backgroundColor: "black"

    // 全局提示工具
    Component.onCompleted: {
        AZTooltip.mainWindow = mainWin
    }

    // 启动参数
    function onStartupFiles(files) {
        AZPlayerState.mediafileDialog.onStartupFiles(files)
    }

    // 鼠标滚轮控制旋转角度或音量
    WheelHandler {
        target: videoArea
        onWheel:function(wheel) {
            if (wheel.modifiers & Qt.ControlModifier) {// 按住 Ctrl
                if (wheel.angleDelta.y > 0) AZPlayerState.videoScale += 1
                else if (wheel.angleDelta.y < 0) AZPlayerState.videoScale -= 1
            }else{
                if (wheel.angleDelta.y > 0) MediaCtrl.addVolum()
                else if (wheel.angleDelta.y < 0) MediaCtrl.subVolum()
            }
            wheel.accepted = true
        }
    }

    Item {
        id: keyboardController

        // 播放/暂停（空格）
        Shortcut {
            sequence: "Space"
            autoRepeat: false
            onActivated: MediaCtrl.togglePaused()
        }

        // 后退 (左方向键)
        Shortcut {
            sequence: "Left"
            autoRepeat: false
            onActivated: MediaCtrl.fastRewind()
        }

        // 前进 (右方向键)
        Shortcut {
            sequence: "Right"
            autoRepeat: false
            onActivated: MediaCtrl.fastForward()
        }

        // 音量加 (上方向键)
        Shortcut {
            sequence: "Up"
            onActivated: MediaCtrl.addVolum()
        }

        // 音量减 (下方向键)
        Shortcut {
            sequence: "Down"
            onActivated: MediaCtrl.subVolum()
        }
    }

    // ====topBar、 bottomBar、sideBar区域是否包含鼠标====
    property bool containsTopBar: false
    property bool containsBottomBar: false
    property bool containsSideBar: false

    // ====topBar、 bottomBar、sideBar是否需要显示====
    property bool showTopBar: {
        if(!mainWin.videoFull) return true
        return containsTopBar | topBar.isActive
    }

    property bool showBottomBar: {
        if(!mainWin.videoFull) return true
        return containsBottomBar | bottomBar.isActive
    }

    property bool showSideBar: {
        if(!sideBar.canShow) return false
        if(!mainWin.videoFull) return true
        return containsSideBar | sideBar.isActive
    }

    // ====计算topBar、 bottomBar、sideBar区域是否包含鼠标====
    MouseArea {
        id: videoInteractionArea
        anchors.fill: parent
        anchors.margins: mainWin.resizeBorderWidth
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        cursorShape: {
            if(pressedButtons & Qt.MiddleButton) return Qt.ClosedHandCursor

            return hideCursor ? Qt.BlankCursor : Qt.ArrowCursor
        }

        property bool hideCursor: false
        property real lastMouseX: 0
        property real lastMouseY: 0

        // 定时器，隐藏鼠标
        Timer {
            id: hideTimer
            interval: 2000
            repeat: false
            onTriggered: {
                videoInteractionArea.hideCursor = true
            }
        }

        // 点击视频区域获取焦点
        function toggleVideoGetForce(x,y) {
            videoArea.forceActiveFocus()
        }

        //全屏时呼出topBar 或 bottomBar 或 sideBar
        function toggleBarContains(x,y){
            if(pressedButtons & Qt.MiddleButton){
                containsTopBar = containsBottomBar = containsSideBar = false
                return
            }

            let pdTop = 20
            let pdBottom = 25
            let pdSide = 40

            if(sideBar.canShow){
                if(containsSideBar) {
                    containsSideBar = x >= sideBar.x - pdSide
                }else{
                    containsSideBar = x >= sideBar.x + Math.max(-pdSide ,sideBar.width - 100) && (y >= Screen.height * 0.2 && y <= Screen.height * 0.8)
                }
            }else{
                containsSideBar = false
            }

            if(containsSideBar) {
                containsTopBar = containsBottomBar = false
                return
            }

            containsTopBar = y <= topBar.y + pdTop + topBar.height
            if(containsTopBar) {
                containsBottomBar = false
                return
            }

            containsBottomBar = y >= bottomBar.y - pdBottom
        }

        // 处理视频移动
        function toggleVideoMove(x,y){
            if(!(pressedButtons & Qt.MiddleButton)) return
            let deltaX = (x - videoInteractionArea.lastMouseX) * Screen.devicePixelRatio
            let deltaY = (y - videoInteractionArea.lastMouseY) * Screen.devicePixelRatio

            AZPlayerState.videoX += deltaX
            AZPlayerState.videoY += deltaY

            videoInteractionArea.lastMouseX = x
            videoInteractionArea.lastMouseY = y
        }

        // 控制鼠标在视频区域内是否隐藏
        function toggleVideoMouseHidden(x,y){
            hideCursor = false
            if(mainWin.containsTopBar || mainWin.containsBottomBar || mainWin.containsSideBar){
                hideTimer.stop()
                return
            }

            hideTimer.restart()
        }

        onPositionChanged:function(mouse) {
            toggleBarContains(mouse.x, mouse.y)
            toggleVideoMove(mouse.x, mouse.y)
            toggleVideoMouseHidden(mouse.x, mouse.y)
        }

        onPressed: function(mouse) {
            toggleVideoGetForce(mouse.x, mouse.y)

            videoInteractionArea.lastMouseX = mouse.x
            videoInteractionArea.lastMouseY = mouse.y
        }

        onReleased: function(mouse) {
            toggleBarContains(mouse.x, mouse.y)
        }
    }


    // 视频
    AZVideoArea{
        // Rectangle{
        //     id: debugDorder
        //     anchors.fill: parent
        //     border.color: "green"
        //     border.width: 1
        //     color: "transparent"
        // }

        id: videoArea

        anchors.left: parent.left
        anchors.leftMargin: mainWin.resizeBorderWidth

        anchors.top: mainWin.videoFull ? parent.top : topBar.bottom
        anchors.topMargin: mainWin.videoFull ? mainWin.resizeBorderWidth : 1

        anchors.bottom: mainWin.videoFull ? parent.bottom : bottomBar.top
        anchors.bottomMargin: mainWin.videoFull ? mainWin.resizeBorderWidth : 1

        anchors.right: mainWin.videoFull ? parent.right : (showSideBar ? splitter.left : parent.right)
        anchors.rightMargin: mainWin.videoFull ? mainWin.resizeBorderWidth : (showSideBar ? 1 : mainWin.resizeBorderWidth)


        Keys.forwardTo : [angleDial]

        Keys.onPressed: function(event) {
            if (event.isAutoRepeat) return
            if (event.key === Qt.Key_Alt) {
                angleDial.visible = true
                event.accepted = true
            }
        }

        Keys.onReleased: function(event) {
            if (event.key === Qt.Key_Alt) {
                angleDial.visible = false
                event.accepted = true
            }
        }

        AZDial{
            id: angleDial
            anchors.centerIn: parent
            height: 150
            width: 150
            visible: false
            onValueChanged: AZPlayerState.videoAngle = value
        }
    }

    // TopBar
    AZTopBar {
        id: topBar
        visible: showTopBar

        height: 32
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: mainWin.resizeBorderWidth

        color: "black"

        // 必须的入参
        mainWindow: mainWin
    }

    // BottomBar
    AZBottomBar {
        id: bottomBar
        visible: showBottomBar

        anchors.bottom: parent.bottom
        anchors.bottomMargin: mainWin.resizeBorderWidth
        anchors.left: parent.left
        anchors.leftMargin: mainWin.resizeBorderWidth
        anchors.right: videoArea.right

        fileListView: sideBar.fileListView

        onRequestToggleSideBar: {
            sideBar.toggleSidebar()
        }
    }

    //Splitter
    Rectangle {
        id: splitter
        visible: showSideBar
        color: "black"
        anchors.top: mainWin.videoFull ? parent.top : topBar.bottom
        anchors.topMargin: mainWin.videoFull ? mainWin.resizeBorderWidth : 1
        anchors.bottom: parent.bottom
        anchors.bottomMargin: mainWin.resizeBorderWidth
        width: 4
        readonly property int leftMinWidth: mainWin.width * 0.4          // 左侧最小宽度
        readonly property int rightMinWidth: mainWin.minimumWidth * 0.4  // 右侧最小宽度

        property int _rightRectWidth: 160

        Component.onCompleted: {
            // 只在初始化时计算一次，不建立动态绑定
            _rightRectWidth = rightMinWidth
            x = mainWin.width - _rightRectWidth - width
        }

        MouseArea {
            id: splitterMouseArea
            anchors.fill: parent
            drag.target: splitter
            drag.axis: Drag.XAxis
            drag.minimumX: splitter.leftMinWidth
            drag.maximumX: mainWin.width - splitter.rightMinWidth - width
            cursorShape: Qt.SplitHCursor
            drag.smoothed: false
            preventStealing: true

            // 拖动时保存右侧宽度
            onPositionChanged: {
                if (drag.active) {
                    splitter._rightRectWidth = mainWin.width - (splitter.x + splitter.width);
                    // 确保右侧宽度不小于最小值
                    splitter._rightRectWidth = Math.max(splitter.rightMinWidth, splitter._rightRectWidth);
                }
            }
        }

        Connections{
            target: mainWin
            function onWidthChanged(){
                // 确保右侧宽度不小于最小值
                splitter._rightRectWidth = Math.max(splitter.rightMinWidth, splitter._rightRectWidth);

                // 检查是否需要调整右侧宽度以适应新的窗口大小
                var maxPossibleRightWidth = mainWin.width - splitter.leftMinWidth - splitter.width; // 右边最大宽度
                if (splitter._rightRectWidth > maxPossibleRightWidth) {
                    splitter._rightRectWidth = maxPossibleRightWidth;
                }

                // 更新分割条位置
                splitter.x = mainWin.width - splitter._rightRectWidth - splitter.width;
            }
        }
    }

    // SideBar
    AZSideBar {
        id: sideBar
        visible: showSideBar
        color: "black"

        property bool canShow: false
        function toggleSidebar(){
            canShow = !canShow
            if(mainWin.windowState === mainWin.winNormal){
                let targetAddWidth = splitter._rightRectWidth - mainWin.resizeBorderWidth + splitter.width + 1 // +1是因为视频与splitter有1px的空隙                
                let tmpWidth = mainWin.width + (canShow ? targetAddWidth : -targetAddWidth)
                tmpWidth = Math.max(mainWin.minimumWidth, Math.min(Screen.desktopAvailableWidth,tmpWidth))
                let offset = tmpWidth + mainWin.x - Screen.desktopAvailableWidth
                if(offset > 0){
                    mainWin.x -= offset;
                }
                mainWin.width = tmpWidth
            }
        }

        anchors.top: mainWin.videoFull ? parent.top : topBar.bottom
        anchors.topMargin: mainWin.videoFull ? mainWin.resizeBorderWidth : 1
        anchors.right: parent.right
        anchors.rightMargin: mainWin.resizeBorderWidth
        anchors.bottom: parent.bottom
        anchors.bottomMargin: mainWin.resizeBorderWidth
        anchors.left: splitter.right
    }
}


// Window {

//     // 鼠标滚轮控制旋转角度
//     WheelHandler {
//         target: videoArea
//         onWheel:function(wheel) {
//             if (wheel.modifiers & Qt.ControlModifier) {// 按住 Ctrl
//                 if (wheel.angleDelta.y > 0) videoWindow.addScale()
//                 else if (wheel.angleDelta.y < 0) videoWindow.subScale()
//             }else{
//                 if (wheel.angleDelta.y > 0) MediaCtrl.addVolum()
//                 else if (wheel.angleDelta.y < 0) MediaCtrl.subVolum()
//             }
//             wheel.accepted = true
//         }
//     }

//     //用于调整窗口大小
//     MouseArea {
//         id: mainMouseArea
//         anchors.fill: parent
//         hoverEnabled: true
//         propagateComposedEvents:true

//         // 定时器，隐藏鼠标
//         Timer {
//             id: hideTimer
//             interval: 2000
//             repeat: false
//             onTriggered: {
//                 parent.cursorShape = Qt.BlankCursor
//             }
//         }

//         //切换调整窗口大小时的鼠标样式
//         function toggleCursorShape(x,y){
//             // 改变鼠标样式
//             if(mainWin.visibility !== Window.Windowed){
//                 if(cursorShape === Qt.SizeFDiagCursor || cursorShape === Qt.SizeBDiagCursor ||
//                    cursorShape === Qt.SizeHorCursor || cursorShape === Qt.SizeVerCursor){
//                    cursorShape = Qt.ArrowCursor
//                 }
//                 return
//             }

//             if (x <= resizeMargin && y <= resizeMargin)
//                 cursorShape = Qt.SizeFDiagCursor   // 左上角
//             else if (x >= parent.width - resizeMargin && y <= resizeMargin)
//                 cursorShape = Qt.SizeBDiagCursor   // 右上角
//             else if (x <= resizeMargin && y >= parent.height - resizeMargin)
//                 cursorShape = Qt.SizeBDiagCursor   // 左下角
//             else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
//                 cursorShape = Qt.SizeFDiagCursor   // 右下角
//             else if (x <= resizeMargin || x >= parent.width - resizeMargin)
//                 cursorShape = Qt.SizeHorCursor     // 左右边
//             else if (y <= resizeMargin || y >= parent.height - resizeMargin)
//                 cursorShape = Qt.SizeVerCursor     // 上下边
//             else if(cursorShape === Qt.SizeFDiagCursor || cursorShape === Qt.SizeBDiagCursor ||
//                 cursorShape === Qt.SizeHorCursor || cursorShape === Qt.SizeVerCursor){
//                 cursorShape = Qt.ArrowCursor
//             }
//         }

//         //调整窗口大小
//         function toggleSystemResize(x,y){
//             if(mainWin.visibility !== Window.Windowed){
//                 return
//             }

//             let edge = 0
//             if (x <= resizeMargin && y <= resizeMargin)
//                 edge = Qt.LeftEdge | Qt.TopEdge          // 左上角
//             else if (x >= parent.width - resizeMargin && y <= resizeMargin)
//                 edge = Qt.RightEdge | Qt.TopEdge         // 右上角
//             else if (x <= resizeMargin && y >= parent.height - resizeMargin)
//                 edge = Qt.LeftEdge | Qt.BottomEdge       // 左下角
//             else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
//                 edge = Qt.RightEdge | Qt.BottomEdge      // 右下角
//             else if (x <= resizeMargin)
//                 edge = Qt.LeftEdge                        // 左边
//             else if (x >= parent.width - resizeMargin)
//                 edge = Qt.RightEdge                       // 右边
//             else if (y <= resizeMargin)
//                 edge = Qt.TopEdge                         // 上边
//             else if (y >= parent.height - resizeMargin)
//                 edge = Qt.BottomEdge                      // 下边

//             if (edge !== 0) {
//                 mainWin.startSystemResize(edge)
//             }
//         }

//         // 控制鼠标在视频区域内是否隐藏
//         function toggleVideoMouseHidden(x,y){
//             let topY = videoArea.y + (mainWin.visibility === Window.FullScreen ? topBar.height : 0)
//             let bottomY = videoArea.y + videoArea.height - (mainWin.visibility === Window.FullScreen ? bottomBar.height : 0)
//             let pd = 10;
//             if(x>=videoArea.x + pd && x<=videoArea.x + videoArea.width -pd && y>=topY+pd && y <= bottomY-pd){
//                 if(cursorShape === Qt.BlankCursor){
//                     cursorShape = Qt.ArrowCursor
//                 }
//                 hideTimer.restart()
//             }else{
//                 hideTimer.stop()
//             }
//         }

//         // 点击视频区域获取焦点
//         function toggleVideoGetForce(x,y){
//             let topY = videoArea.y + (mainWin.visibility === Window.FullScreen ? topBar.height : 0)
//             let bottomY = videoArea.y + videoArea.height - (mainWin.visibility === Window.FullScreen ? bottomBar.height : 0)
//             let pd = 10;
//             if(x>=videoArea.x + pd && x<=videoArea.x + videoArea.width -pd && y>=topY+pd && y <= bottomY-pd){
//                 videoArea.forceActiveFocus()
//             }
//         }

//         // 处理视频移动
//         function toggleVideoMove(x,y){
//             if(!pressed || !videoArea.spacePressed) return
//             let deltaX = (x - videoArea.lastMouseX) * Screen.devicePixelRatio
//             let deltaY = (y - videoArea.lastMouseY) * Screen.devicePixelRatio

//             videoWindow.addXY(deltaX, deltaY)

//             videoArea.lastMouseX = x
//             videoArea.lastMouseY = y
//         }

//         // 处理移动视频时的指针样式
//         function toggleVideoMoveCursorShape(){
//             if(!videoArea.spacePressed) {
//                 if(cursorShape === Qt.ClosedHandCursor || cursorShape === Qt.OpenHandCursor)
//                     cursorShape = Qt.ArrowCursor
//                 return
//             }

//             cursorShape = pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
//         }

//         //全屏时呼出topBar和bottomBar
//         function toggleBarVisible(x,y){
//             if(mainWin.visibility !== Window.FullScreen){
//                 forceShowTopBar = forceShowBottomBar = false
//                 return
//             }
//             forceShowTopBar = (y <= topBar.height && (!fileListBar.visible || x <= fileListBar.x))
//             forceShowBottomBar = bottomBar.playerSettingOpened || (y >= mainWin.height - bottomBar.height && x <= bottomBar.width)
//         }

//         onPositionChanged:function(mouse) {
//             toggleCursorShape(mouse.x,mouse.y)      // 切换调整窗口大小时的鼠标样式
//             toggleBarVisible(mouse.x,mouse.y)       // 全屏时呼出topBar和bottomBar
//             toggleVideoMouseHidden(mouse.x,mouse.y) // 控制鼠标在视频区域内是否隐藏
//             toggleVideoMove(mouse.x,mouse.y)        // 处理视频移动
//         }

//         onPressed: function(mouse) {
//             toggleSystemResize(mouse.x,mouse.y)     // 调整窗口大小
//             toggleVideoGetForce(mouse.x,mouse.y)    // 点击视频区域使其获取焦点
//             toggleVideoMoveCursorShape()            // 处理移动视频时的指针样式

//             videoArea.lastMouseX = mouse.x
//             videoArea.lastMouseY = mouse.y
//         }

//         onReleased: function(mouse) {
//             toggleVideoMoveCursorShape()            // 处理移动视频时的指针样式
//         }
//     }

//     AZTopBar{
//         id: topBar
//         mainWindow: mainWin
//         anchors.top: parent.top
//         anchors.left: parent.left
//         anchors.right: parent.right
//         anchors.topMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         visible: mainWin.visibility !== Window.FullScreen || forceShowTopBar
//         height: 32
//         color: "black"
//         z:1
//     }

//     Item{
//         id: splitView
//         anchors.left: parent.left
//         anchors.right: parent.right
//         anchors.bottom: parent.bottom
//         anchors.top: mainWin.visibility === Window.FullScreen ? parent.top : topBar.bottom
//         anchors.bottomMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
//         anchors.topMargin: mainWin.visibility === Window.FullScreen ? 0 : 1
//         z:0

//         property int leftMinWidth: 360      // 左侧最小宽度
//         property int rightMinWidth: 140     // 右侧最小宽度
//         property bool isLeftExpanded: (mainWin.visibility === Window.FullScreen || !showed) ? true : false
//         property bool showed: false
//         property int _rightRectWidth: 200

//         function toggleSidebar(){
//             showed = !showed
//         }

//         //视频显示区域
//         Item{
//             id: videoArea
//             anchors.top: parent.top
//             anchors.left: parent.left
//             anchors.right: splitView.isLeftExpanded ? parent.right : splitter.left
//             anchors.bottom: mainWin.visibility === Window.FullScreen ? parent.bottom : bottomBar.top
//             anchors.bottomMargin: mainWin.visibility === Window.FullScreen ? 0 : 1
//             z:0

//             VideoWindow{
//                 anchors.fill: parent
//                 id:videoWindow
//                 objectName: "videoWindow"
//                 Component.onCompleted:{
//                     MediaCtrl.setVideoWindow(videoWindow)
//                 }
//             }

//             property bool spacePressed: false
//             property real lastMouseX: 0
//             property real lastMouseY: 0

//             onSpacePressedChanged: mainMouseArea.toggleVideoMoveCursorShape()

//             Keys.forwardTo : [angleDial]

//             Keys.onPressed: function(event){
//                 if(event.isAutoRepeat) return
//                 if (event.key === Qt.Key_Space) spacePressed = true

//                 if (event.modifiers & Qt.AltModifier){
//                     angleDial.visible = true
//                 }
//             }

//             Keys.onReleased:function(event) {
//                 if(event.isAutoRepeat) return
//                 if (event.key === Qt.Key_Space) spacePressed = false

//                 if (event.key === Qt.Key_Alt) {
//                     angleDial.visible = false
//                 }
//             }

//             AZDial{
//                 id: angleDial
//                 anchors.centerIn: parent
//                 height: 150
//                 width: 150
//                 visible: false
//                 onValueChanged: videoWindow.setAngle(value)
//             }
//         }

//         //底部控制栏
//         AZBottomBar{
//             id:bottomBar
//             anchors.left: parent.left
//             anchors.right: splitView.showed ? splitter.left : parent.right
//             anchors.bottom: parent.bottom
//             visible: mainWin.visibility !== Window.FullScreen || forceShowBottomBar
//             z:1
//             listView: fileListBar.fileTab
//             splitView: splitView
//             videoWindow: videoWindow
//             angleDial: angleDial
//             fileDialog: fileDialog
//         }

//         Rectangle {
//             id: splitter
//             anchors.top: parent.top
//             anchors.bottom: parent.bottom
//             visible: splitView.showed
//             width: 4
//             color: "black"

//             MouseArea {
//                 id: tmp
//                 anchors.fill: parent
//                 drag.target: splitter
//                 drag.axis: Drag.XAxis
//                 drag.minimumX: splitView.leftMinWidth
//                 drag.maximumX: splitView.width - splitView.rightMinWidth - width
//                 cursorShape: Qt.SplitHCursor
//                 drag.smoothed:false

//                 // 拖动时保存右侧宽度
//                 onPositionChanged: {
//                     if (drag.active) {
//                         splitView._rightRectWidth = splitView.width - (splitter.x + splitter.width);
//                         // 确保右侧宽度不小于最小值
//                         splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);
//                     }
//                 }
//             }
//         }

//         AZFileSideBar {
//             id:fileListBar
//             anchors.right: parent.right
//             anchors.bottom: parent.bottom
//             anchors.top: parent.top
//             anchors.left: splitter.right
//             visible: splitView.showed
//             spacing: 1
//             fileDialog:fileDialog
//             topBar: topBar
//             videoWindow: videoWindow
//         }

//         onWidthChanged: {
//             // 确保右侧宽度不小于最小值
//             splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);

//             // 检查是否需要调整右侧宽度以适应新的窗口大小
//             var maxPossibleRightWidth = splitView.width - splitView.leftMinWidth - splitter.width;
//             if (splitView._rightRectWidth > maxPossibleRightWidth) {
//                 splitView._rightRectWidth = maxPossibleRightWidth;
//             }

//             // 更新分割条位置
//             splitter.x = splitView.width - splitView._rightRectWidth - splitter.width;
//         }
//     }

//     AZFileDialog{
//         id:fileDialog
//     }
// }

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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

        // 播放信息（Tab键）
        Shortcut {
            sequence: "Tab"
            onActivated: {
                if(playbackStatsArea.visible) playbackStatsArea.hideStats()
                else playbackStatsArea.showStats()

                playbackStatsArea.visible = !playbackStatsArea.visible
            }
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

        AZPlaybackStatsArea {
            id: playbackStatsArea
            visible: false
            font.family: "Consolas"
            font.pixelSize: Math.max(12, parent.width * Screen.devicePixelRatio * 0.02)
            anchors.fill: parent
        }
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

        // 点击视频区域，角度转盘获取焦点
        function toggleVideoDialGetForce(x,y) {
            videoAngleDialArea.forceActiveFocus()
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
            toggleVideoDialGetForce(mouse.x, mouse.y)

            videoInteractionArea.lastMouseX = mouse.x
            videoInteractionArea.lastMouseY = mouse.y
        }

        onReleased: function(mouse) {
            toggleBarContains(mouse.x, mouse.y)
        }        
    }

    AZVideoAngleDialArea {
        id: videoAngleDialArea
        anchors.fill: videoArea
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

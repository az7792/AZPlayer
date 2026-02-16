// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import AZPlayer 1.0

Rectangle{
    id: root

    // 外部传入的窗口
    required property AZWindow targetWindow

    // 按钮统一样式
    property int iconWidth: 16
    property int iconHeight: 16

    Connections {
        target: targetWindow
        function onWindowStateChanged() {
            let tmp = targetWindow.windowState

            if(tmp === targetWindow.winMaximized){
                fullScreenBtn.tooltipText = "全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen.png"

                maximizeBtn.tooltipText = "恢复"
                maximizeBtn.iconSource = "qrc:/icon/restore.png"
            }else if(tmp === targetWindow.winFullScreen){
                fullScreenBtn.tooltipText = "退出全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen_exit.png"

                maximizeBtn.tooltipText = "恢复"
                maximizeBtn.iconSource = "qrc:/icon/restore.png"
            }else if(tmp === targetWindow.winNormal){
                maximizeBtn.tooltipText = "最大化"
                maximizeBtn.iconSource = "qrc:/icon/maximize.png"

                fullScreenBtn.tooltipText = "全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen.png"
            }
        }
    }

    RowLayout {
        id: controlBar
        anchors.fill: parent
        spacing: 0

        function toggleFullScreen(){
            let nowWinState = targetWindow.windowState
            if(nowWinState === targetWindow.winFullScreen){
                targetWindow.restore()
            }else{
                targetWindow.fullscreen()
            }
        }

        function toggleMaximized(){
            let nowWinState = targetWindow.windowState
            if(nowWinState === targetWindow.winNormal){
                targetWindow.maximize()
            }else if(nowWinState === targetWindow.winFullScreen || nowWinState == Window.Maximized){
                targetWindow.restore()
            }
        }

        AZButton {
            id: minimizeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/minimize.png"
            tooltipText: "最小化"
            onLeftClicked: targetWindow.minimize()
        }

        AZButton {
            id: maximizeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/maximize.png"
            tooltipText: "最大化"
            onLeftClicked: controlBar.toggleMaximized()
        }

        AZButton {
            id: fullScreenBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/fullscreen.png"
            tooltipText: "全屏"
            onLeftClicked: controlBar.toggleFullScreen()
        }

        AZButton {
            id: closeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/close.png"
            tooltipText: "关闭"
            onLeftClicked: targetWindow.close()
        }
    }
}

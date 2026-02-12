// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: tooltipWindow
    visible: false
    flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    color: "transparent"

    required property Window mainWindow

    Connections{
        target: mainWindow
        function onWidthChanged(){
            if(tooltipWindow.visible) tooltipWindow.hide()
        }
    }

    Rectangle {
        id: box
        color: "#f9f9f9"
        radius: 3
        border.color: "#d0d0d0"
        border.width: 1

        Text {
            id: label
            anchors.centerIn: parent
            color: "black"
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
        }

        width: label.width + 10
        height: label.height + 6
    }

    width: box.width
    height: box.height

    function show(msg, px, py) {
        px += mainWindow.x
        py += mainWindow.y
        label.text = msg
        let offsetX = px + 8 + box.width - Screen.width
        let offsetY = py + 16 + box.height - Screen.height
        y = py + 16 - (offsetY > 0 ? offsetY : 0)
        x = (offsetX > 0 && offsetY > 0) ? (px - box.width - 4)
                : (px + 8 - (offsetX > 0 ? offsetX : 0))
        width = box.width
        height = box.height
        visible = true
        raise() // 保证显示在最上层
    }

    function hide() {
        visible = false
    }
}

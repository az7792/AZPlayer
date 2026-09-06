// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15

// 画面左上角的文本提示(OSD)
Item {
    id: root

    // 根据画面大小计算的字号
    readonly property int fontSize: Math.max(16, parent.width * 0.015)

    visible: false

    Text {
        id: label
        anchors.top: root.top
        anchors.left: root.left
        anchors.margins: 4
        color: "#ebebeb"
        font.pixelSize: root.fontSize
        style: Text.Outline
        styleColor: "black"
    }

    Timer {
        id: hideTimer
        interval: root.defaultDuration
        repeat: false
        onTriggered: root.hide()
    }

    function show(msg, durationMs) {
        label.text = msg
        hideTimer.interval = durationMs > 0 ? durationMs : 1000
        visible = true
        hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        visible = false
    }
}

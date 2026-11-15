// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15

// 画面左上角的文本提示(OSD)，最多两行
Item {
    id: root

    // 根据画面大小计算的字号
    readonly property int fontSize: parent ? Math.max(16, parent.width * 0.015) : 16

    // 普通文本的默认颜色
    readonly property color defaultColor: "#ebebeb"

    // 普通文本内容与显示状态
    property string normalText: ""
    property bool normalVisible: false

    // 常亮文本内容与颜色；内容为空表示当前没有常亮文本
    property string persistentText: ""
    property color persistentColor: defaultColor

    visible: normalVisible || persistentText !== ""

    // 第一行：只有一种文本时显示该文本；两种文本同时存在时显示普通文本
    Text {
        id: label
        anchors.top: root.top
        anchors.left: root.left
        anchors.margins: 4
        text: root.normalVisible ? root.normalText : root.persistentText
        color: root.normalVisible ? root.defaultColor : root.persistentColor
        font.pixelSize: root.fontSize
        style: Text.Outline
        styleColor: "black"
    }

    // 第二行：仅在普通文本与常亮文本同时存在时显示常亮文本
    Text {
        id: persistentLabel
        anchors.top: label.bottom
        anchors.left: root.left
        anchors.margins: 4
        visible: root.normalVisible && root.persistentText !== ""
        text: root.persistentText
        color: root.persistentColor
        font.pixelSize: root.fontSize
        style: Text.Outline
        styleColor: "black"
    }

    Timer {
        id: hideTimer
        interval: 1000
        repeat: false
        onTriggered: root.hide()
    }

    // 显示普通文本，持续时间结束后自动结束
    function show(msg, durationMs) {
        normalText = msg
        hideTimer.interval = durationMs > 0 ? durationMs : 1000
        normalVisible = true
        hideTimer.restart()
    }

    // 结束普通文本显示
    function hide() {
        hideTimer.stop()
        normalVisible = false
    }

    // 显示常亮文本，第二个参数为颜色；需调用 hidePersistent() 才会结束显示
    function showPersistent(msg, color) {
        persistentText = msg
        persistentColor = (color === undefined || color === null) ? defaultColor : color
    }

    // 结束常亮文本显示
    function hidePersistent() {
        persistentText = ""
        persistentColor = defaultColor
    }
}

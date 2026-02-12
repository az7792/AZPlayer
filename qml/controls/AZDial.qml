// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dial {
    id: root
    width: 200
    height: 200
    from: 0
    to:360
    startAngle: 90
    endAngle:450
    wrap:true
    snapMode: Dial.NoSnap
    stepSize: 0.1

    // Shift 按下时切换模式
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Shift) {
            root.stepSize = 15
            root.snapMode = Dial.SnapAlways
        }
    }

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Shift) {
            root.stepSize = 0.1
            root.snapMode = Dial.NoSnap
        }
    }

    // 背景样式
    background: Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, 0.3)
        radius: width / 2
        border.color: "#55555580"
        border.width: 2
    }

    // 指针样式
    handle: Rectangle {
        id: handleItem
        x: root.background.x + root.background.width / 2 - width / 2
        y: root.background.y + root.background.height / 2 - height / 2
        border.color: "#212222"
        width: 16
        height: 16
        color: root.hovered ? "#a1cafe" : "white"
        radius: 8
        transform: [
            Translate {
                y: -Math.min(root.background.width, root.background.height) * 0.45 + handleItem.height / 2
            },
            Rotation {
                angle: root.angle
                origin.x: handleItem.width / 2
                origin.y: handleItem.height / 2
            }
        ]
    }


    // 重置按钮
    Rectangle {
        id: resetBtn
        width: root.width / 2.5
        height: width
        radius: width / 2
        color: "#2e2f30"
        border.color: "#888"
        anchors.centerIn: parent

        Text {
            anchors.centerIn: parent
            text: Math.round(root.value)
            color: "white"
            font.pixelSize: 18
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.value = 0
            hoverEnabled: true
            onEntered: resetBtn.color = "#c42b1c"
            onExited: resetBtn.color = "#2e2f30"
        }
    }
}

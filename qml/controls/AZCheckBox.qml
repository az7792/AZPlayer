// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
Item {
    id: root
    width: 70
    height: 20

    property alias text: text.text
    property alias textColor: text.color
    property color hoverColor: "#3b3b3b"
    property color unhoverColor: "#454545"
    property bool checked: false

    Rectangle {
        id: background
        anchors.fill: parent
        color: mouseArea.containsMouse ? root.hoverColor : root.unhoverColor  // 悬停变色

        Row {
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 2
            anchors.rightMargin: 2

            Rectangle{
                height: 16
                width: height
                color: "#454545"
                border.color: "#a1cafe"
                border.width: 2
                Image {
                    width: 16
                    height: 16
                    visible: root.checked
                    source: "qrc:/icon/check_box_checked.png"
                }
            }

            // ---- 文本 ----
            Text {
                id: text
                verticalAlignment: Text.AlignVCenter
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.checked = !root.checked   // 点击切换状态
        }
    }
}


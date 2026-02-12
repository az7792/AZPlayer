// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

TabBar {
    id:tabBar

    property alias model:repeater.model

    background: Rectangle{
        color:"black"
    }
    position:TabBar.Header

    Repeater {
        id:repeater

        TabButton {
            id: tabBtn
            background: Rectangle {
                color: (tabBtn.pressed || tabBtn.checked) ? "#3b3b3b" : "#333333"
            }
            contentItem: Text {
                text: label
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: tabBtn.checked ? "white" : "#747474"
            }
        }
    }
}

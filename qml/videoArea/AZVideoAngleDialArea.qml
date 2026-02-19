// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import AZPlayer 1.0

Item {
    id: videoAngleDialArea

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

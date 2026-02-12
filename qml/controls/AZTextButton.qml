// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    width: 60
    height: 30

    background: Rectangle {
        anchors.fill: parent
        color: control.pressed ? "#424242" : "#5a5a5a"
        radius: 3
    }

    contentItem: Text {
        text: control.text
        color: "white"
        font.pointSize: 8
        anchors.centerIn: parent
    }
}

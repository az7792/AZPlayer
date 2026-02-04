// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import "../controls"
import "../playerState"
Rectangle{
    id: topBar

    required property AZWindow mainWindow
    property bool isActive: false

    AZEventBlocker{anchors.fill: parent}

    AZPlayerAbout{
        id:appAbout
    }

    AZButton{
        id:menuBtn
        height: parent.height
        width: 2.5 * height
        anchors.left: parent.left
        text: "AZPlayer"
        tooltipText: "关于"
        onClicked: appAbout.show();
    }

    Rectangle{
        id: textWrapper
        height: parent.height
        anchors.left: menuBtn.right
        anchors.right: windowControls.left
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        color: "#1d1d1d"
        clip: true
        DragHandler {
            target: null
            onActiveChanged: if(active) mainWindow.startMoveWindow()
        }
        MouseArea {
            anchors.fill: parent            
            onDoubleClicked: mainWindow.toggleMaximize()
        }
        Text {
            id: titleText
            color: "white"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            text: AZPlayerState.currentFile
        }
    }

    AZWindowControls{
        id:windowControls
        targetWindow: mainWindow
        height: parent.height
        anchors.right: parent.right
        width: 120
    }
}

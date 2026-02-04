// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: aboutWindow
    title: "关于 AZPlayer"
    width: 300
    height: 200
    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    visible: false
    minimumWidth: 300
    minimumHeight: 200
    maximumWidth: 300
    maximumHeight: 200

    Rectangle {
        anchors.fill: parent
        color: "#2e2f30"

        Image {
            id: icon
            source: "qrc:/icon/AZPlayer.ico"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 10
            anchors.topMargin: 10
            fillMode: Image.PreserveAspectFit
            width: 64
            height: 64
        }

        Text {
            id:aboutText
            anchors.left: icon.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 10
            color: "white"
            text: "<b>AZPlayer v0.3.0</b><br><br>" +
                  "本项目开源地址: <a href=\"https://github.com/az7792/AZPlayer\">GitHub</a><br><br>" +
                  "开放源代码许可: <a href='" + appDirPath + "/LICENSES'>开源许可</a>"
            textFormat: Text.RichText
            onLinkActivated:function(link) {
                Qt.openUrlExternally(link)
            }
        }


    }
}

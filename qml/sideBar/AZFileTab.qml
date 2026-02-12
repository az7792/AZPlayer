// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import "../controls"
import "../playerState"
import "../topBar"

ColumnLayout{
    property alias fileListView: fileTab

    AZListView{
        id: fileTab
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: "#3b3b3b"
        listModel: AZPlayerState.mediaListModel

        onDelActive:{
            AZPlayerState.currentFile = ""
            MediaCtrl.close()
        }

        onStopActive: {
            AZPlayerState.currentFile = ""
            MediaCtrl.close()
        }

        Connections {
            target: AZPlayerState.mediafileDialog
            function onOpenMediaByIdx(index) {
                fileTab.setHighlightIdx(index)
                AZPlayerState.currentFile = fileTab.listModel.get(index).text
                MediaCtrl.open(AZPlayerState.mediafileDialog.activeFilePath)
            }
        }

        onOpenIndex:function(index){
            AZPlayerState.currentFile = fileTab.listModel.get(index).text
            MediaCtrl.open(fileTab.listModel.get(index).fileUrl)
        }
    }

    Rectangle{
        Layout.fillWidth: true
        Layout.minimumHeight: 30
        Layout.maximumHeight: 30
        color: "#323232"
        AZTextButton{
            id:addFileBtn
            width: 60
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            anchors.leftMargin: 3
            text: "添加文件"
            onClicked: AZPlayerState.mediafileDialog.addMediaFile()
        }
    }
}

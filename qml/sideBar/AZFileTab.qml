// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import AZPlayer 1.0

ColumnLayout{
    property alias listView: fileTab

    AZListView{
        id: fileTab
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: "#3b3b3b"
        listModel: AZMediaList

        onRemoveIndex: function(index) {
            AZMediaList.removeMedia(index)
        }

        Connections {
            target: AZMediaList
            // 播放索引变化时只更新播放行（停止/删除时保留单击选中项）
            function onCurrentIndexChanged() {
                fileTab.setActiveIdx(AZMediaList.currentIndex)
            }
            // 主动打开媒体时同步高亮和选中项
            function onMediaOpened(index) {
                fileTab.setHighlightIdx(index)
            }
        }

        onOpenIndex:function(index){
            AZMediaList.open(index)
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
            onClicked: AZFileDialog.addMediaFiles()
        }
    }
}

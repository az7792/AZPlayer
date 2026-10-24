// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import VideoWindow 1.0
import AZPlayer 1.0
Rectangle {
    id: sideBar
    property bool hasPopupOpened: false

    // 请求把焦点从侧栏收回视频区域（Esc）
    signal requestClearFocus()

    function switchTab(delta) {
        if (tabBar.count <= 0) return
        const n = tabBar.count
        tabBar.currentIndex = ((tabBar.currentIndex + delta) % n + n) % n
    }

    // 聚焦当前页签对应的列表
    function focusCurrentView() {
        const page = stackView.children[tabBar.currentIndex]
        if (page) page.listView.focusView()
    }

    // 侧栏内列表的按键冒泡到这里统一处理
    Keys.onPressed: function(event) {
        // ESC 回收焦点
        if (event.key === Qt.Key_Escape) {
            event.accepted = true
            requestClearFocus()
            return
        }

        // 左右方向键切换列表
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            event.accepted = true
            if (!event.isAutoRepeat)
                switchTab(event.key === Qt.Key_Left ? -1 : 1)
        }
    }

    AZEventBlocker{anchors.fill: parent}

    ColumnLayout {
        anchors.fill: parent
        spacing: 1
        AZTabBar{
            id:tabBar
            Layout.fillWidth: true
            model: ListModel {
                ListElement { label: "文件" }
                ListElement { label: "字幕" }
                ListElement { label: "音频" }
            }
        }

        StackLayout {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // 切换页签后让对应列表直接获得焦点
            onCurrentIndexChanged: focusCurrentView()

            AZFileTab{
                id: fileTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }

            AZSubtitleTab{
                id: subtitleTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }

            AZAudioTab{
                id: audioTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }
        }
    }
}


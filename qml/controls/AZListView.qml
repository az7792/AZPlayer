// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

Rectangle {
    id:root
    clip: true

    // 记录当前播放和单击选择
    property int activeIndex: -1
    property ListModel listModel: null
    property bool readOnly: false

    signal removeIndex(int index)
    signal openIndex(int index)

    function setHighlightIdx(idx){
        activeIndex = idx;
        listView.currentIndex = idx
    }

    // 只更新正在播放的行，不改变用户的单击选中项
    function setActiveIdx(idx){
        activeIndex = idx
    }

    // 让列表直接获得焦点
    function focusView() {
        listView.forceActiveFocus()
    }

    // 播放当前选中项
    function openSelection() {
        const idx = listView.currentIndex
        if (idx >= 0 && root.activeIndex !== idx) openIndex(idx)
    }

    ListView {
        id: listView
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        highlight:Rectangle{
            width: listView.width
            height: 30
            color: listView.focus ? "#2b2b2b" : "#444444"
        }
        model: listModel
        keyNavigationEnabled: true
        // 上下键导航后让选中行保持可见
        onCurrentIndexChanged: {
            if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        }

        // 点击时让列表获得焦点
        TapHandler { onTapped: focusView() }

        ScrollBar.vertical: ScrollBar {
            id:scrollBar
            policy: ScrollBar.AsNeeded
            background: Rectangle {
                color: "#222222"
            }
            contentItem: Rectangle {
                implicitWidth: 8
                color: scrollBar.pressed ? "#666666" : "#888888"
            }
        }

        delegate: Text {
            id: textItem
            width: listView.width
            height: 20
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: {
                // count >= 1 才会显示项, 不用担心负数
                let padLength = String(listView.count).length
                return " " + String(index + 1).padStart(padLength, '0') + ". " + model.text
            }

            color: index == root.activeIndex ? "#2b82d5" : (index == listView.currentIndex ? "#f0f0f0" : "#747474")

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = index
                    listView.forceActiveFocus()
                }
                onDoubleClicked: {
                    if(root.activeIndex !== index)
                        openIndex(index)
                }
            }
        }

        Keys.onShortcutOverride:function(event) {
            // 列表有焦点时，上下键用于选择、左右键交给侧栏切页签，不触发快进快退/音量调节
            const key = event.key
            event.accepted = key === Qt.Key_Up || key === Qt.Key_Down ||
                             key === Qt.Key_Left || key === Qt.Key_Right
        }

        Keys.onPressed:function(event) {
            const key = event.key
            if (key === Qt.Key_Return || key === Qt.Key_Enter) {
                openSelection()
                event.accepted = true
            } else if (!root.readOnly && key === Qt.Key_Delete && listView.currentIndex >= 0) {
                removeIndex(listView.currentIndex)

                // 更新选中索引
                if (listView.currentIndex >= listModel.count)
                    listView.currentIndex = listModel.count - 1

                event.accepted = true
            }
        }

    }

}

// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
ComboBox {
    id:root

    property alias popupWidth: mypopup.width

    // 渲染下拉列表的每一项
    delegate: ItemDelegate {
        id: delegate

        required property var model
        required property int index

        width: mypopup.width
        implicitHeight: 30
        contentItem: Text {
            anchors.fill: parent
            text: delegate.model[root.textRole]
            color: "#ebebeb"
            font: root.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: delegate.highlighted ? "#2b2b2b" : "#3b3b3b"
        }
    }

    background: Rectangle{
        implicitWidth: 30
        implicitHeight: 40
        color: root.pressed ? "#252525" : "#3b3b3b"
        border.width: root.visualFocus ? 2 : 1
    }

    // ComboBox右侧的小箭头
    indicator:Text{
        text:">"
        font.pixelSize: 16
        anchors.right: root.right
        anchors.verticalCenter: root.verticalCenter
        width: 16
        height: 16
        color: "#ebebeb"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    contentItem: Text {
        leftPadding: 2
        rightPadding: root.indicator.width

        text: root.displayText
        font: root.font
        color: root.pressed ? "white" : "#ebebeb"
        verticalAlignment: Text.AlignVCenter
    }

    popup: Popup {
        id: mypopup
        y: root.height - 1
        x: root.width + 1
        height: Math.min(contentItem.implicitHeight, root.Window.height - topMargin - bottomMargin)
        padding: 1

        contentItem: ListView {
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            implicitHeight: contentHeight
            model: root.delegateModel
            currentIndex: root.highlightedIndex
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
        }

        background: Rectangle {
            border.color: "#a1cafe"
            border.width: 1
            color: "#3b3b3b"
        }
    }
}

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 1.15
import AZPlayer 1.0

Item {
    // 控制区域显示
    property bool showAreas: false
    property int dragCount: 0
    property var areas: [
        {
            title: "播放",
            handler: function(urls) {
                const mediaFiles = FileHelper.expandMediaFiles(urls, true)
                if (mediaFiles.length > 0) {
                    AZPlayerState.mediaListModel.clear()
                    for (let i = 0; i < mediaFiles.length; ++i) {
                        AZPlayerState.mediaListModel.append(mediaFiles[i])
                    }
                    let dialog = AZPlayerState.mediafileDialog
                    dialog.activeFilePath = mediaFiles[0].fileUrl
                    dialog.openMediaByIdx(0)
                }
            }
        },
        {
            title: "添加到列表",
            handler: function(urls) {
                const mediaFiles = FileHelper.expandMediaFiles(urls, true)
                for (let i = 0; i < mediaFiles.length; ++i) {
                    if (!AZPlayerState.mediaListModel.existsInModel(mediaFiles[i].fileUrl.toString())) {
                        AZPlayerState.mediaListModel.append(mediaFiles[i])
                    }
                }
            }
        },
        {
            title: "以外部字幕打开",
            handler: function(urls) {
                const subtitleFiles = FileHelper.expandSubtitleFiles(urls, true)

                if (subtitleFiles.length > 0) {
                    MediaCtrl.loadExternalSubtitle(subtitleFiles[0].fileUrl)
                } else if (urls.length > 0) {
                    MediaCtrl.loadExternalSubtitle(urls[0])
                }
            }
        },
        {
            title: "以外部音频打开",
            handler: function(urls) {
                const audioFiles = FileHelper.expandAudioFiles(urls, true)

                if (audioFiles.length > 0) {
                    MediaCtrl.loadExternalAudio(audioFiles[0].fileUrl)
                } else if (urls.length > 0) {
                    MediaCtrl.loadExternalAudio(urls[0])
                }
            }
        }
    ]

    // 控制 4 个子 dropArea 是否显示
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]

        onEntered: function(drag) {
            dragCount++
            showAreas = true
            drag.accept()
        }

        onExited: {
            dragCount = Math.max(0, dragCount - 1)
            exitDelay.restart()
        }

        onDropped: function(drop) {
            showAreas = false
            dragCount = 0
            drop.acceptProposedAction()
        }
    }

    // 4个放置区域
    GridLayout {
        anchors.fill: parent
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        visible: showAreas

        Repeater {
            model: areas

            DropArea {
                id: dropArea
                keys: ["text/uri-list"]
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: dropArea.containsDrag ? "transparent" : "#99000000"
                    border.color: dropArea.containsDrag ? "white" : "#1f1f1f"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData.title
                        font.pixelSize: 32
                        color: "#ebebeb"
                        style: Text.Outline
                        styleColor: "black"
                    }
                }

                onEntered: function(drag) {
                    drag.accept()
                    dragCount++
                    showAreas = true
                }

                onExited: {
                    dragCount = Math.max(0, dragCount - 1)
                    exitDelay.restart()
                }

                onDropped: function(drop) {
                    modelData.handler(drop.urls)
                    drop.acceptProposedAction()
                    dragCount = 0
                    showAreas = false
                }
            }
        }
    }

    Timer {
        id: exitDelay
        interval: 100
        repeat: false
        onTriggered: {
            showAreas = dragCount > 0
        }
    }
}

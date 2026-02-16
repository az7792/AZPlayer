// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15
import AZPlayer 1.0

Item {
    property string currentFile: ""            // 当前播放的媒体文件
    property real videoX: 0                    // 视频x
    property real videoY: 0                    // 视频y
    property real videoAngle:0.0               // 视频顺时针旋转(角度)
    property int  videoScale:100               // 缩放(100为不缩放)
    property bool videoShowSubtitle: true      // 是否显示字幕
    property bool videoHorizontalMirror: false // 水平镜像
    property bool videoVerticalMirror: false   // 垂直镜像

    property alias mediaListModel: fileDialog.mediaListModel
    property alias mediafileDialog: fileDialog

    // FileDialog
    AZFileDialog{
        id: fileDialog
        currentPlayingFile: currentFile // NOTE: 避免 AZFileDialog 直接访问 AZPlayerState.currentFile 导致的循环引用
    }
}

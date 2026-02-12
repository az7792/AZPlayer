// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import VideoWindow 1.0
import "../playerState"
Item {
    VideoWindow {
        anchors.fill: parent
        id:videoWindow
        objectName: "videoWindow"
        Component.onCompleted:{
            MediaCtrl.setVideoWindow(videoWindow)
        }
        Connections {
            target: AZPlayerState
            function onVideoXChanged() {videoWindow.setX(AZPlayerState.videoX)}
            function onVideoYChanged() {videoWindow.setY(AZPlayerState.videoY)}
            function onVideoAngleChanged() {videoWindow.setAngle(AZPlayerState.videoAngle)}
            function onVideoScaleChanged() {videoWindow.setScale(AZPlayerState.videoScale)}
            function onVideoShowSubtitleChanged() {videoWindow.setShowSubtitle(AZPlayerState.videoShowSubtitle) }
            function onVideoHorizontalMirrorChanged() {videoWindow.setHorizontalMirror(AZPlayerState.videoHorizontalMirror) }
            function onVideoVerticalMirrorChanged() {videoWindow.setVerticalMirror(AZPlayerState.videoVerticalMirror) }
        }
    }
}

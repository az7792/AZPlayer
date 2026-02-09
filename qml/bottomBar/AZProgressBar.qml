// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import "../controls"
Rectangle{
    id:progressBar
    height: 20
    color: "#1d1d1d"

    AZSlider{
        id:videoSlider

        property bool isSeeking: false
        property real lastSeekTime: 0  // 上次实际 seek 的时间 (毫秒)

        // 延时seek
        Timer {
            id: seekDelayTimer
            interval: 50
            repeat: false
            onTriggered: {
                videoSlider.doSeek(videoSlider.value)
            }
        }

        // 执行seek后延时恢复seeking状态，避免进度条鬼畜
        Timer {
            id: closeSeekingTimeer
            interval: 350
            repeat: false
            onTriggered: {
                videoSlider.isSeeking = false
            }
        }

        function doSeek(targetValue) {
            videoSlider.lastSeekTime = Date.now() // 当前系统时间（毫秒）
            MediaCtrl.seekBySec(targetValue, 0.0)
            closeSeekingTimeer.start()
        }

        height: parent.height
        anchors.left: parent.left
        anchors.right: volumeBtn.left
        from: 0.0
        value: (pressed || isSeeking) ? value : MediaCtrl.progress
        to: MediaCtrl.duration
        stepSize: 0.01
        snapMode: Slider.SnapOnRelease
        onValueChanged: {
            if(!pressed) return
            videoSlider.isSeeking = true
            closeSeekingTimeer.stop()
            let timeDiff = Date.now() - lastSeekTime
            if (timeDiff >= 100) {
                seekDelayTimer.stop()
                doSeek(value)
            } else {
                // 延时执行
                seekDelayTimer.restart()
            }
        }
        Keys.onPressed: (event) => {
            event.accepted = true
        }
    }

    AZButton{
        id:volumeBtn
        height: parent.height
        width: height
        anchors.right: volumSlider.left
        iconWidth: 16
        iconHeight: 16
        iconSource: MediaCtrl.muted ? "qrc:/icon/volume_off.png" : "qrc:/icon/volume_on.png"
        tooltipText: "静音 开/关"
        onLeftClicked: {
            MediaCtrl.toggleMuted();
        }
    }

    AZSlider{
        id:volumSlider
        height: parent.height
        width: 100
        anchors.right: parent.right
        from: 0.0
        value: MediaCtrl.volume
        to: 1.0
        stepSize: 0.01
        snapMode: Slider.SnapOnRelease
        onMoved:{
            MediaCtrl.setVolume(value);
        }
        Keys.onPressed: (event) => {
            event.accepted = true
        }
    }
}

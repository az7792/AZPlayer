// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
Rectangle{
    id:progressBar
    height: 20
    color: "#1d1d1d"

    property int currentTime: 0

    Timer {
        id: currentTimeTimer
        interval: 500
        repeat: true
        onTriggered: {
            progressBar.currentTime = MediaCtrl.getCurrentTime()
        }
    }
    Component.onCompleted:{
        currentTimeTimer.start()
    }

    AZSlider{
        id:videoSlider

        property bool isSeeking: false
        property real seekTs: 0.0

        height: parent.height
        anchors.left: parent.left
        anchors.right: volumeBtn.left
        from: 0.0
        value: (pressed || isSeeking) ? value : progressBar.currentTime
        to: MediaCtrl.duration
        stepSize: 0.01
        snapMode: Slider.SnapOnRelease
        onValueChanged: {
            if(pressed){
                seekTs = value
                videoSlider.isSeeking = true
                MediaCtrl.seekBySec(videoSlider.seekTs,0.0)
                //seekDelay1.restart()
            }
        }
        Connections {
            target: MediaCtrl
            function onSeeked() {
                progressBar.currentTime = MediaCtrl.getCurrentTime()//强制更新
                videoSlider.isSeeking = false
            }
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
    }
}

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import VideoWindow 1.0
import AZPlayer 1.0
Popup{
    id:root
    background: Rectangle{
        color: "#1c1c1c"
        border.color: "#a1cafe"
        border.width: 1
    }
    padding: 10

    contentItem: ColumnLayout {
        AZTabBar{
            id:tabBar
            Layout.fillWidth: true
            model: ListModel {
                ListElement { label: "视频" }
                ListElement { label: "字幕" }
                ListElement { label: "音频" }
            }
        }

        StackLayout {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            MyVidioCtrl{}
            MySubtitleCtrl{}
            MyAudioCtrl{}
        }
    }




    component MyVidioCtrl:Column{
        spacing: 5
        Row {
            spacing: 10
            AZTextInput{
                id: inputAngle
                width: 70
                height: 20
                text: "0"
                labelText:"角度:"
                validator: DoubleValidator{
                    bottom: 0.0
                    top: 360.0
                    decimals : 1
                    notation: DoubleValidator.StandardNotation
                }
                anchors.verticalCenter: parent.verticalCenter
                onEditingFinished: AZPlayerState.videoAngle = Number(text)
                onWheelUp: AZPlayerState.videoAngle = (Number(text) + 1) % 360
                onWheelDown: AZPlayerState.videoAngle = (Number(text) - 1 + 360) % 360
                Connections {
                    target: AZPlayerState
                    function onVideoAngleChanged() {
                        inputAngle.text = AZPlayerState.videoAngle.toFixed(1)
                    }
                }
            }

            AZTextInput{
                id: inputScale
                width: 60
                height: 20
                text: "100"
                labelText:"缩放:"
                validator: IntValidator{
                    bottom: 2
                    top: 600
                }
                anchors.verticalCenter: parent.verticalCenter
                onEditingFinished: AZPlayerState.videoScale = Number(text)
                onWheelUp: AZPlayerState.videoScale += 1
                onWheelDown: AZPlayerState.videoScale -= 1
                Connections {
                    target: AZPlayerState
                    function onVideoScaleChanged() {
                        inputScale.text = AZPlayerState.videoScale
                    }
                }
            }

            AZTextInput{
                id: inputX
                width: 60
                height: 20
                text: "0"
                labelText:"X:"
                validator: DoubleValidator{
                    bottom: -10000
                    top: 10000
                    decimals : 1
                    notation: DoubleValidator.StandardNotation
                }
                anchors.verticalCenter: parent.verticalCenter
                onEditingFinished: AZPlayerState.videoX = Number(text)
                onWheelUp: AZPlayerState.videoX += 1
                onWheelDown: AZPlayerState.videoX -= 1
                Connections {
                    target: AZPlayerState
                    function onVideoXChanged() {
                        inputX.text = AZPlayerState.videoX.toFixed(1)
                    }
                }
            }

            AZTextInput{
                id: inputY
                width: 60
                height: 20
                text: "0"
                labelText:"Y:"
                validator: DoubleValidator{
                    bottom: -10000
                    top: 10000
                    decimals : 1
                    notation: DoubleValidator.StandardNotation
                }
                anchors.verticalCenter: parent.verticalCenter
                onEditingFinished: AZPlayerState.videoY = Number(text)
                onWheelUp: AZPlayerState.videoY += 1
                onWheelDown: AZPlayerState.videoY -= 1
                Connections {
                    target: AZPlayerState
                    function onVideoYChanged() {
                        inputY.text = AZPlayerState.videoY.toFixed(1)
                    }
                }
            }

            AZTextButton{
                id: resetBtn
                width: 60
                height: 25
                anchors.verticalCenter: parent.verticalCenter
                text: "恢复屏幕"
                onClicked: {
                    AZPlayerState.videoX = 0
                    AZPlayerState.videoY = 0
                    horizontalMirrorCheckBox.checked = false
                    verticalMirrorCheckBox.checked = false
                    AZPlayerState.videoAngle = 0
                    AZPlayerState.videoScale = 100
                }
            }
        }

        Row {
            spacing: 3
            AZCheckBox{
                id: horizontalMirrorCheckBox
                height: 20
                width: 80
                text:"水平镜像"
                textColor: "#ebebeb"
                onCheckedChanged: { AZPlayerState.videoHorizontalMirror = checked }
            }

            AZCheckBox {
                id: verticalMirrorCheckBox
                height: 20
                width: 80
                text:"垂直镜像"
                textColor: "#ebebeb"
                onCheckedChanged: { AZPlayerState.videoVerticalMirror = checked }
            }
        }
    }

    component MySubtitleCtrl:Column{
        spacing: 5
        AZCheckBox {
            id: showSubtitleCheckBox
            height: 20
            width: 80
            text:"显示字幕"
            checked: true
            textColor: "#ebebeb"
            onCheckedChanged: { AZPlayerState.videoShowSubtitle = checked }
        }
    }

    component MyAudioCtrl:Item{}
}

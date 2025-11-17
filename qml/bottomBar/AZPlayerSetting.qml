import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import VideoWindow 1.0
import "../controls"
Rectangle{
    id:root
    color: "#1c1c1c"
    border.color: "#a1cafe"
    border.width: 1

    property VideoWindow videoWindow: null
    property AZDial angleDial : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: border.width
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
                onEditingFinished: angleDial.value = Number(text) //angleDial会自己调用setAngle
                onWheelUp: angleDial.value = (Number(text) + 1) % 360     //angleDial会自己调用setAngle
                onWheelDown: angleDial.value = (Number(text) - 1 + 360) % 360   //angleDial会自己调用setAngle
                Connections {
                    target: videoWindow
                    function onVideoAngleChanged() {
                        inputAngle.text = videoWindow.angle().toFixed(1)
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
                onEditingFinished: videoWindow.setScale(Number(text))
                onWheelUp: videoWindow.addScale()
                onWheelDown: videoWindow.subScale()
                Connections {
                    target: videoWindow
                    function onVideoScaleChanged() {
                        inputScale.text = videoWindow.scale()
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
                onEditingFinished: {videoWindow.setX(Number(text))
                console.log(text)}
                onWheelUp: videoWindow.addX()
                onWheelDown: videoWindow.subX()
                Connections {
                    target: videoWindow
                    function onVideoXChanged() {
                        inputX.text = videoWindow.tx().toFixed(1)
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
                onEditingFinished: videoWindow.setY(-Number(text)) // QML坐标的Y轴与OpenGL的Y轴是相反的
                onWheelUp: videoWindow.subY() // QML坐标的Y轴与OpenGL的Y轴是相反的
                onWheelDown: videoWindow.addY() // QML坐标的Y轴与OpenGL的Y轴是相反的
                Connections {
                    target: videoWindow
                    function onVideoYChanged() {
                        inputY.text = -videoWindow.ty().toFixed(1) // QML坐标的Y轴与OpenGL的Y轴是相反的
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
                    videoWindow.setXY(0,0)
                    horizontalMirrorCheckBox.checked = false
                    verticalMirrorCheckBox.checked = false
                    angleDial.value = 0
                    videoWindow.setScale(100)
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
                onCheckedChanged: videoWindow.setHorizontalMirror(checked)
            }

            AZCheckBox {
                id: verticalMirrorCheckBox
                height: 20
                width: 80
                text:"垂直镜像"
                textColor: "#ebebeb"
                onCheckedChanged: videoWindow.setVerticalMirror(checked)
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
            onCheckedChanged: videoWindow.setShowSubtitle(checked)
        }
    }

    component MyAudioCtrl:Item{}
}

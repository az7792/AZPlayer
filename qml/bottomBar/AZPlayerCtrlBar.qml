// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import "../controls"
import "../playerState"
Item{
    id: playerCtrlBar
    height: 40

    // 开关侧边栏
    signal requestToggleSideBar()

    // 打开setting
    signal requestOpenSetting()

    readonly property bool popupOpened: chapterComboBox.popupOpened

    required property bool playerSettingOpened
    required property AZListView fileListView

    Row{
        id:btnRow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 1
        AZButton{
            id:playBtn
            height: parent.height
            width: height
            iconWidth: 16
            iconHeight: 16
            iconSource: MediaCtrl.paused ? "qrc:/icon/play.png" : "qrc:/icon/pause.png"
            tooltipText: MediaCtrl.paused ? "播放" : "暂停"
            onLeftClicked: MediaCtrl.togglePaused();
        }
        AZButton{
            id:stopBtn
            height: parent.height
            width: height
            iconWidth: 16
            iconHeight: 16
            iconSource: "qrc:/icon/stop.png"
            tooltipText: "停止"
            onLeftClicked: MediaCtrl.close()
        }
        AZButton{
            id:skipPrevBtn
            height: parent.height
            width: height
            iconWidth: 16
            iconHeight: 16
            iconSource: "qrc:/icon/skip_previous.png"
            tooltipText: "L:快退，R:上一个"
            onLeftClicked: MediaCtrl.fastRewind()
            onRightClicked: playerCtrlBar.fileListView.openPrev()
        }
        AZButton{
            id:skipNextBtn
            height: parent.height
            width: height
            iconWidth: 16
            iconHeight: 16
            iconSource: "qrc:/icon/skip_next.png"
            tooltipText: "L:快进，R:下一个"
            onLeftClicked: MediaCtrl.fastForward()
            onRightClicked: playerCtrlBar.fileListView.openNext()
        }

        AZButton{
            id:playbackModeBtn
            //0播完重播 1列表顺序 2列表随机
            property int mode: 0

            height: parent.height
            width: height
            iconHeight: 20
            iconWidth: 20
            iconSource: {
                if(mode === 0)
                    return "qrc:/icon/repeat_one.png"
                else if(mode === 1)
                    return "qrc:/icon/repeat.png"
                else
                    return "qrc:/icon/shuffle.png"
            }
            tooltipText:mode === 0? "播完重播" : (mode === 1 ? "顺序播放" : "随机播放")
            onLeftClicked: {
                mode = (mode + 1) % 3
                if(mode === 0){
                    MediaCtrl.setLoopOnEnd(true)
                } else {//1 or 2
                    MediaCtrl.setLoopOnEnd(false)
                }
            }
            Connections {
                target: MediaCtrl
                function onPlayed() {
                    if(playbackModeBtn.mode === 1){
                        playerCtrlBar.fileListView.openNext()
                    } else if(playbackModeBtn.mode === 2){
                        playerCtrlBar.fileListView.openRandom()
                    }
                }
            }
        }

        AZButton{
            id:openFileBtn
            height: parent.height
            width: height
            iconWidth: 16
            iconHeight: 16
            iconSource: "qrc:/icon/open.png"
            tooltipText: "打开文件"
            onLeftClicked: AZPlayerState.mediafileDialog.openMediaFile()
        }
    }


    Rectangle{
        height: parent.height
        anchors.left: btnRow.right
        anchors.right: playerSetting.left
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        color: "#1c1c1c"
        clip: true

        function secondsToHMS(seconds) {
            var h = Math.floor(seconds / 3600);
            var m = Math.floor((seconds % 3600) / 60);
            var s = seconds % 60;

            // 保证两位数字显示
            var hh = h < 10 ? "0" + h : "" + h;
            var mm = m < 10 ? "0" + m : "" + m;
            var ss = s < 10 ? "0" + s : "" + s;

            return hh + ":" + mm + ":" + ss;
        }

        Text {
            id: mediaDurationText1
            color: "#ebebeb"
            text: parent.secondsToHMS(MediaCtrl.progress)
            font.family: "Segoe UI"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            wrapMode: Text.NoWrap
            elide: Text.ElideNone

        }
        Text {
            id: mediaDurationText2
            color: "#747474"
            text: "  /  " + parent.secondsToHMS(MediaCtrl.duration)
            font.family: "Segoe UI"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: mediaDurationText1.right
            wrapMode: Text.NoWrap
            elide: Text.ElideNone
        }

        AZComboBox{
            id: chapterComboBox
            displayText: "章节"
            width: 45
            height: 20
            anchors.left: mediaDurationText2.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            popupWidth: 180
            textRole:"text"
            Connections{
                target: MediaCtrl
                function onChaptersInfoUpdate(){
                    chapterComboBox.model = MediaCtrl.getChaptersInfo();
                }
            }
            onActivated: function(index){
                MediaCtrl.seekBySec(chapterComboBox.model[index]["pts"],0.0);
            }
        }
    }

    AZButton{
        id:playerSetting
        height: parent.height
        width: height
        anchors.right: fileListBtn.left
        anchors.rightMargin: 1
        iconHeight: 20
        iconWidth: 20        
        onLeftClicked: requestOpenSetting()
        iconSource: playerCtrlBar.playerSettingOpened ? "qrc:/icon/player_settings_opened.png" : "qrc:/icon/player_settings_closed.png"
        tooltipText: "打开播放设置"

        AZEventBlocker{
            anchors.fill: parent
            visible: playerCtrlBar.playerSettingOpened

            onPositionChanged: (mouse) => {
                if (containsMouse) {
                    let globalPos = playerSetting.mapToItem(playerCtrlBar.window, 0, 0)
                    AZTooltip.show("关闭播放设置" ,mouse.x + globalPos.x, mouse.y + globalPos.y)
                }
            }

            onExited: {
                AZTooltip.hide()
            }
        }
    }

    AZButton{
        id:fileListBtn
        height: parent.height
        width: height
        anchors.right: parent.right
        
        property bool sideBarOpened: false
        property bool tooltipEnabled: true

        // 切换侧边栏后延时一段时间再启用tooltip，避免因窗口尺寸改变导致tooltip乱飞
        Timer {
            id: tooltipDelayTimer
            interval: 100
            repeat: false
            onTriggered: {
                fileListBtn.tooltipEnabled = true
            }
        }

        onLeftClicked: {
            AZTooltip.hide()
            tooltipEnabled = false
            tooltipDelayTimer.restart()

            playerCtrlBar.requestToggleSideBar()
            sideBarOpened = !sideBarOpened
        }
        iconHeight: 20
        iconWidth: 20
        iconSource: sideBarOpened ? "qrc:/icon/list_opened.png" : "qrc:/icon/list.png"
        tooltipText: tooltipEnabled ? (sideBarOpened ? "关闭列表" : "打开列表") : ""
    }
}

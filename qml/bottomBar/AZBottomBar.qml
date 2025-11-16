import QtQuick
import "../fileDialog"
//底部控制栏
Rectangle{
    id:bottomBar
    height: progressBar.height + playerCtrlBar.height + 1

    property alias listView: playerCtrlBar.listView
    property alias splitView: playerCtrlBar.splitView
    property alias videoWindow: playerSetting.videoWindow
    property alias angleDial: playerSetting.angleDial
    property AZFileDialog fileDialog:null
    property alias playerSettingOpened: playerCtrlBar.playerSettingOpened

    color: "black"    

    AZPlayerSetting{
        id:playerSetting
        visible: playerCtrlBar.playerSettingOpened
        height: 250
        width: 400
        anchors.right: parent.right
        anchors.bottom: progressBar.top
    }

    AZProgressBar{
        id:progressBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    AZPlayerCtrlBar{
        id:playerCtrlBar
        currentTime: progressBar.currentTime
        z:1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.topMargin: 1
        onOpenFileBtnClicked: fileDialog.openFile()
    }
}

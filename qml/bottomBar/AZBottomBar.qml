import QtQuick
import "../fileDialog"
import "../controls"
import "../playerState"
//底部控制栏
Rectangle{
    id: bottomBar
    height: progressBar.height + playerCtrlBar.height + 1

    // 开关侧边栏
    signal requestToggleSideBar()

    property bool isActive: playerSetting.opened | playerCtrlBar.popupOpened

    required property AZListView fileListView

    color: "black"

    AZEventBlocker{
        anchors.fill: parent
        onWheel: function(wheel) {
            if (wheel.angleDelta.y > 0) MediaCtrl.addVolum()
            else if (wheel.angleDelta.y < 0) MediaCtrl.subVolum()
        }
    }

    AZPlayerSetting{
        id:playerSetting
        height: 250
        width: 400
        x: parent.width - width
        y: -height
    }

    AZProgressBar{
        id:progressBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    AZPlayerCtrlBar{
        id:playerCtrlBar
        z:1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.topMargin: 1
        fileListView: bottomBar.fileListView        
        onRequestToggleSideBar: bottomBar.requestToggleSideBar()
        playerSettingOpened: playerSetting.opened
        onRequestOpenSetting: playerSetting.open()
    }    
}

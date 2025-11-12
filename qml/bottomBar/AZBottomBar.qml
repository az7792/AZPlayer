import QtQuick
import "../fileDialog"
//底部控制栏
Rectangle{
    id:bottomBar
    height: progressBar.height + playerCtrlBar.height + 1

    property alias listView: playerCtrlBar.listView
    property alias splitView: playerCtrlBar.splitView
    property alias videoWindow: playerCtrlBar.videoWindow
    property alias angleDial: playerCtrlBar.angleDial
    property AZFileDialog fileDialog:null

    color: "black"

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

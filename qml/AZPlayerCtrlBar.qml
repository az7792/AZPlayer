import QtQuick
import QtQuick.Dialogs
Item{
    id:playerCtrlBar
    height: 40

    property AZTooltip tooltip: null
    property Rectangle textWrapper: null
    property FileDialog fileDialog: null
    property Item splitView: null
    property int currentTime: 0

    AZButton{
        id:playBtn
        height: parent.height
        width: height
        anchors.left: parent.left
        iconWidth: 16
        iconHeight: 16
        iconSource: MediaCtrl.paused ? "qrc:/icon/play.png" : "qrc:/icon/pause.png"
        tooltipText: MediaCtrl.paused ? "播放" : "暂停"
        onLeftClicked: MediaCtrl.togglePaused();
        onHoverTip: (txt, x, y) => playerCtrlBar.tooltip.show(txt, x, y)
        onHideTip: playerCtrlBar.tooltip.hide()
    }
    AZButton{
        id:stopBtn
        height: parent.height
        width: height
        anchors.left: playBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/stop.png"
        tooltipText: "停止"
        onLeftClicked: {
            MediaCtrl.close()
            playerCtrlBar.textWrapper.text = ""
        }
        onHoverTip: (txt, x, y) => playerCtrlBar.tooltip.show(txt, x, y)
        onHideTip: playerCtrlBar.tooltip.hide()
    }
    AZButton{
        id:skipPrevBtn
        height: parent.height
        width: height
        anchors.left:stopBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/skip_previous.png"
        tooltipText: "L:快退，R:上一个"
        onLeftClicked: MediaCtrl.fastRewind()
        onRightClicked: ;
        onHoverTip: (txt, x, y) => playerCtrlBar.tooltip.show(txt, x, y)
        onHideTip: playerCtrlBar.tooltip.hide()
    }
    AZButton{
        id:skipNextBtn
        height: parent.height
        width: height
        anchors.left:skipPrevBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/skip_next.png"
        tooltipText: "L:快进，R:下一个"
        onLeftClicked: MediaCtrl.fastForward()
        onRightClicked: ;
        onHoverTip: (txt, x, y) => playerCtrlBar.tooltip.show(txt, x, y)
        onHideTip: playerCtrlBar.tooltip.hide()
    }
    AZButton{
        id:openFileBtn
        height: parent.height
        width: height
        anchors.left:skipNextBtn.right
        anchors.leftMargin: 1
        iconWidth: 16
        iconHeight: 16
        iconSource: "qrc:/icon/open.png"
        tooltipText: "打开文件"
        onLeftClicked: playerCtrlBar.fileDialog.open()
        onHoverTip: (txt, x, y) => playerCtrlBar.tooltip.show(txt, x, y)
        onHideTip: playerCtrlBar.tooltip.hide()
    }

    Rectangle{
        height: parent.height
        anchors.left: openFileBtn.right
        anchors.right: fileListBtn.left
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        color: "#1c1c1c"

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
            text: parent.secondsToHMS(playerCtrlBar.currentTime)
            font.pixelSize: 10
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
            font.pixelSize: 10
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: mediaDurationText1.right
            wrapMode: Text.NoWrap
            elide: Text.ElideNone
        }
    }

    AZButton{
        id:fileListBtn
        height: parent.height
        width: height
        anchors.right: parent.right
        onLeftClicked: playerCtrlBar.splitView.toggleSidebar()
        text:"列表"
    }
}

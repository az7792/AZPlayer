import QtQuick
import QtQuick.Dialogs
Item{
    id:playerCtrlBar
    height: 40

    property AZListView listView: null
    property Item splitView: null
    property int currentTime: 0
    signal openFileBtnClicked()

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
        onLeftClicked: listView.stopActiveItem()
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
        onRightClicked: listView.openPrev()
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
        onRightClicked: listView.openNext()
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
        onLeftClicked: playerCtrlBar.openFileBtnClicked()
    }

    Rectangle{
        height: parent.height
        anchors.left: openFileBtn.right
        anchors.right: playbackModeBtn.left
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
        id:playbackModeBtn
        //0播完重播 1列表顺序 2列表随机
        property int mode: 0

        height: parent.height
        width: height
        anchors.right: fileListBtn.left
        anchors.rightMargin: 1
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
        tooltipText:mode === 0?"播完重播" : (mode === 1 ? "顺序播放" : "随机播放")
        onLeftClicked: {
            mode = (mode + 1) % 3
        }
    }

    AZButton{
        id:fileListBtn
        height: parent.height
        width: height
        anchors.right: parent.right
        onLeftClicked: playerCtrlBar.splitView.toggleSidebar()
        iconHeight: 20
        iconWidth: 20
        iconSource: "qrc:/icon/list.png"
        tooltipText: "打开/关闭列表"
    }
}

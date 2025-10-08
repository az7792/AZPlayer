import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import VideoWindow 1.0
Window {
    id: mainWin
    width: 640
    height: 480
    visible: true
    title: qsTr("AZPlayer")
    color: "black"
    visibility: Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window| Qt.WindowSystemMenuHint|
           Qt.WindowMaximizeButtonHint | Qt.WindowMinimizeButtonHint

    property int resizeMargin: 8
    property bool forceShowBottomBar: false
    property bool forceShowTopBar: false

    //用于显示提示信息
    AZTooltip {
        id: tooltip
        targetWindow: mainWin
        z:999
    }

    //用于调整窗口大小
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents:true

        //切换调整窗口大小时的鼠标样式
        function toggleCursorShape(x,y){
            // 改变鼠标样式
            if(mainWin.visibility !== Window.Windowed){
                cursorShape = Qt.ArrowCursor
                return
            }

            if (x <= resizeMargin && y <= resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 左上角
            else if (x >= parent.width - resizeMargin && y <= resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 右上角
            else if (x <= resizeMargin && y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeBDiagCursor   // 左下角
            else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeFDiagCursor   // 右下角
            else if (x <= resizeMargin || x >= parent.width - resizeMargin)
                cursorShape = Qt.SizeHorCursor     // 左右边
            else if (y <= resizeMargin || y >= parent.height - resizeMargin)
                cursorShape = Qt.SizeVerCursor     // 上下边
            else
                cursorShape = Qt.ArrowCursor
        }

        //调整窗口大小
        function toggleSystemResize(x,y){
            if(mainWin.visibility !== Window.Windowed){
                return
            }

            let edge = 0
            if (x <= resizeMargin && y <= resizeMargin)
                edge = Qt.LeftEdge | Qt.TopEdge          // 左上角
            else if (x >= parent.width - resizeMargin && y <= resizeMargin)
                edge = Qt.RightEdge | Qt.TopEdge         // 右上角
            else if (x <= resizeMargin && y >= parent.height - resizeMargin)
                edge = Qt.LeftEdge | Qt.BottomEdge       // 左下角
            else if (x >= parent.width - resizeMargin && y >= parent.height - resizeMargin)
                edge = Qt.RightEdge | Qt.BottomEdge      // 右下角
            else if (x <= resizeMargin)
                edge = Qt.LeftEdge                        // 左边
            else if (x >= parent.width - resizeMargin)
                edge = Qt.RightEdge                       // 右边
            else if (y <= resizeMargin)
                edge = Qt.TopEdge                         // 上边
            else if (y >= parent.height - resizeMargin)
                edge = Qt.BottomEdge                      // 下边

            if (edge !== 0) {
                mainWin.startSystemResize(edge)
            }
        }

        //全屏时呼出topBar和bottomBar
        function toggleBarVisible(x,y){
            if(mainWin.visibility !== Window.FullScreen){
                forceShowTopBar = forceShowBottomBar = false
                return
            }
            forceShowTopBar = (y <= topBar.height)
            forceShowBottomBar = (y >= mainWin.height - bottomBar.height)
        }

        onPositionChanged:function(mouse) {
            toggleCursorShape(mouse.x,mouse.y)
            toggleBarVisible(mouse.x,mouse.y)
        }

        onPressed: function(mouse) {
            toggleSystemResize(mouse.x,mouse.y)
        }
    }

    Rectangle{
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        visible: mainWin.visibility !== Window.FullScreen || forceShowTopBar
        height: 32
        color: "black"
        z:1

        AZButton{
            id:menuBtn
            height: parent.height
            width: 2.5 * height
            anchors.left: parent.left
            defaultColor: "#1c1c1c"
            hoverColor: "#252525"
            pressedColor:"#161616"
            text: "AZPlayer"
            tooltipText: "测试"
            iconHeight: 16
            iconWidth: 16
            onClicked: console.log("按下")
            onLeftClicked: console.log("L按下")
            onRightClicked: console.log("R按下")
            onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
            onHideTip: tooltip.hide()
        }

        Rectangle{
            id: textWrapper
            height: parent.height
            anchors.left: menuBtn.right
            anchors.right: windowControls.left
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            color: "#1d1d1d"
            clip: true
            property alias text: titleText.text
            MouseArea {
                anchors.fill: parent
                onPressed: mainWin.startSystemMove()
                onDoubleClicked: mainWin.visibility = mainWin.visibility == Window.Windowed ? Window.Maximized : Window.Windowed
            }
            Text {
                id: titleText
                color: "white"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }

        AZWindowControls{
            id:windowControls
            targetWindow: mainWin
            targetToolTip: tooltip
            height: parent.height
            anchors.right: parent.right
            width: 120
        }
    }

    //视频显示区域
    Item{
        id: videoArea
        anchors.top:topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        anchors.leftMargin: resizeMargin / 2
        anchors.rightMargin: resizeMargin / 2
        z:0

        states: [
            State {
                name: "fullscreen"
                when: mainWin.visibility === Window.FullScreen
                PropertyChanges { target: videoArea;
                    anchors.topMargin: 0; anchors.bottomMargin: 0; anchors.leftMargin: 0; anchors.rightMargin: 0;
                    anchors.top:mainWin.contentItem.top ; anchors.bottom: mainWin.contentItem.bottom;}
            },
            State {
                name: "Maximized"
                when: mainWin.visibility === Window.Maximized
                PropertyChanges { target: videoArea;
                    anchors.top: topBar.bottom; anchors.bottom: bottomBar.top;
                    anchors.topMargin: 1; anchors.bottomMargin: 1; anchors.leftMargin: 0; anchors.rightMargin: 0 }
            },
            State {
                name: "windowed"
                when: mainWin.visibility === Window.Windowed
                PropertyChanges { target: videoArea;
                    anchors.top: topBar.bottom; anchors.bottom: bottomBar.top;
                    anchors.topMargin: 1; anchors.bottomMargin: 1; anchors.leftMargin: resizeMargin / 2; anchors.rightMargin: resizeMargin / 2 }
            }
        ]

        VideoWindow{
            anchors.fill: parent
            id:videoWindow
            objectName: "videoWindow"
            Component.onCompleted:{
                MediaCtrl.setVideoWindow(videoWindow)
            }
        }
    }

    Rectangle{
        id:bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        visible: mainWin.visibility !== Window.FullScreen || forceShowBottomBar
        height: progressBar.height + playerCtrlBar.height + 1
        z:1
        color: "black"

        Component.onCompleted:{
            currentTimeTimer.start()
        }

        property int currentTime: 0
        Timer {
            id: currentTimeTimer
            interval: 500
            repeat: true
            onTriggered: {
                bottomBar.currentTime = MediaCtrl.getCurrentTime()
            }
        }

        Rectangle{
            id:progressBar
            height: 20
            z:1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: "#1d1d1d"

            AZSlider{
                id:videoSlider

                property bool isSeeking: false
                property real seekTs: 0.0

                height: parent.height
                anchors.left: parent.left
                anchors.right: volumeBtn.left
                from: 0.0
                value: (pressed || isSeeking) ? value : bottomBar.currentTime
                to: MediaCtrl.duration
                stepSize: 0.01
                snapMode: Slider.SnapOnRelease
                Timer {
                    id: seekDelay1//防止短时内频繁seek
                    interval: 100
                    repeat: false
                    onTriggered: {
                        MediaCtrl.seekBySec(videoSlider.seekTs,0.0)
                    }
                }
                onValueChanged: {
                    if(pressed){
                        seekTs = value
                        videoSlider.isSeeking = true
                        seekDelay1.restart()
                    }
                }
                Connections {
                    target: MediaCtrl
                    function onSeeked() {
                        bottomBar.currentTime = MediaCtrl.getCurrentTime()//强制更新
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
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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

        Item{
            id:playerCtrlBar
            z:1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: progressBar.bottom
            anchors.topMargin: 1
            height: 40

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
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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
                    textWrapper.text = ""
                }
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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
                onLeftClicked: fileDialog.open()
                onHoverTip: (txt, x, y) => tooltip.show(txt, x, y)
                onHideTip: tooltip.hide()
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
                    text: parent.secondsToHMS(bottomBar.currentTime)
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
                text:"列表"
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择一个媒体文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFile
        nameFilters: ["媒体文件 (*.mp4 *.mkv *.avi *.mp3 *.aac *.mka)", "所有文件 (*)"]
        onAccepted: {
            console.log("选中的文件:", selectedFile)
            var ok = MediaCtrl.open(selectedFile)
            if(ok){
                textWrapper.text = selectedFile.toString().split(/[\\/]/).pop()
            }
        }
        onRejected: console.log("取消选择")
    }
}

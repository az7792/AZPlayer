import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import VideoWindow 1.0
import QtQuick.Layouts
import AZPlayer 1.0
Window {
    id: mainWin
    width: 640
    minimumWidth: splitView.leftMinWidth + splitView.rightMinWidth + 20
    height: 480
    minimumHeight: 360
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
    Component.onCompleted: {
        AZTooltip.mainWindow = mainWin
    }

    //用于调整窗口大小
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents:true

        // 定时器，隐藏鼠标
        Timer {
            id: hideTimer
            interval: 1000
            repeat: false
            onTriggered: {
                parent.cursorShape = Qt.BlankCursor
            }
        }

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

        // 控制鼠标在视频区域内是否隐藏
        function toggleMouseHidden(x,y){
            let topY = videoArea.y + (mainWin.visibility === Window.FullScreen ? topBar.height : 0)
            let bottomY = videoArea.y + videoArea.height - (mainWin.visibility === Window.FullScreen ? bottomBar.height : 0)
            let pd = 10;
            if(x>=videoArea.x + pd && x<=videoArea.x + videoArea.width -pd && y>=topY+pd && y <= bottomY-pd){
                if(cursorShape === Qt.BlankCursor){
                    cursorShape = Qt.ArrowCursor
                }
                hideTimer.restart()
            }else{
                hideTimer.stop()
            }
        }

        //全屏时呼出topBar和bottomBar
        function toggleBarVisible(x,y){
            if(mainWin.visibility !== Window.FullScreen){
                forceShowTopBar = forceShowBottomBar = false
                return
            }
            forceShowTopBar = (y <= topBar.height && (!fileListBar.visible || x <= fileListBar.x))
            forceShowBottomBar = (y >= mainWin.height - bottomBar.height)
        }

        onPositionChanged:function(mouse) {
            toggleCursorShape(mouse.x,mouse.y)
            toggleBarVisible(mouse.x,mouse.y)
            toggleMouseHidden(mouse.x,mouse.y)
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
            height: parent.height
            anchors.right: parent.right
            width: 120
        }
    }

    Item{
        id: splitView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: mainWin.visibility === Window.FullScreen ? parent.top : topBar.bottom
        anchors.bottomMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.leftMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.rightMargin: mainWin.visibility === Window.Windowed ? resizeMargin / 2 : 0
        anchors.topMargin: mainWin.visibility === Window.FullScreen ? 0 : 1
        z:0

        property int leftMinWidth: 360      // 左侧最小宽度
        property int rightMinWidth: 140     // 右侧最小宽度
        property bool isLeftExpanded: (mainWin.visibility === Window.FullScreen || !showed) ? true : false
        property bool showed: false
        property int _rightRectWidth: 200

        function toggleSidebar(){
            showed = !showed
        }

        //视频显示区域
        Item{
            id: videoArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: splitView.isLeftExpanded ? parent.right : splitter.left
            anchors.bottom: mainWin.visibility === Window.FullScreen ? parent.bottom : bottomBar.top
            anchors.bottomMargin: 1
            z:0

            VideoWindow{
                anchors.fill: parent
                id:videoWindow
                objectName: "videoWindow"
                Component.onCompleted:{
                    MediaCtrl.setVideoWindow(videoWindow)
                }
            }
        }

        //底部控制栏
        Rectangle{
            id:bottomBar
            anchors.left: parent.left
            anchors.right: splitView.showed ? splitter.left : parent.right
            anchors.bottom: parent.bottom
            visible: mainWin.visibility !== Window.FullScreen || forceShowBottomBar
            height: progressBar.height + playerCtrlBar.height + 1
            z:1
            color: "black"

            AZProgressBar{
                id:progressBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
            }

            AZPlayerCtrlBar{
                id:playerCtrlBar
                listView: fileTab
                splitView: splitView
                currentTime: progressBar.currentTime
                z:1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: progressBar.bottom
                anchors.topMargin: 1
                onOpenFileBtnClicked: fileDialog.openFile()
            }

        }


        Rectangle {
            id: splitter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: splitView.showed
            width: 4
            color: "black"

            MouseArea {
                id: tmp
                anchors.fill: parent
                drag.target: splitter
                drag.axis: Drag.XAxis
                drag.minimumX: splitView.leftMinWidth
                drag.maximumX: splitView.width - splitView.rightMinWidth - width
                cursorShape: Qt.SplitHCursor
                drag.smoothed:false

                // 拖动时保存右侧宽度
                onPositionChanged: {
                    if (drag.active) {
                        splitView._rightRectWidth = splitView.width - (splitter.x + splitter.width);
                        // 确保右侧宽度不小于最小值
                        splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);
                    }
                }
            }
        }

        ColumnLayout {
            id:fileListBar
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: parent.top
            anchors.left: splitter.right
            visible: splitView.showed
            spacing: 1

            TabBar {
                id:tabBar
                Layout.fillWidth: true
                background: Rectangle{
                    color:"black"
                }
                position:TabBar.Header

                Repeater {
                    model: ListModel {
                        ListElement { label: "文件" }
                        ListElement { label: "字幕" }
                        ListElement { label: "音频" }
                    }

                    TabButton {
                        id: tabBtn
                        background: Rectangle {
                            color: (tabBtn.pressed || tabBtn.checked) ? "#3b3b3b" : "#333333"
                        }
                        contentItem: Text {
                            text: label
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: tabBtn.checked ? "white" : "#747474"
                        }
                    }
                }

            }

            StackLayout {
                id: stackView
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabBar.currentIndex

                ColumnLayout{
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 1
                    AZListView{
                        id: fileTab
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: "#3b3b3b"
                        listModel: fileDialog.listModel

                        onDelActive:{
                            textWrapper.text = ""
                            MediaCtrl.close()
                        }

                        onStopActive: {
                            textWrapper.text = ""
                            MediaCtrl.close()
                        }

                        Connections {
                            target: fileDialog
                            function onOpenIndex(index) {
                                fileTab.setHighlightIdx(index)
                                textWrapper.text = fileDialog.listModel.get(index).text
                                MediaCtrl.open(fileDialog.activeFilePath)
                            }
                        }

                        onOpenIndex:function(index){
                            textWrapper.text = fileDialog.listModel.get(index).text
                            MediaCtrl.open(fileDialog.listModel.get(index).fileUrl)
                        }
                    }
                    Rectangle{
                        Layout.fillWidth: true
                        Layout.minimumHeight: 30
                        Layout.maximumHeight: 30
                        color: "#323232"
                        Button{
                            id:addFileBtn
                            width: 60
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.topMargin: 3
                            anchors.bottomMargin: 3
                            anchors.leftMargin: 3
                            background: Rectangle {
                                anchors.fill: parent
                                color: addFileBtn.pressed ? "#424242" : "#5a5a5a"
                                radius: 3
                            }

                            contentItem: Text {
                                text: "添加文件"
                                color: "white"
                                font.pointSize: 8
                                anchors.centerIn: parent
                            }
                            onClicked: fileDialog.addFile()
                        }
                    }
                }

                AZStreamView {
                    id: subtitleTab
                    streamType: "SUBTITLE"
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                AZStreamView {
                    id: audioTab
                    streamType: "AUDIO"
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
            }

        }

        onWidthChanged: {
            // 确保右侧宽度不小于最小值
            splitView._rightRectWidth = Math.max(splitView.rightMinWidth, splitView._rightRectWidth);

            // 检查是否需要调整右侧宽度以适应新的窗口大小
            var maxPossibleRightWidth = splitView.width - splitView.leftMinWidth - splitter.width;
            if (splitView._rightRectWidth > maxPossibleRightWidth) {
                splitView._rightRectWidth = maxPossibleRightWidth;
            }

            // 更新分割条位置
            splitter.x = splitView.width - splitView._rightRectWidth - splitter.width;
        }
    }

    AZFileDialog{
        id:fileDialog
    }
}

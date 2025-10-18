import QtQuick
import QtQuick.Layouts
/*TODO: 目前Qt|Windows从FullScreen到Maximized的实际路线是FullScreen->Windowed->Maximized
        从Minimized到Maximized或FullScreen（这儿指从dock栏点击图标从最小化还原到之前的状态）的实际路线都是Minimized->Windowed
        且直接使用窗口自带的控件没有上述问题
        因此窗口切换逻辑暂且采用一下的简单逻辑
*/
Rectangle{
    id: root

    // 外部传入的窗口
    property Window targetWindow: null

    // 按钮统一样式
    property int iconWidth: 16
    property int iconHeight: 16

    Connections {
        target: targetWindow
        function onVisibilityChanged() {
            let tmp = targetWindow.visibility

            if(tmp == Window.Maximized ){
                fullScreenBtn.tooltipText = "全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen.png"

                maximizeBtn.tooltipText = "恢复"
                maximizeBtn.iconSource = "qrc:/icon/restore.png"
            }else if(tmp == Window.FullScreen){
                fullScreenBtn.tooltipText = "退出全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen_exit.png"

                maximizeBtn.tooltipText = "恢复"
                maximizeBtn.iconSource = "qrc:/icon/restore.png"
            }else if(tmp == Window.Windowed){
                maximizeBtn.tooltipText = "最大化"
                maximizeBtn.iconSource = "qrc:/icon/maximize.png"

                fullScreenBtn.tooltipText = "全屏"
                fullScreenBtn.iconSource = "qrc:/icon/fullscreen.png"
            }
        }
    }

    RowLayout {
        id: controlBar
        anchors.fill: parent
        spacing: 0

        function toggleFullScreen(){
            let nowVis = targetWindow.visibility
            if(nowVis == Window.FullScreen){
                targetWindow.showNormal()
            }else{
                targetWindow.showFullScreen()
            }
        }

        function toggleMaximized(){
            let nowVis = targetWindow.visibility
            if(nowVis == Window.Windowed){
                targetWindow.showMaximized()
            }else if(nowVis == Window.FullScreen || nowVis == Window.Maximized){
                targetWindow.showNormal()
            }
        }

        AZButton {
            id: minimizeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/minimize.png"
            tooltipText: "最小化"
            onLeftClicked:targetWindow.showMinimized();
        }

        AZButton {
            id: maximizeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/maximize.png"
            tooltipText: "最大化"
            onLeftClicked: controlBar.toggleMaximized()
        }

        AZButton {
            id: fullScreenBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/fullscreen.png"
            tooltipText: "全屏"
            onLeftClicked: controlBar.toggleFullScreen()
        }

        AZButton {
            id: closeBtn
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconWidth: root.iconWidth
            iconHeight: root.iconHeight
            iconSource: "qrc:/icon/close.png"
            tooltipText: "关闭"
            onLeftClicked:targetWindow.close();
        }
    }
}

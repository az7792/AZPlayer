import QtQuick 2.15
import QtQuick.Window 2.15

Rectangle{
    id: topBar

    property Window mainWindow: null

    function setTextWrapper(str){
        titleText.text = str
    }

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
        MouseArea {
            anchors.fill: parent
            onPressed: mainWindow.startSystemMove()
            onDoubleClicked: mainWindow.visibility = mainWindow.visibility == Window.Windowed ? Window.Maximized : Window.Windowed
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
        targetWindow: mainWindow
        height: parent.height
        anchors.right: parent.right
        width: 120
    }
}

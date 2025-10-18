import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 120
    height: 40
    color: defaultColor

    property alias text: label.text            // 可选文字标签
    property string iconSource: ""             // 第一个图标
    property int iconWidth: width * 0.6         // 图片宽度
    property int iconHeight: height * 0.6       // 图片高度
    property color defaultColor: "#1c1c1c"      // 默认背景色
    property color hoverColor: "#252525"        // 鼠标悬浮颜色
    property color pressedColor: "#161616"      // 按下颜色
    property string tooltipText: ""             // 鼠标悬浮显示的提示

    signal clicked       //左键或右键点击
    signal leftClicked   //左键点击
    signal rightClicked  //右键点击

    // 图标
    Image {
        id: icon
        source: iconSource
        anchors.centerIn: parent
        visible: source !== ""
        fillMode: Image.PreserveAspectFit
        width: iconWidth
        height: iconHeight
    }

    // 文字
    Text {
        id: label
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 16
        visible: text !== ""
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property bool leftPressed: false
        property bool rightPressed: false

        onPressed: {
            var retL = pressedButtons & Qt.LeftButton;
            var retR = pressedButtons & Qt.RightButton;
            leftPressed = retL ? true : false
            rightPressed = retR ? true : false
            root.color = pressedColor
        }

        onReleased: {
            // 仅在按下和释放都在区域内才触发信号
            var retL = pressedButtons & Qt.LeftButton;
            var retR = pressedButtons & Qt.RightButton;

            if(containsMouse && leftPressed && !retL){
                root.leftClicked()
                root.clicked()
            }

            if(containsMouse && rightPressed && !retR) {
                root.rightClicked()
                root.clicked()
            }

            if(!retL) leftPressed = false
            if(!retR) rightPressed = false

            root.color = pressed ? pressedColor : (containsMouse ? hoverColor: defaultColor)
        }

        onEntered: {
            if (!pressed) root.color = hoverColor
            if (tooltipText !== "") {
                let globalPos = root.mapToItem(root.window, 0, 0)                
                AZTooltip.show(tooltipText,mouseX + globalPos.x, mouseY + globalPos.y)
            }
        }

        onExited: {
            if (!pressed) root.color = defaultColor
            AZTooltip.hide()
        }
        onPositionChanged: {
            if (containsMouse && tooltipText !== "") {
                let globalPos = root.mapToItem(root.window, 0, 0)
                AZTooltip.show(tooltipText,mouseX + globalPos.x, mouseY + globalPos.y)
            }
        }
    }

}

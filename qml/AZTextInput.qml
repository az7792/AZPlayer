import QtQuick
import QtQuick.Controls

Item {
    id: root
    property alias text: input.text           // 外部可直接访问文本
    property string labelText: ""             // 标签文字
    property alias validator:input.validator  // 外部可以设置 Validator
    signal wheelUp()                          // 滚轮向上信号
    signal wheelDown()                        // 滚轮向下信号
    signal editingFinished()

    // 左边标签
    Text {
        id: label
        text: root.labelText
        font.family: "Segoe UI"
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        color: "white"
        width: implicitWidth
    }

    // 右边输入框
    Rectangle {
        id: inputBg
        height: parent.height
        color: "#454545"
        border.color: input.focus ? "#a1cafe" : "#626262"
        border.width: 1
        anchors.left: label.right
        anchors.leftMargin: 4
        anchors.right: parent.right

        // 鼠标悬浮改变鼠标样式
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor
            onWheel: function(wheel) {
                if (input.focus) {
                    if (wheel.angleDelta.y > 0)
                        root.wheelUp()
                    else if (wheel.angleDelta.y < 0)
                        root.wheelDown()
                }
            }
        }

        // 实际文本框
        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 4
            font.family: "Segoe UI"
            font.pixelSize: 12
            color: "white"
            onEditingFinished: root.editingFinished()
        }
    }
}

import QtQuick

//TODO: 超出显示区域的会被裁掉
Rectangle {
    id: overlay
    visible: false
    z: 9999
    color: "#f9f9f9"
    radius: 3

    property Window targetWindow: null
    property alias text: tipLabel.text

    Text {
        id: tipLabel
        anchors.centerIn: parent
        color: "black"
        font.pixelSize: 12
    }

    width: tipLabel.width + 10
    height: tipLabel.height + 6

    function show(msg, px, py) {
        text = msg
        px += 8
        py += 16
        if(!targetWindow){
            x = px
            y = py
            visible = true
            return
        }

        // 获取窗口宽高
        var winWidth = targetWindow.width
        var winHeight = targetWindow.height

        // 修正 x 坐标，确保 tooltip 不超出右边界
        if (px + width > winWidth)
            x = winWidth - width
        else if (px < 0)
            x = 0
        else
            x = px

        // 修正 y 坐标，确保 tooltip 不超出下边界
        if (py + height > winHeight)
            y = winHeight - height
        else if (py < 0)
            y = 0
        else
            y = py
        visible = true
    }

    function hide() {
        visible = false
    }
}

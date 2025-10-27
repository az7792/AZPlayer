import QtQuick
import QtQuick.Controls
Slider {
    id: control
    hoverEnabled: true

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 2
        width: control.availableWidth
        height: implicitHeight
        radius: width / 2
        color: "black"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            color: "#a1cafe"
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 10
        implicitHeight: 10
        radius: width / 2
        color: control.hovered ? "#a1cafe" : "#ebebeb";
    }
}

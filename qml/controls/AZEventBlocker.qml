import QtQuick

MouseArea {
    id: blocker
    hoverEnabled: true
    acceptedButtons: Qt.AllButtons
    onWheel: (wheel) => wheel.accepted = true
}

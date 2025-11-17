import QtQuick
import QtQuick.Layouts
import "../controls"
import "../fileDialog"

ColumnLayout{
    property AZFileDialog fileDialog:null

    AZStreamView {
        id: subtitleTab
        streamType: "SUBTITLE"
        Layout.fillHeight: true
        Layout.fillWidth: true
    }
    Rectangle{
        Layout.fillWidth: true
        Layout.minimumHeight: 30
        Layout.maximumHeight: 30
        color: "#323232"
        AZTextButton{
            id:addSubtitleBtn
            width: 60
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            anchors.leftMargin: 3
            text: "添加字幕"
            onClicked: fileDialog.openSubtitleStreamFile()
        }
    }
}

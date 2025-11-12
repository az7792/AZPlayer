import QtQuick
import QtQuick.Layouts
import VideoWindow 1.0
import "../controls"
import "../fileDialog"

ColumnLayout{
    property AZFileDialog fileDialog:null
    property VideoWindow videoWindow: null

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

        AZTextButton{
            id:showSubtitleBtn
            property bool showSub: true
            width: 60
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: addSubtitleBtn.right
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            anchors.leftMargin: 3
            text: showSub ? "关闭字幕" : "显示字幕"
            onClicked: {
                showSub = !showSub
                videoWindow.setShowSubtitle(showSub)
            }
        }
    }
}

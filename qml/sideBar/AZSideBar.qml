import QtQuick
import QtQuick.Layouts
import VideoWindow 1.0
import "../controls"
import "../fileDialog"
import "../topBar"
Rectangle {
    id: sideBar
    property alias fileListView: fileTab.fileListView
    property bool isActive: false

    AZEventBlocker{anchors.fill: parent}

    ColumnLayout {
        anchors.fill: parent
        spacing: 1
        AZTabBar{
            id:tabBar
            Layout.fillWidth: true
            model: ListModel {
                ListElement { label: "文件" }
                ListElement { label: "字幕" }
                ListElement { label: "音频" }
            }
        }

        StackLayout {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            AZFileTab{
                id: fileTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }

            AZSubtitleTab{
                id: subtitleTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }

            AZAudioTab{
                id: audioTab
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1
            }
        }
    }
}


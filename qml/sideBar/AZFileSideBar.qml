import QtQuick
import QtQuick.Layouts
import VideoWindow 1.0
import "../controls"
import "../fileDialog"
import "../topBar"
ColumnLayout {
    id:root
    property AZFileDialog fileDialog:null
    property AZTopBar topBar: null
    property VideoWindow videoWindow: null
    property alias fileTab: fileTab.fileTab

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
            fileDialog: root.fileDialog
            topBar: root.topBar
        }

        AZSubtitleTab{
            id: subtitleTab
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 1
            fileDialog: root.fileDialog
        }

        AZAudioTab{
            id: audioTab
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 1
            fileDialog: root.fileDialog
        }
    }
}

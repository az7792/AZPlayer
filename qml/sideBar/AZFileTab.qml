import QtQuick
import QtQuick.Layouts
import "../controls"
import "../fileDialog"
import "../topBar"

ColumnLayout{
    property AZFileDialog fileDialog:null
    property AZTopBar topBar: null
    property alias fileTab: fileTab


    AZListView{
        id: fileTab
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: "#3b3b3b"
        listModel: fileDialog.listModel

        onDelActive:{
            topBar.setTextWrapper("")
            MediaCtrl.close()
        }

        onStopActive: {
            topBar.setTextWrapper("")
            MediaCtrl.close()
        }

        Connections {
            target: fileDialog
            function onOpenIndex(index) {
                fileTab.setHighlightIdx(index)
                topBar.setTextWrapper(fileDialog.listModel.get(index).text)
                MediaCtrl.open(fileDialog.activeFilePath)
            }
        }

        onOpenIndex:function(index){
            topBar.setTextWrapper(fileDialog.listModel.get(index).text)
            MediaCtrl.open(fileDialog.listModel.get(index).fileUrl)
        }
    }

    Rectangle{
        Layout.fillWidth: true
        Layout.minimumHeight: 30
        Layout.maximumHeight: 30
        color: "#323232"
        AZTextButton{
            id:addFileBtn
            width: 60
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            anchors.leftMargin: 3
            text: "添加文件"
            onClicked: fileDialog.addFile()
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel
import QtQuick.Dialogs

Rectangle {
    id:root

    signal openIndex(int index)

    property url activeFilePath: ""
    property alias listModel: listModel
    // 是否允许加载
    property bool allowFolderLoad: false

    function openFile(){
        fileDialogSingle.open()
    }

    function addFile(){
        fileDialogMulti.open()
    }

    function openSubtitleStreamFile(){
        subtitleStreamDialog.open()
    }

    function openAudioStreamFile(){
        audioStreamDialog.open()
    }

    ListModel {
        id: listModel

        function existsInModel(fileUrl){
            let lower = fileUrl.toString().toLowerCase()
            for (let i = 0; i < count; ++i) {
                if (listModel.get(i).fileUrl.toString().toLowerCase() === lower)
                    return true
            }
            return false
        }
    }


    // 临时 FolderListModel 用于扫描某个文件夹里的文件（按后缀过滤）
    FolderListModel {
        id: folderModel
        caseSensitive: false
        showDirs:false
        onStatusChanged: {
            if (status === FolderListModel.Ready && root.allowFolderLoad) {
                listModel.clear()

                let i;
                for (i = 0; i < count; ++i) {
                    listModel.append({text: folderModel.get(i, "fileName"),
                                      fileUrl: folderModel.get(i, "fileUrl")})
                }
                root.allowFolderLoad = false;

                //更新播放索引
                for(i = 0; i < listModel.count ;++i){
                    if(listModel.get(i).fileUrl.toString().toLowerCase() === root.activeFilePath.toString().toLowerCase()){
                        openIndex(i)
                        break
                    }
                }
            }
        }
    }


    // 单选对话框
    FileDialog {
        id: fileDialogSingle
        title: "选择一个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFile
        nameFilters: ["媒体文件 (*.mp4 *.mkv *.avi *.mp3 *.aac *.mka)", "所有文件 (*)"]
        onAccepted: {
            if (!selectedFile) return
            root.activeFilePath = selectedFile

            var parts = selectedFile.toString().split('.')

            if(parts.length < 2){
                console.log("无后缀")
                return
            }

            var suffix = '.' + parts.pop()

            root.allowFolderLoad = true;
            folderModel.folder = ""//先清空地址防止加载两次
            folderModel.nameFilters = ["*" + suffix]
            folderModel.folder = currentFolder
        }
        onRejected: { console.log("[FileDialogSingle] 取消") }
    }

    // 多选对话框
    FileDialog {
        id: fileDialogMulti
        title: "选择一个或多个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i) {
                var fileName = selectedFiles[i].toString().split(/[\\/]/).pop()
                if (!listModel.existsInModel(selectedFiles[i])) {
                    listModel.append({text: fileName, fileUrl:selectedFiles[i]})
                }
            }
        }
        onRejected: { console.log("[FileDialogMulti] 取消") }
    }

    // 打开字幕文件对话框
    FileDialog {
        id: subtitleStreamDialog
        title: "选择字幕文件"
        acceptLabel: "打开"
        rejectLabel: "取消"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "字幕文件 (*.srt *.ass *.ssa *.sub *.vtt)",
            "所有文件 (*)"
        ]
        onAccepted: {
            if (!selectedFile) return
            MediaCtrl.openSubtitleStream(selectedFile)
        }

        onRejected: {
            console.log("[SubtitleDialog] 用户取消选择")
        }
    }

    // 打开音轨文件对话框
    FileDialog {
        id: audioStreamDialog
        title: "选择音轨文件"
        acceptLabel: "打开"
        rejectLabel: "取消"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "音频文件 (*.mp3 *.aac *.flac *.mka *.wav *.ogg *.ac3)",
            "所有文件 (*)"
        ]
        onAccepted: {
            if (!selectedFile) return
            MediaCtrl.openAudioStream(selectedFile)
        }
        onRejected: {
            console.log("[AudioTrackDialog] 用户取消选择")
        }
    }
}

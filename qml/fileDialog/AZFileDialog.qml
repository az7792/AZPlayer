// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel
import QtQuick.Dialogs

Rectangle {
    id:root

    signal openMediaByIdx(int index)

    property string activeFilePath: ""
    property alias mediaListModel: medialist
    // 是否允许加载
    property bool allowFolderLoad: false

    function openMediaFile(){
        openMediafileDialog.open()
    }

    function onStartupFiles(files){
        medialist.clear()
        for (var i = 0; i < files.length; ++i) {
            // var fileUrl = files[i].fileUrl
            // var name = files[i].text
            // console.log("Open file:", fileUrl,name)
            medialist.append(files[i])
        }
        if (files.length > 0) {
            root.activeFilePath = files[0].fileUrl
            openMediaByIdx(0)
        }
    }

    function addMediaFile(){
        addMediafileDialog.open()
    }

    function openSubtitleStreamFile(){
        subtitleStreamDialog.open()
    }

    function openAudioStreamFile(){
        audioStreamDialog.open()
    }

    // 已加载的媒体文件
    ListModel {
        id: medialist

        function existsInModel(fileUrl){ //fileUrl 为string类型
            let lower = fileUrl.toLowerCase()
            for (let i = 0; i < count; ++i) {
                if (medialist.get(i).fileUrl.toLowerCase() === lower)
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
                medialist.clear()

                let i;
                for (i = 0; i < count; ++i) {
                    medialist.append({text: folderModel.get(i, "fileName").toString(),
                                      fileUrl: folderModel.get(i, "fileUrl").toString()})
                }
                root.allowFolderLoad = false;

                //更新播放索引
                for(i = 0; i < medialist.count ;++i){
                    if(medialist.get(i).fileUrl.toLowerCase() === root.activeFilePath.toLowerCase()){
                        openMediaByIdx(i)
                        break
                    }
                }
            }
        }
    }


    // 打开一个媒体文件，并刷新媒体文件列表
    FileDialog {
        id: openMediafileDialog
        title: "选择一个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFile
        nameFilters: ["媒体文件 (*.mp4 *.mkv *.avi *.mp3 *.aac *.mka)", "所有文件 (*)"]
        onAccepted: {
            if (!selectedFile) return
            root.activeFilePath = selectedFile.toString()

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

    // 添加单个或多个文件到媒体文件列表
    FileDialog {
        id: addMediafileDialog
        title: "选择一个或多个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i) {
                var fileName = selectedFiles[i].toString().split(/[\\/]/).pop()
                if (!medialist.existsInModel(selectedFiles[i].toString())) {
                    medialist.append({text: fileName, fileUrl:selectedFiles[i].toString()})
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

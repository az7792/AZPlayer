// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

Rectangle {
    id:root

    signal openMediaByIdx(int index)

    property string currentPlayingFile: "" // 当前播放的文件 fileName ，由 AZPlayerState 创建时绑定到 AZPlayerState.currentFile
    property string activeFilePath: ""     // 当前选中的文件路径 file:///....
    property alias mediaListModel: medialist

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

    // 关闭所有已打开的文件对话框
    function closeAllDialogs(){
        if (openMediafileDialog.visible) openMediafileDialog.close()
        if (addMediafileDialog.visible) addMediafileDialog.close()
        if (subtitleStreamDialog.visible) subtitleStreamDialog.close()
        if (audioStreamDialog.visible) audioStreamDialog.close()
    }

    // 已加载的媒体文件
    ListModel {
        id: medialist

        function existsInModel(fileUrl){ //fileUrl 为string类型
            // TODO: 可能会有性能问题
            let lower = fileUrl.toLowerCase()
            for (let i = 0; i < count; ++i) {
                if (medialist.get(i).fileUrl.toLowerCase() === lower)
                    return true
            }
            return false
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

            // 扫描所选文件所在目录下同后缀的文件，重建媒体文件列表
            const files = FileHelper.expandSiblingFiles(selectedFile)

            if (files.length === 0) {
                console.log("未找到同类型文件")
                return
            }

            medialist.clear()
            for (let i = 0; i < files.length; ++i)
                medialist.append(files[i])

            // 更新播放索引
            for (let i = 0; i < medialist.count; ++i) {
                if (medialist.get(i).fileUrl.toLowerCase() === root.activeFilePath.toLowerCase()) {
                    openMediaByIdx(i)
                    break
                }
            }
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
        title: "选择字幕文件 - 当前在正在播放:" + root.currentPlayingFile
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
        title: "选择音轨文件 - 当前在正在播放:" + root.currentPlayingFile
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

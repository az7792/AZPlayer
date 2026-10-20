// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs

Rectangle {
    id:root

    function openMediaFile(){
        openMediaDialog.open()
    }

    function addMediaFiles() {
        addMediaDialog.open()
    }

    function openSubtitleStreamFile(){
        subtitleStreamDialog.open()
    }

    function openAudioStreamFile(){
        audioStreamDialog.open()
    }

    // 关闭所有已打开的文件对话框
    function closeAllDialogs(){
        if (openMediaDialog.visible) openMediaDialog.close()
        if (addMediaDialog.visible) addMediaDialog.close()
        if (subtitleStreamDialog.visible) subtitleStreamDialog.close()
        if (audioStreamDialog.visible) audioStreamDialog.close()
    }

    // 选择主媒体文件。
    FileDialog {
        id: openMediaDialog
        title: "选择一个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFile
        nameFilters: ["媒体文件 (*.mp4 *.mkv *.avi *.mp3 *.aac *.mka)", "所有文件 (*)"]
        onAccepted: {
            if (!selectedFile) return
            // 扫描所选文件所在目录下同后缀的文件，重建媒体文件列表
            const files = FileHelper.expandSiblingFiles(selectedFile)

            if (files.length === 0) {
                console.log("未找到同类型文件")
                return
            }

            AZMediaList.replaceAndOpen(files, selectedFile)
        }
        onRejected: { console.log("[FileDialogSingle] 取消") }
    }

    // 添加单个或多个媒体文件。
    FileDialog {
        id: addMediaDialog
        title: "选择一个或多个文件"
        acceptLabel:"打开"
        rejectLabel:"取消"
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            AZMediaList.appendUnique(selectedFiles)
        }
        onRejected: { console.log("[FileDialogMulti] 取消") }
    }

    // 打开字幕文件对话框
    FileDialog {
        id: subtitleStreamDialog
        title: "选择字幕文件 - 当前在正在播放:" + AZMediaList.currentFile
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
        title: "选择音轨文件 - 当前在正在播放:" + AZMediaList.currentFile
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

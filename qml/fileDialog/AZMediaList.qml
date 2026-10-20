// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick

// 集中管理播放列表的数据规范化及常用操作。
ListModel {
    id: root

    property int currentIndex: -1
    property string currentFile: ""

    // 主动打开媒体后发出（用于区分 currentIndex 是为什么变的）
    signal mediaOpened(int index)

    // 生成用于比较的规范化 URL。
    // HACK: Windows 文件是大小写不敏感的
    function normalizedUrl(fileUrl) {
        return String(fileUrl).toLowerCase()
    }

    // 从 URL 或路径中提取文件名
    function fileName(fileUrl) {
        return String(fileUrl).split(/[\\/]/).pop()
    }

    // 将输入转换为列表项数据
    function toEntry(file) {
        const fileUrl = file && file.fileUrl !== undefined ? file.fileUrl : file
        return {
            text: file && file.text !== undefined ? file.text : fileName(fileUrl),
            fileUrl: String(fileUrl)
        }
    }

    // 查找 URL 对应的列表索引
    function indexOfUrl(fileUrl) {
        const target = normalizedUrl(fileUrl)
        for (let i = 0; i < count; ++i) {
            if (normalizedUrl(get(i).fileUrl) === target)
                return i
        }
        return -1
    }

    // 去重并添加到列表
    function appendUnique(files) {
        const knownUrls = Object.create(null)
        for (let i = 0; i < count; ++i)
            knownUrls[normalizedUrl(get(i).fileUrl)] = true

        for (let idx = 0; idx < files.length; ++idx) {
            const entry = toEntry(files[idx])
            const url = normalizedUrl(entry.fileUrl)
            if (!knownUrls[url]) {
                append(entry)
                knownUrls[url] = true
            }
        }
    }

    // 清空后替换为新文件列表
    function replace(files) {
        clear()
        appendUnique(files)
    }

    // 打开指定索引的媒体
    function open(index) {
        if (index < 0 || index >= count)
            return

        const media = get(index)
        currentIndex = index
        currentFile = media.text
        MediaCtrl.open(media.fileUrl)
        mediaOpened(index)
    }

    // 替换列表并打开指定文件
    function replaceAndOpen(files, selectedFile) {
        replace(files)
        if (count === 0)
            return

        const index = selectedFile === undefined ? 0 : indexOfUrl(selectedFile)
        open(index === -1 ? 0 : index)
    }

    // 循环打开下一项
    function openNext() {
        if (currentIndex === -1 || count === 0)
            return
        open((currentIndex + 1) % count)
    }

    // 循环打开上一项
    function openPrev() {
        if (currentIndex === -1 || count === 0)
            return
        open((currentIndex - 1 + count) % count)
    }

    // 随机打开另一项
    function openRandom() {
        if (currentIndex === -1 || count === 0)
            return

        if (count === 1) {
            open(0)
            return
        }

        const index = Math.floor(Math.random() * (count - 1))
        open(index < currentIndex ? index : index + 1)
    }

    // 删除指定项，并同步当前播放索引。
    function removeMedia(index) {
        if (index < 0 || index >= count)
            return

        const isCurrentMedia = index === currentIndex
        remove(index)

        if (isCurrentMedia) {
            close()
        } else if (index < currentIndex) {
            --currentIndex
        }
    }

    // 关闭当前媒体并清除播放状态
    function close() {
        currentIndex = -1
        currentFile = ""
        MediaCtrl.close()
    }
}

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

Text {
    id: statsText
    width: 400
    height: 450
    textFormat: Text.RichText
    text: ""
    wrapMode: Text.Wrap
    style: Text.Outline
    styleColor: "black"

    // ==== 左上对齐 ====
    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignTop

    Timer {
        id: refreshTimer
        interval: 1000
        running: false
        repeat: true
        onTriggered: {
            statsText.text = PlaybackStats.getPlaybackStatsStringHTML()
        }
    }

    // 显示
    function showStats() {
        statsText.text = PlaybackStats.getPlaybackStatsStringHTML()
        refreshTimer.start()
    }

    // 隐藏
    function hideStats() {
        refreshTimer.stop()
        statsText.text = ""
    }
}

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtCore

Settings {
    property real volume: 1.0          // 音量
    property bool muted: false         // 静音
    property bool autoLoadExtSub: true // 自动加载外部字幕
}

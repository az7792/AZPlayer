// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick 2.15

Item {
    property real videoX: 0                    // 视频x
    property real videoY: 0                    // 视频y
    property real videoAngle:0.0               // 视频顺时针旋转(角度)
    property int  videoScale:100               // 缩放(100为不缩放)
    property int  videoScaleMode:0             // 缩放策略(0适应 1拉伸 2裁剪)
    property int  videoAspectMode:0            // 显示比例(0默认 1:1 2:4:3 3:16:9)
    property bool videoShowSubtitle: true      // 是否显示字幕
    property bool videoHorizontalMirror: false // 水平镜像
    property bool videoVerticalMirror: false   // 垂直镜像
    property bool videoInvert: false           // 反色
    property bool videoGrayscale: false        // 黑白

    // 持久化设置
    property bool windowAutoFitVideo: true     // 窗口自适应视频分辨率
}

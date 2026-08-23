// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton
import QtQuick

QtObject {
    id: root

    // C++ SettingManager，持久化到 程序目录/settings/setting.json
    readonly property var store: Setting

    // 设置键常量
    readonly property string keyVolume: "volume"
    readonly property string keyMuted: "muted"
    readonly property string keyAutoLoadExtSub: "autoLoadExtSub"
    readonly property string keyAudioDevice: "audioDevice"

    property real volume: store.doubleValue(keyVolume, 1.0)                // 音量
    property bool muted: store.boolValue(keyMuted, false)                  // 静音
    property bool autoLoadExtSub: store.boolValue(keyAutoLoadExtSub, true) // 自动加载外部字幕
    property string audioDevice: store.stringValue(keyAudioDevice, "")     // 音频输出设备(base64)，空 = 跟随系统

    onVolumeChanged: store.setValue(keyVolume, volume)
    onMutedChanged: store.setValue(keyMuted, muted)
    onAutoLoadExtSubChanged: store.setValue(keyAutoLoadExtSub, autoLoadExtSub)
    onAudioDeviceChanged: store.setValue(keyAudioDevice, audioDevice)

    // 启动时恢复上次保存的输出设备
    Component.onCompleted: {
        if (root.audioDevice !== "") AudioDevices.restoreSelection(root.audioDevice)
    }

    // value 为空（remove() 或非法 JSON）时回退到默认值
    function resolveValue(value, defaultValue) {
        return (value === null || value === undefined) ? defaultValue : value
    }

    // 外部通过 C++ 或其他文件写入时同步到本单例
    // QtObject 没有默认属性，Connections 必须挂到具名属性上
    property Connections storeConnections: Connections {
        target: store
        function onValueChanged(key, value) {
            if (key === root.keyVolume) root.volume = resolveValue(value, 1.0)
            else if (key === root.keyMuted) root.muted = resolveValue(value, false)
            else if (key === root.keyAutoLoadExtSub) root.autoLoadExtSub = resolveValue(value, true)
            else if (key === root.keyAudioDevice) root.audioDevice = resolveValue(value, "")
        }
    }

    // 设备选择变化时持久化（含自动回退到跟随系统）
    property Connections audioDeviceConn: Connections {
        target: AudioDevices
        function onSelectionChanged() {
            root.audioDevice = AudioDevices.selectedSerialized()
        }
    }
}

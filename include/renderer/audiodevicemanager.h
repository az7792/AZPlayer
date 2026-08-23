// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIODEVICEMANAGER_H
#define AUDIODEVICEMANAGER_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <vector>

struct ma_context;

/**
 * 音频输出设备管理器（单例）。
 *
 * - 持有常驻 ma_context，负责枚举播放设备；
 * - 管理"跟随系统"(-1) / 指定播放设备两种模式；
 * - 当所选"非跟随系统"设备被断开时，自动回退到"跟随系统"；
 * - 通过 selectedSerialized() 与外部（QML 设置界面 / AudioPlayer）交换选择。
 */
class AudioDeviceManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioDeviceManager)
public:
    static AudioDeviceManager &instance();

    // 播放设备列表 [{name, isDefault}]，每次调用都会重新枚举
    Q_INVOKABLE QVariantList playbackDevices();

    // 刷新设备列表（供 UI 手动刷新/监听用）
    Q_INVOKABLE void refresh();

    // 当前选中设备索引，-1 = 跟随系统
    Q_INVOKABLE int selectedIndex() const;

    // 切换到指定设备，-1 = 跟随系统
    Q_INVOKABLE void selectDeviceByIndex(int index);

    // 当前所选设备的原始 id 字节；空 = 跟随系统
    QByteArray selectedIdBytes() const;

    // 当前所选设备的序列化值（base64）；空串 = 跟随系统
    Q_INVOKABLE QString selectedSerialized() const;

    // 从持久化的序列化值恢复选择；设备不存在时回退到跟随系统
    Q_INVOKABLE void restoreSelection(const QString &serialized);

    // 懒初始化并返回常驻 context（进程级，供 AudioPlayer 使用）
    ma_context *context();

signals:
    void devicesChanged();    // 设备列表变化（含自动回退时）
    void selectionChanged();  // 当前选择变化

private:
    explicit AudioDeviceManager(QObject *parent = nullptr);
    ~AudioDeviceManager() override;

    void ensureMaContext();
    void enumerate();
    void monitor(); // 定时检查当前所选设备是否还存在

    struct DeviceEntry {
        QString name;
        QByteArray idBytes; // 原始 ma_device_id 字节
        bool isDefault = false;
    };
    std::vector<DeviceEntry> m_devices;
    int m_selectedIndex = -1;      // -1 = 跟随系统
    QByteArray m_selectedIdBytes;  // 当前所选设备的原始 id
    ma_context *m_maContext = nullptr;
    QTimer m_monitorTimer;
};

#endif // AUDIODEVICEMANAGER_H
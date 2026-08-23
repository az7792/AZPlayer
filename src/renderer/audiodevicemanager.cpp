// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "renderer/audiodevicemanager.h"
#include "3rd/miniaudio/miniaudio.h"
#include <QDebug>
#include <cstring>
#include <utility>

AudioDeviceManager &AudioDeviceManager::instance() {
    static AudioDeviceManager s_instance;
    return s_instance;
}

AudioDeviceManager::AudioDeviceManager(QObject *parent)
    : QObject{parent} {
    m_monitorTimer.setInterval(1000); // 每秒检查一次所选设备是否仍存在
    connect(&m_monitorTimer, &QTimer::timeout, this, [this]() { monitor(); });
}

AudioDeviceManager::~AudioDeviceManager() {
    m_monitorTimer.stop();
    if (m_maContext) {
        ma_context_uninit(m_maContext);
        delete m_maContext;
    }
}

ma_context *AudioDeviceManager::context() {
    ensureMaContext();
    return m_maContext;
}

void AudioDeviceManager::ensureMaContext() {
    if (m_maContext) {
        return;
    }
    auto *ctx = new ma_context{};
    if (ma_context_init(NULL, 0, NULL, ctx) != MA_SUCCESS) {
        qDebug() << "AudioDeviceManager: ma_context_init 失败";
        delete ctx;
        return;
    }
    m_maContext = ctx;
}

void AudioDeviceManager::enumerate() {
    ensureMaContext();
    std::vector<DeviceEntry> newDevices;
    if (m_maContext) {
        ma_device_info *pPlayback = nullptr;
        ma_uint32 playbackCount = 0;
        if (ma_context_get_devices(m_maContext, &pPlayback, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
            newDevices.reserve(playbackCount);
            for (ma_uint32 i = 0; i < playbackCount; ++i) {
                DeviceEntry entry;
                entry.name = QString::fromUtf8(pPlayback[i].name);
                entry.isDefault = (pPlayback[i].isDefault != MA_FALSE);
                entry.idBytes.resize(sizeof(ma_device_id));
                memcpy(entry.idBytes.data(), &pPlayback[i].id, sizeof(ma_device_id));
                newDevices.push_back(std::move(entry));
            }
        }
    }

    bool changed = newDevices.size() != m_devices.size();
    if (!changed) {
        for (size_t i = 0; i < m_devices.size(); ++i) {
            if (m_devices[i].name != newDevices[i].name ||
                m_devices[i].isDefault != newDevices[i].isDefault ||
                m_devices[i].idBytes != newDevices[i].idBytes) {
                changed = true;
                break;
            }
        }
    }
    m_devices = std::move(newDevices);
    if (changed) {
        emit devicesChanged();
    }
}

QVariantList AudioDeviceManager::playbackDevices() {
    enumerate();
    QVariantList list;
    for (const auto &dev : m_devices) {
        QVariantMap item;
        item["name"] = dev.name;
        item["isDefault"] = dev.isDefault;
        list.append(item);
    }
    return list;
}

void AudioDeviceManager::refresh() {
    enumerate();
}

int AudioDeviceManager::selectedIndex() const {
    return m_selectedIndex;
}

void AudioDeviceManager::selectDeviceByIndex(int index) {
    if (index < -1 || index >= static_cast<int>(m_devices.size())) {
        if (index >= 0 && m_devices.empty()) {
            enumerate(); // 列表尚未枚举时先枚举再判断
        }
        if (index < -1 || index >= static_cast<int>(m_devices.size())) {
            return;
        }
    }
    if (index == m_selectedIndex) {
        return;
    }

    m_selectedIndex = index;
    if (index == -1) {
        m_selectedIdBytes.clear();
        m_monitorTimer.stop();
    } else {
        m_selectedIdBytes = m_devices[static_cast<size_t>(index)].idBytes;
        m_monitorTimer.start();
    }
    emit selectionChanged();
}

QByteArray AudioDeviceManager::selectedIdBytes() const {
    return m_selectedIdBytes;
}

QString AudioDeviceManager::selectedSerialized() const {
    if (m_selectedIdBytes.isEmpty()) {
        return {};
    }
    return QString::fromLatin1(m_selectedIdBytes.toBase64());
}

void AudioDeviceManager::restoreSelection(const QString &serialized) {
    if (serialized.isEmpty()) {
        selectDeviceByIndex(-1);
        return;
    }
    const QByteArray bytes = QByteArray::fromBase64(serialized.toLatin1());
    if (bytes.isEmpty()) {
        selectDeviceByIndex(-1);
        return;
    }

    if (m_devices.empty()) {
        enumerate();
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].idBytes == bytes) {
            selectDeviceByIndex(static_cast<int>(i));
            return;
        }
    }
    // 设备已不存在，回退到跟随系统
    qDebug() << "AudioDeviceManager: 保存的音频设备已不存在，回退到系统默认设备";
    selectDeviceByIndex(-1);
}

void AudioDeviceManager::monitor() {
    if (m_selectedIndex == -1) {
        return;
    }
    enumerate();

    const QByteArray selected = m_selectedIdBytes;
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].idBytes == selected) {
            if (m_selectedIndex != static_cast<int>(i)) {
                m_selectedIndex = static_cast<int>(i); // 设备列表重排，同步索引
                emit selectionChanged();
            }
            return;
        }
    }
    // 所选设备被断开，回退到跟随系统
    qDebug() << "AudioDeviceManager: 音频输出设备已断开，回退到系统默认设备";
    selectDeviceByIndex(-1);
}
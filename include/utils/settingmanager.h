// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGMANAGER_H
#define SETTINGMANAGER_H

#include "types/types.h"
#include <QObject>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <unordered_map>

/**
 * JSON 持久化设置管理器。
 *
 * - 数据以 JSON 文件存储，固定位于程序所在目录的 settings/ 文件夹下；
 * - 每个实例对应一个设置文件 settings/<fileName>.json，通过文件名创建：
 *     SettingManager window("window");   // 管理 settings/window.json
 */
class SettingManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingManager)
public:
    /**
     * 构造一个设置管理器。
     * @param fileName 设置文件名，可带或不带 ".json" 后缀
     */
    explicit SettingManager(const QString &fileName, QObject *parent = nullptr);
    ~SettingManager() override;

    // settings 文件夹路径（程序所在目录/settings）
    [[nodiscard]] static QString settingsDir();

    // 当前设置的文件名（不含路径与 .json 后缀）
    [[nodiscard]] QString fileName() const;

    // 切换设置文件（重新加载对应 JSON）
    void setFileName(const QString &fileName);

    // 读取 key 对应的值，不存在时返回 defaultValue
    Q_INVOKABLE [[nodiscard]] QVariant value(const QString &key, const QVariant &defaultValue) const;

    // 写入 key 对应的值并自动保存
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);

    // 判断 key 是否存在
    Q_INVOKABLE [[nodiscard]] bool contains(const QString &key) const;

    // 删除 key
    Q_INVOKABLE void remove(const QString &key);

    // 清空当前文件的所有设置
    Q_INVOKABLE void clear();

    // 立即写盘
    Q_INVOKABLE void sync();
public:
    Q_INVOKABLE [[nodiscard]] QString stringValue(const QString &key, const QString &defaultValue = {}) const;
    Q_INVOKABLE [[nodiscard]] int intValue(const QString &key, int defaultValue = 0) const;
    Q_INVOKABLE [[nodiscard]] bool boolValue(const QString &key, bool defaultValue = false) const;
    Q_INVOKABLE [[nodiscard]] double doubleValue(const QString &key, double defaultValue = 0.0) const;
signals:
    // key 对应的值发生变化
    void valueChanged(const QString &key, const QVariant &value);

private:
    // 规范化文件名：去空、去掉 ".json" 后缀
    [[nodiscard]] static QString normalizedFileName(const QString &fileName);

    void load();
    void save();
    void scheduleSave();

    QString m_fileName;
    QString m_filePath;
    mutable QMutex m_mutex;
    std::unordered_map<QString, QVariant, QStringHasher> m_values;
    QTimer m_saveTimer;
};

#endif // SETTINGMANAGER_H

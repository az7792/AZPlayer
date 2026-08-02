// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/settingmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QTimer>

// 写盘延迟：多次 setValue 合并为一次写盘
static constexpr int SAVE_DELAY_MS = 500;

SettingManager::SettingManager(const QString &fileName, QObject *parent)
    : QObject(parent),
      m_fileName(normalizedFileName(fileName)) {
    QDir().mkpath(settingsDir());
    m_filePath = settingsDir() + QLatin1Char('/') + m_fileName + QStringLiteral(".json");

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&m_saveTimer, &QTimer::timeout, this, &SettingManager::save);

    load();
}

SettingManager::~SettingManager() {
    sync();
}

QString SettingManager::settingsDir() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/settings");
}

QString SettingManager::fileName() const {
    return m_fileName;
}

void SettingManager::setFileName(const QString &fileName) {
    const QString normalized = normalizedFileName(fileName);
    if (normalized == m_fileName)
        return;

    m_fileName = normalized;
    m_filePath = settingsDir() + QLatin1Char('/') + m_fileName + QStringLiteral(".json");
    load();
}

QVariant SettingManager::value(const QString &key, const QVariant &defaultValue) const {
    QMutexLocker locker(&m_mutex);
    const auto it = m_values.find(key);
    return (it != m_values.end()) ? it->second : defaultValue;
}

void SettingManager::setValue(const QString &key, const QVariant &value) {
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_values.find(key);
        if (it != m_values.end() && it->second == value)
            return;
        m_values[key] = value;
    }
    scheduleSave();
    emit valueChanged(key, value);
}

bool SettingManager::contains(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_values.find(key) != m_values.end();
}

void SettingManager::remove(const QString &key) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_values.erase(key) == 0)
            return;
    }
    scheduleSave();
    emit valueChanged(key, QVariant());
}

void SettingManager::clear() {
    {
        QMutexLocker locker(&m_mutex);
        m_values.clear();
    }
    scheduleSave();
}

void SettingManager::sync() {
    if (m_saveTimer.isActive())
        m_saveTimer.stop();
    save();
}

QString SettingManager::stringValue(const QString &key, const QString &defaultValue) const {
    return value(key, defaultValue).toString();
}

int SettingManager::intValue(const QString &key, int defaultValue) const {
    const QVariant v = value(key, defaultValue);
    bool ok = false;
    const int result = v.toInt(&ok);
    return ok ? result : defaultValue;
}

bool SettingManager::boolValue(const QString &key, bool defaultValue) const {
    return value(key, defaultValue).toBool();
}

double SettingManager::doubleValue(const QString &key, double defaultValue) const {
    const QVariant v = value(key, defaultValue);
    bool ok = false;
    const double result = v.toDouble(&ok);
    return ok ? result : defaultValue;
}

QString SettingManager::normalizedFileName(const QString &fileName) {
    QString trimmed = fileName.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("setting");

    const QString suffix = QStringLiteral(".json");
    QString result = trimmed.endsWith(suffix, Qt::CaseInsensitive)
                     ? trimmed.left(trimmed.size() - suffix.size())
                     : trimmed;

    return result.trimmed();
}

void SettingManager::load() {
    QMutexLocker locker(&m_mutex);
    m_values.clear();

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QByteArray data = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        m_values[it.key()] = it.value().toVariant();
}

void SettingManager::save() {
    QMutexLocker locker(&m_mutex);

    QJsonObject obj;
    for (const auto &pair : m_values)
        obj.insert(pair.first, QJsonValue::fromVariant(pair.second));

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void SettingManager::scheduleSave() {
    if (!m_saveTimer.isActive())
        m_saveTimer.start();
}

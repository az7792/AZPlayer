// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include <QObject>

class PowerManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PowerManager)

    explicit PowerManager(QObject *parent = nullptr);
    ~PowerManager();
public:

    static PowerManager &instance();

    // 控制是否保持唤醒
    void setKeepScreenOn(bool on);
};

#endif // POWERMANAGER_H

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRTYRECTMANAGER_H
#define DIRTYRECTMANAGER_H

#include <QRect>
#include <vector>

class DirtyRectManager {
public:
    DirtyRectManager() = default;

    // 初始化/清空
    void init();

    // 添加并尝试合并
    void addRect(const QRect &newRect);

    // 返回第一个相交矩形的索引，无交集返回 -1
    int findFirstIntersect(const QRect &rect) const;

    // 获取所有不重叠的矩形
    const std::vector<QRect> &getRects() const;

    // 获取指定索引的矩形
    const QRect &operator[](int index) const;

    size_t size() const;

    bool isEmpty() const;

private:
    std::vector<QRect> m_rects;
};

#endif // DIRTYRECTMANAGER_H

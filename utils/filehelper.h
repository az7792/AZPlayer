// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <QObject>
#include <QString>
#include <QVariant>

class FileHelper : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FileHelper)
public:
    // 获取单例
    static FileHelper &instance();

    // 常用过滤器
    static const QStringList VIDEO_FILTERS;
    static const QStringList AUDIO_FILTERS;
    static const QStringList SUBTITLE_FILTERS;
    static const QStringList MEDIA_FILTERS;     // 音视频格式
    static const QStringList ALL_MEDIA_FILTERS; // 所有媒体格式

    /**
     * 展开文件和文件夹，返回QVariantMap列表，单个map包含两个(key,value),类型均为QString:
     * text:文件名
     * fileUrl:URL
     */
    Q_INVOKABLE [[nodiscard]] QVariantList expandFiles(const QStringList &inputPaths, const QStringList &filters) const;

signals:

private:
    explicit FileHelper(QObject *parent = nullptr);

    // 匹配文件后缀,不区分大小写
    [[nodiscard]] bool matchesFilter(const QString &fileName, const QStringList &filters) const;

    // 递归展开文件夹内的所有文件
    void scanFolderRecursive(const QString &folderPath, QStringList &outFiles, const QStringList &filters) const;
};

#endif // FILEHELPER_H

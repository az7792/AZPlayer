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
     * 展开文件和文件夹
     * @param inputPaths 文件/文件夹列表, 路径为本地格式, 例如：C:/Media_Library/Videos
     * @param filters 后缀过滤器, 例如：{"*.mp4", "*.mkv", "*.avi", "*.mov"}
     * @param recursive 是否递归子文件夹
     * @return QVariantMap列表, 单个map包含两个(key, value), 类型均为 QString:
     *
     * - text: 文件名, 例如: a.mkv
     *
     * - fileUrl: URL格式的文件路径, 例如：file:///C:/Media_Library/Videos/a.mkv
     */
    [[nodiscard]] QVariantList expandFiles(const QStringList &inputPaths, const QStringList &filters, bool recursive) const;

    // 给 QML 用的简单包装
    Q_INVOKABLE [[nodiscard]] QVariantList expandVideoFiles(const QList<QUrl> &urls, bool recursive) const;
    Q_INVOKABLE [[nodiscard]] QVariantList expandAudioFiles(const QList<QUrl> &urls, bool recursive) const;
    Q_INVOKABLE [[nodiscard]] QVariantList expandSubtitleFiles(const QList<QUrl> &urls, bool recursive) const;
    Q_INVOKABLE [[nodiscard]] QVariantList expandMediaFiles(const QList<QUrl> &urls, bool recursive) const;

signals:

private:
    explicit FileHelper(QObject *parent = nullptr);

    // 匹配文件后缀,不区分大小写
    [[nodiscard]] bool matchesFilter(const QString &fileName, const QStringList &filters) const;

    // 递归展开文件夹内的所有文件
    void scanFolder(const QString &folderPath, QStringList &outFiles, const QStringList &filters, bool recursive) const;

    // urls -> local strings
    [[nodiscard]] static QStringList toLocalPaths(const QList<QUrl> &urls);
};

#endif // FILEHELPER_H

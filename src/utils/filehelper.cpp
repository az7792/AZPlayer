// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/filehelper.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>

static constexpr qsizetype MAX_SCAN_FILES = 500; // 扫描单个文件夹时允许的最多文件数量
const QStringList FileHelper::VIDEO_FILTERS = {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.wmv", "*.flv", "*.webm", "*.m4v", "*.mpg", "*.mpeg"};
const QStringList FileHelper::AUDIO_FILTERS = {"*.mp3", "*.aac", "*.flac", "*.mka", "*.wav", "*.ogg", "*.ac3", "*.m4a", "*.opus", "*.wma"};
const QStringList FileHelper::SUBTITLE_FILTERS = {"*.srt", "*.ass", "*.ssa", "*.vtt", "*.sub", "*.idx", "*.pgs"};
const QStringList FileHelper::MEDIA_FILTERS = []() {
    QStringList list = VIDEO_FILTERS;
    list.append(AUDIO_FILTERS);
    return list;
}();
const QStringList FileHelper::ALL_MEDIA_FILTERS = []() {
    QStringList list = VIDEO_FILTERS;
    list.append(AUDIO_FILTERS);
    list.append(SUBTITLE_FILTERS);
    return list;
}();

FileHelper::FileHelper(QObject *parent)
    : QObject{parent} {}

FileHelper &FileHelper::instance() {
    static FileHelper inst;
    return inst;
}

QVariantList FileHelper::expandFiles(const QStringList &inputPaths, const QStringList &filters, bool recursive) const {
    QVariantList result;

    QSet<QString> seen; // 去重 URL

    for (const QString &path : inputPaths) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;

        if (fi.isFile() && matchesFilter(fi.fileName(), filters)) {
            QString url = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
            if (!seen.contains(url)) {
                QVariantMap map;
                map["text"] = fi.fileName();
                map["fileUrl"] = url;
                result << map;
                seen.insert(url);
            }
        } else if (fi.isDir()) {
            QStringList folderFiles;
            QStringList effectiveFilters = filters;
            if (effectiveFilters.isEmpty()) {
                effectiveFilters << "*";
            }
            scanFolder(fi.absoluteFilePath(), folderFiles, effectiveFilters, recursive);
            for (const QString &f : std::as_const(folderFiles)) {
                QString url = QUrl::fromLocalFile(f).toString();
                if (!seen.contains(url)) {
                    QFileInfo fi2(f);
                    QVariantMap map;
                    map["text"] = fi2.fileName();
                    map["fileUrl"] = url;
                    result << map;
                    seen.insert(url);
                }
            }
        }
    }

    return result;
}

QVariantList FileHelper::expandVideoFiles(const QList<QUrl> &urls, bool recursive) const {
    return expandFiles(toLocalPaths(urls), VIDEO_FILTERS, recursive);
}

QVariantList FileHelper::expandAudioFiles(const QList<QUrl> &urls, bool recursive) const {
    return expandFiles(toLocalPaths(urls), AUDIO_FILTERS, recursive);
}

QVariantList FileHelper::expandSubtitleFiles(const QList<QUrl> &urls, bool recursive) const {
    return expandFiles(toLocalPaths(urls), SUBTITLE_FILTERS, recursive);
}

QVariantList FileHelper::expandMediaFiles(const QList<QUrl> &urls, bool recursive) const {
    return expandFiles(toLocalPaths(urls), MEDIA_FILTERS, recursive);
}

// ===== static =====

bool FileHelper::matchesFilter(const QString &fileName, const QStringList &filters) const {
    if (filters.isEmpty()) return true; // 空过滤器匹配所有

    for (const QString &f : filters) {
        if (fileName.endsWith(QStringView(f).sliced(1), Qt::CaseInsensitive)) return true;
    }
    return false;
}

void FileHelper::scanFolder(const QString &folderPath, QStringList &outFiles, const QStringList &filters, bool recursive) const {
    if (outFiles.size() >= MAX_SCAN_FILES) return;

    QDir dir(folderPath);
    if (!dir.exists()) return;

    // 筛选文件
    const QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : fileList) {
        if (outFiles.size() >= MAX_SCAN_FILES) return;
        outFiles << fi.absoluteFilePath();
    }

    if (!recursive) return;

    // 递归子目录
    const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subDirs) {
        scanFolder(fi.absoluteFilePath(), outFiles, filters, recursive);
    }
}

QStringList FileHelper::toLocalPaths(const QList<QUrl> &urls) {
    QStringList paths;
    paths.reserve(urls.size());

    for (const QUrl &url : urls) {
        paths.append(url.toLocalFile());
    }

    return paths;
}

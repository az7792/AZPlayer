// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filehelper.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>

FileHelper::FileHelper(QObject *parent)
    : QObject{parent} {}

FileHelper &FileHelper::instance() {
    static FileHelper inst;
    return inst;
}

QVariantList FileHelper::expandFiles(const QStringList &inputPaths) {
    QVariantList result;
    QStringList filters = {"*.mp4", "*.mkv", "*.avi", "*.mp3", "*.aac", "*.flac", "*.mka", "*.wav", "*.ogg", "*.ac3"};

    QSet<QString> seen; // 去重 URL

    for (const QString &path : inputPaths) {
        QFileInfo fi(path);
        if (!fi.exists())
            continue;

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
            scanFolderRecursive(fi.absoluteFilePath(), folderFiles, filters);
            for (const QString &f : folderFiles) {
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

bool FileHelper::matchesFilter(const QString &fileName, const QStringList &filters) {
    for (const QString &f : filters) {
        if (fileName.endsWith(QStringView(f).sliced(1), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void FileHelper::scanFolderRecursive(const QString &folderPath, QStringList &outFiles, const QStringList &filters) {
    QDir dir(folderPath);
    if (!dir.exists())
        return;

    // 筛选文件
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : fileList) {
        outFiles << fi.absoluteFilePath();
    }

    // 递归子目录
    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subDirs) {
        scanFolderRecursive(fi.absoluteFilePath(), outFiles, filters);
    }
}

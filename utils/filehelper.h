#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <QObject>
#include <QString>
#include <QVariant>

class FileHelper : public QObject {
    Q_OBJECT

public:
    // 获取单例
    static FileHelper &instance();

    /**
     * 展开文件和文件夹，返回QVariantMap列表，单个map包含两个(key,value),类型均为QString:
     * text:文件名
     * fileUrl:URL
     */
    Q_INVOKABLE QVariantList expandFiles(const QStringList &inputPaths);

signals:

private:
    explicit FileHelper(QObject *parent = nullptr);
    FileHelper(const FileHelper &) = delete;
    FileHelper &operator=(const FileHelper &) = delete;

    // 匹配文件后缀,不区分大小写
    bool matchesFilter(const QString &fileName, const QStringList &filters);

    // 递归展开文件夹内的所有文件
    void scanFolderRecursive(const QString &folderPath, QStringList &outFiles, const QStringList &filters);
};

#endif // FILEHELPER_H

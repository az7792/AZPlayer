#ifndef EPISODEASSETMANAGER_H
#define EPISODEASSETMANAGER_H

#include <QObject>
#include <QUrl>

class EpisodeAssetManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EpisodeAssetManager)
    explicit EpisodeAssetManager(QObject *parent = nullptr);
public:
    static EpisodeAssetManager &instance();

    /**
     * @brief 根据文件解析其对应文件夹下的字幕
     * @param videoFileURL 视频 URL
     * @return 字幕 URL
     */
    [[nodiscard]] QUrl resolveSubtitleURL(const QUrl &videoFileURL);

private:
};

#endif // EPISODEASSETMANAGER_H

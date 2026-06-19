// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/episodeassetmanager.h"
#include <QFileInfo>
#include <QHash>
#include <QUrl>
#include "utils/filehelper.h"
#include <unordered_map>
#include <vector>

namespace {
struct QStringHasher {
    std::size_t operator()(const QString& s) const noexcept { return qHash(s); }
};

struct FileItem {
    QString fileName;
    QString filePath;
};

struct FileInfo {
    QString skeleton;      // 将所有数字统一替换为 '\0' 后的模板字符串
    std::vector<int> nums; // 文件名中的数字部分

    FileInfo() {}
    FileInfo(const QString& s) {
        skeleton.reserve(s.size());
        for (int i = 0; i < s.size();) {
            if (s[i].isDigit()) {
                skeleton.push_back(QChar('\0'));

                int num = 0;
                while (i < s.size() && s[i].isDigit()) {
                    num = num * 10 + s[i].digitValue();
                    i++;
                }
                nums.push_back(num);
            } else {
                Q_ASSERT(s[i] != QChar('\0'));
                skeleton.push_back(s[i]);
                i++;
            }
        }
    }
};

/**
 * 提取集数
 * @param filenames 文件名数组
 * @return 与各个文件名对应的集数, -1表示无法提取集数
 */
[[nodiscard]] std::vector<int> extractEpisodes(const std::vector<QString>& filenames) {
    const size_t n = filenames.size();
    std::vector<FileInfo> files(n);
    std::vector<int> answer(n, -1), unique_nums;
    std::unordered_map<QString, std::vector<int>, QStringHasher> groups;

    for (int i = 0; i < static_cast<int>(n); ++i) {
        files[i] = FileInfo(filenames[i]);
        groups[files[i].skeleton].push_back(i);
    }

    for (const auto& pair : groups) {
        const std::vector<int>& group = pair.second;

        // 单个文件
        if (group.size() == 1) {
            const int idx = group[0];
            // 如果数字 token 刚好等于 1，也算作集数
            if (files[idx].nums.size() == 1) {
                answer[idx] = files[idx].nums[0];
            }
            continue;
        }

        const size_t num_count = (int) files[group[0]].nums.size();
        int best_pos = -1;
        size_t best_unique = 0;

        for (size_t pos = 0; pos < num_count; ++pos) {
            unique_nums.clear();

            for (auto idx : group) {
                unique_nums.push_back(files[idx].nums[pos]);
            }

            std::sort(unique_nums.begin(), unique_nums.end());
            auto it = std::unique(unique_nums.begin(), unique_nums.end());
            const size_t current_unique = std::distance(unique_nums.begin(), it);

            if (current_unique > best_unique) {
                best_unique = current_unique;
                best_pos = static_cast<int>(pos);
            }
        }

        if (best_unique < 2) continue;

        for (int idx : group) {
            answer[idx] = files[idx].nums[best_pos];
        }
    }

    return answer;
}
} // namespace

EpisodeAssetManager::EpisodeAssetManager(QObject* parent)
    : QObject{parent} {}

EpisodeAssetManager& EpisodeAssetManager::instance() {
    static EpisodeAssetManager instance;
    return instance;
}

QUrl EpisodeAssetManager::resolveSubtitleURL(const QUrl& videoFileURL) {
    const QString localFile = videoFileURL.toLocalFile();
    const QFileInfo videoInfo(localFile);
    if (!videoInfo.exists() || !videoInfo.isFile()) {
        return {};
    }

    const QString videoSuffix = videoInfo.suffix().toLower();      // 文件后缀(小写)
    const QString videoDir = videoInfo.absolutePath();             // 文件所在目录
    const QString videoFileName = videoInfo.fileName();            // 文件名(含后缀)
    const QString videoBaseName = videoInfo.completeBaseName();    // 文件名(不含后缀)
    const QString currentVideoPath = videoInfo.absoluteFilePath(); // 文件全路径

    // === 如果不是视频文件直接返回空 ===
    if (!FileHelper::VIDEO_FILTERS.contains("*." + videoSuffix, Qt::CaseInsensitive)) {
        return {};
    }

    // === 字幕 + 与当前视频同后缀的视频 ===
    QStringList filters = FileHelper::SUBTITLE_FILTERS;
    filters.append("*." + videoSuffix);
    const QVariantList files = FileHelper::instance().expandFiles(QStringList(videoDir), filters, false);

    // === 筛选出字幕和视频文件 ===
    std::vector<FileItem> subtitles, videos;

    for (const QVariant& v : files) {
        const QVariantMap map = v.toMap();

        FileItem item{map["text"].toString(), map["fileUrl"].toString()};

        const QString ext = QFileInfo(item.fileName).suffix();

        if (ext.compare(videoSuffix, Qt::CaseInsensitive) == 0) {
            videos.push_back(std::move(item));
        } else {
            subtitles.push_back(std::move(item));
        }
    }

    // === 精确匹配 ===
    // MAYBE: 也许没必要精确匹配
    QHash<QString, QString> subtitleMap;
    subtitleMap.reserve(subtitles.size());

    for (const auto& sub : subtitles) {
        subtitleMap.insert(sub.fileName, sub.filePath);
    }

    const QStringList exactPatterns = {
        videoBaseName + ".ass",
        videoBaseName + ".ssa",
        videoBaseName + ".srt",
    };

    for (const QString& pattern : exactPatterns) {
        auto it = subtitleMap.find(pattern);

        if (it != subtitleMap.end()) {
            return it.value();
        }
    }

    // === 通配符匹配 {video}*.ass, {video}*.ssa, {video}*.srt ... ===
    for (const auto& sub : subtitles) {
        // subtitles 已经保证了后缀, 所以直接判断前缀即可
        if (sub.fileName.startsWith(videoBaseName)) {
            return sub.filePath;
        }
    }

    // === 集数匹配 ===
    std::vector<QString> fileNames;
    fileNames.reserve(videos.size() + subtitles.size());

    size_t currentVideoIndex = videos.size();

    for (const auto& video : videos) {
        if (video.fileName == videoFileName) {
            currentVideoIndex = fileNames.size();
        }

        fileNames.push_back(video.fileName);
    }

    for (const auto& sub : subtitles) {
        fileNames.push_back(sub.fileName);
    }

    if (currentVideoIndex < videos.size()) {
        const std::vector<int> episodes = extractEpisodes(fileNames);
        const int targetEpisode = episodes[currentVideoIndex];

        // TODO: 缓存结果
        if (targetEpisode != -1) {
            for (size_t i = 0; i < subtitles.size(); ++i) {
                if (episodes[videos.size() + i] == targetEpisode) {
                    return subtitles[i].filePath;
                }
            }
        }
    }

    // === 降级策略 ===
    if (videos.size() == 1 && subtitles.size() == 1) {
        return subtitles.front().filePath;
    }

    return {};
}

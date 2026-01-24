#include "dirtyrectmanager.h"
#include <QDebug>

void DirtyRectManager::init() {
    m_rects.clear();
}

void DirtyRectManager::addRect(const QRect &newRect) {
    if (!newRect.isValid())
        return;

    QRect mergedRect = newRect;
    int i = 0;

    while (i < static_cast<int>(m_rects.size())) {
        if (mergedRect.intersects(m_rects[i])) {
            // 合并
            mergedRect = mergedRect.united(m_rects[i]);

            if (i < static_cast<int>(m_rects.size()) - 1) {
                m_rects[i] = std::move(m_rects.back());
            }
            m_rects.pop_back();

            // 重新检查，因为新的 mergedRect 可能与列表中的任何矩形相交
            i = 0;
        } else {
            ++i;
        }
    }

    m_rects.push_back(mergedRect);

    // ========Debug============
    static size_t ct = 0;
    if (m_rects.size() > ct) {
        ct = m_rects.size();
        qDebug() << "Max Size:" << ct;
        // 对应ASS字幕而言，采用的多帧合成，大多数帧其实是重叠的，
        // 我手上最复杂的ASS字幕目前测得最大的size为8，大部分情况其实只有1-3，所以这儿是没什么性能问题的
    }
}

int DirtyRectManager::findFirstIntersect(const QRect &rect) const {
    if (rect.isEmpty())
        return -1;

    for (int i = 0; i < static_cast<int>(m_rects.size()); ++i) {
        if (rect.intersects(m_rects[i])) {
            return i;
        }
    }
    return -1;
}

const std::vector<QRect> &DirtyRectManager::getRects() const {
    return m_rects;
}

const QRect &DirtyRectManager::operator[](int index) const {
    return m_rects[index];
}

size_t DirtyRectManager::size() const {
    return m_rects.size();
}

bool DirtyRectManager::isEmpty() const {
    return m_rects.empty();
}

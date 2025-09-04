#ifndef SPSCQUEUE_H
#define SPSCQUEUE_H

#include <atomic>
#include <vector>

/**
 * 单生产者单消费者的有限无锁队列(缓冲区)
 */
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : m_capacity(capacity + 1), // 多分配一个，用于处理满条件
          m_buffer(m_capacity) {}

    bool push(const T &value) {
        // 生产者本地快照
        size_t current_tail = m_tail.v.load(std::memory_order_relaxed);
        size_t next_tail = nextIndex(current_tail);

        // 检查队列是否已满
        if (next_tail == m_head.v.load(std::memory_order_acquire)) {
            return false; // 队列满，推送失败
        }

        // 将数据存入当前tail指向的位置
        m_buffer[current_tail] = value;

        // 发布tail的更新，确保前面的数据存储对消费者可见
        m_tail.v.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T &value) {
        // 消费者本地快照
        size_t current_head = m_head.v.load(std::memory_order_relaxed);

        // 检查队列是否为空
        if (current_head == m_tail.v.load(std::memory_order_acquire)) {
            return false; // 队列空，弹出失败
        }

        // 从当前head指向的位置取出数据
        value = m_buffer[current_head];

        // 发布head的更新，告知生产者新的头部位置
        m_head.v.store(nextIndex(current_head), std::memory_order_release);
        return true;
    }

    size_t capacity() const {
        return m_capacity - 1;
    }

private:
    size_t nextIndex(size_t index) const {
        return (index + 1) % m_capacity;
    }

    const size_t m_capacity; // 环形缓冲区的总容量（包括浪费的一个位置）
    std::vector<T> m_buffer; // 数据缓冲区

    struct alignas(64) PaddedAtomic {
        std::atomic<size_t> v;
        char pad[64 - sizeof(std::atomic<size_t>)]; // 填充缓存行,防止伪共享
        PaddedAtomic() : v(0) {}
    };
    PaddedAtomic m_head; // 读索引（消费者使用）
    PaddedAtomic m_tail; // 写索引（生产者使用）
};

#endif // SPSCQUEUE_H

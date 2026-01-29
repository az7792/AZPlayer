#ifndef ATOMICDOUBLEBUFFER_H
#define ATOMICDOUBLEBUFFER_H

// 读线程可以复用数据，使用不可复用的模式下频繁seek时画面可能小概率出现发绿，发黑的问题，原因未知，且性能提升目前感觉很有限，所有暂时先用可复用的版本
// FIXME:
#define SAFE_ATOMIC_DOUBLE_BUFFER

#ifdef SAFE_ATOMIC_DOUBLE_BUFFER
#include <atomic>
#include <functional>
#include <qdebug.h>
#include <thread>

template <typename T>
class AtomicDoubleBuffer {
public:
    std::atomic<uint32_t> serial{0};
    // 读写回调函数，可安全的在内部进行读写
    using WriterFunc = std::function<bool(T &, [[maybe_unused]] int)>;
    using ReaderFunc = std::function<bool(T &, [[maybe_unused]] int)>;
    using ResetFunc = std::function<bool(T &, T &)>;

    AtomicDoubleBuffer() = default;

    /**
     * @brief 重置buf, 请确保读写线程都未使用时调用
     * TODO: 在多线程环境也能使用
     */
    bool reset(ResetFunc func) {
        m_state = 0;
        m_realLatestIdx = -1;
        if (func) {
            return func(m_buffers[0], m_buffers[1]);
        }
        return false;
    }

    // 收到发布数据
    void release() {
        if (m_realLatestIdx != 0 && m_realLatestIdx != 1)
            return;
        // 发布新的索引，同时保留其他 Busy 位的状态
        uint8_t oldState = m_state.load(std::memory_order_acquire);
        uint8_t newState;
        do {
            newState = (oldState & ~LATEST_MASK) | (uint8_t)m_realLatestIdx | READY_BIT;
        } while (!m_state.compare_exchange_weak(oldState, newState, std::memory_order_release, std::memory_order_relaxed));
    }

    /**
     * @brief 写操作：用户在回调中直接修改 Buffer
     * @warning 请不要在回调中进行阻塞操作
     */
    bool write(WriterFunc func, bool autoRelease = true) {
        // 获取当前最新索引，确定要写的“目标”是哪一个
        uint8_t s = m_state.load(std::memory_order_acquire);
        const int targetIdx = 1 - (s & LATEST_MASK); // 1 -> 0 或 0 -> 1
        uint8_t targetBusyBit = targetIdx == 0 ? BUSY0_BIT : BUSY1_BIT;

        // 如果读线程正锁着要写的那个，则“等一会”
        while (m_state.load(std::memory_order_acquire) & targetBusyBit) {
            std::this_thread::yield();
        }

        bool ok = true;
        // 用户安全的进行写操作
        if (func) {
            ok = func(m_buffers[targetIdx], targetIdx);
        }

        m_realLatestIdx = targetIdx;

        if (autoRelease)
            release();

        if (!((m_state.load(std::memory_order_relaxed) & 0x06) != 0x06)) {
            qDebug() << "write error, state=" << int(m_state.load());
            exit(-1);
        }
        return ok;
    }

    /**
     * @brief 读操作：用户在回调中直接读取最新 Buffer
     * @warning 请不要在回调中进行阻塞操作
     */
    bool read(ReaderFunc func) {
        uint8_t oldState, lockedState;
        int targetIdx;
        uint8_t targetBusyBit;

        // 尝试锁定最新索引对应的 Busy 位
        oldState = m_state.load(std::memory_order_acquire);
        if (!(oldState & READY_BIT)) {
            return false;
        }

        do {
            targetIdx = oldState & LATEST_MASK;
            targetBusyBit = targetIdx == 0 ? BUSY0_BIT : BUSY1_BIT;
            lockedState = oldState | targetBusyBit;

            // 如果已经被锁定了，CAS 依然会尝试再次设置 busy 位，这是安全的
        } while (!m_state.compare_exchange_weak(oldState, lockedState, std::memory_order_acquire, std::memory_order_relaxed));

        if (!((m_state.load(std::memory_order_relaxed) & 0x06) != 0x06)) {
            qDebug() << "read error, state=" << int(m_state.load());
            exit(-2);
        }

        bool ok = true;
        // 执行用户回调，读访问 / 允许进行修改

        if (func) {
            ok = func(m_buffers[targetIdx], targetIdx);
        }

        // 将对应的 Busy 位清零
        m_state.fetch_and(~targetBusyBit, std::memory_order_release);

        return ok;
    }

    // 获取当前状态用于调试
    uint8_t get_state() const {
        return m_state.load(std::memory_order_relaxed);
    }

private:
    T m_buffers[2];

    /**
     * 原子状态位:
     * bit 0: latest_idx (0 或 1) -> 标记哪一个是最新写完的
     * bit 1: buf0_busy  (1 为忙) -> 读线程正在读 buffers[0]
     * bit 2: buf1_busy  (1 为忙) -> 读线程正在读 buffers[1]
     * bit 3: empty      (0 为空) -> 两个buffers都没准备好
     */
    std::atomic<uint8_t> m_state{0};

    // 实际最新的idx,由write()产生/修改，由release()去修改m_state
    int8_t m_realLatestIdx{-1};

    static constexpr uint8_t LATEST_MASK = 0x01;
    static constexpr uint8_t BUSY0_BIT = 0x02;
    static constexpr uint8_t BUSY1_BIT = 0x04;
    static constexpr uint8_t READY_BIT = 0x08;
};

#else

#include <atomic>
#include <functional>
#include <qdebug.h>
#include <thread>

/**
 * @class AtomicDoubleBuffer
 * @brief 双缓冲数据交换原子组件。
 *
 * 仅适用于单生产者单消费者（SPSC）场景
 *
 * 写线程 (Producer)：
 *
 * - 目标：写 LATEST 镜像位的 Buffer。
 *
 * - 等待：仅在目标 Buffer 被 Reader 锁定时（Busy 位为 1）自旋 yield。
 *
 * - 覆盖：如果目标 Buffer 已发布但未被读取（Avail 位为 1），直接覆盖。
 *
 * 读线程 (Consumer)：
 *
 * - 目标：仅尝试读取 LATEST 且 Avail 的 Buffer。
 *
 * - 处理：读取前原子加锁（Busy 位），处理完成后原子解锁并销毁数据有效标记（Avail 位）。
 *
 * * @tparam T 缓冲区数据类型。
 */
template <typename T>
class AtomicDoubleBuffer {
public:
    std::atomic<uint32_t> serial{0};
    // 读写回调函数，可安全的在内部进行读写
    using WriterFunc = std::function<bool(T &, [[maybe_unused]] int)>;
    using ReaderFunc = std::function<bool(T &, [[maybe_unused]] int)>;
    using ResetFunc = std::function<bool(T &, T &)>;

    AtomicDoubleBuffer() = default;

    /**
     * @brief 重置buf, 请确保读写线程都未使用时调用
     * TODO: 在多线程环境也能使用
     */
    bool reset(ResetFunc func) {
        m_state = 0;
        m_realLatestIdx = -1;
        if (func) {
            return func(m_buffers[0], m_buffers[1]);
        }
        return false;
    }

    // 手动发布数据
    void release() {
        if (m_realLatestIdx != 0 && m_realLatestIdx != 1)
            return;

        uint8_t availBit = (m_realLatestIdx == 0) ? AVAIL0_BIT : AVAIL1_BIT;
        uint8_t oldState = m_state.load(std::memory_order_acquire);
        uint8_t newState;

        do {
            // 更新最新索引，并打上 AVAIL 标签
            newState = (oldState & ~LATEST_MASK) | (uint8_t)m_realLatestIdx | availBit;
        } while (!m_state.compare_exchange_weak(oldState, newState, std::memory_order_release, std::memory_order_relaxed));
    }

    /**
     * @brief 写操作：用户在回调中直接修改 Buffer
     * @warning 请不要在回调中进行阻塞操作
     */
    bool write(WriterFunc func, bool autoRelease = true) {
        // 获取当前最新索引，确定要写的“目标”是哪一个
        uint8_t s = m_state.load(std::memory_order_acquire);
        const int targetIdx = 1 - (s & LATEST_MASK); // 1 -> 0 或 0 -> 1
        const uint8_t targetBusyBit = targetIdx == 0 ? BUSY0_BIT : BUSY1_BIT;

        // 如果读线程正锁着要写的那个，则等一会
        while (m_state.load(std::memory_order_acquire) & targetBusyBit) {
            std::this_thread::yield();
        }

        bool ok = true;
        // 用户安全的进行写操作
        if (func) {
            ok = func(m_buffers[targetIdx], targetIdx);
        }

        m_realLatestIdx = targetIdx;

        if (autoRelease)
            release();

        if (!((m_state.load(std::memory_order_relaxed) & 0x06) != 0x06)) {
            qDebug() << "write error, state=" << int(m_state.load());
            exit(-1);
        }
        return ok;
    }

    /**
     * @brief 读操作：用户在回调中直接读取最新 Buffer
     * @warning 请不要在回调中进行阻塞操作
     */
    bool read(ReaderFunc func) {
        uint8_t oldState = m_state.load(std::memory_order_acquire);

        // 可能的最新的索引
        int targetIdx = oldState & LATEST_MASK;
        uint8_t targetAvailBit = (targetIdx == 0) ? AVAIL0_BIT : AVAIL1_BIT;

        // 如果当前的不是最新的，检查另一个
        if (!(oldState & targetAvailBit)) {
            targetIdx = 1 - targetIdx;
            targetAvailBit = (targetIdx == 0) ? AVAIL0_BIT : AVAIL1_BIT;
            if (!(oldState & targetAvailBit))
                return false; // 两个都不可读
        }

        uint8_t targetBusyBit = (targetIdx == 0) ? BUSY0_BIT : BUSY1_BIT;
        uint8_t lockedState;
        do {
            if (!(oldState & targetAvailBit))
                return false; // 再次检查：如果不再可用，直接退出
            lockedState = oldState | targetBusyBit;
        } while (!m_state.compare_exchange_weak(oldState, lockedState, std::memory_order_acquire, std::memory_order_relaxed));

        if (!((m_state.load(std::memory_order_relaxed) & 0x06) != 0x06)) {
            qDebug() << "read error, state=" << int(m_state.load());
            exit(-2);
        }

        bool ok = true;
        // 执行用户回调，读访问 / 允许进行修改
        if (func) {
            ok = func(m_buffers[targetIdx], targetIdx);
        }

        // 清除 BUSY 和 AVAIL
        m_state.fetch_and(~(targetBusyBit | targetAvailBit), std::memory_order_release);

        return ok;
    }

    // 获取当前状态用于调试
    uint8_t get_state() const {
        return m_state.load(std::memory_order_relaxed);
    }

private:
    T m_buffers[2];

    /**
     * 原子状态位:
     * bit 0: latest_idx (0/1) - 仅作为读线程参考，实际状态看 AVAIL 位
     * bit 1: buf0_busy  (Reader正在读)
     * bit 2: buf1_busy  (Reader正在读)
     * bit 3: buf0_avail (Buffer0 有新数据可读)
     * bit 4: buf1_avail (Buffer1 有新数据可读)
     */
    std::atomic<uint8_t> m_state{0};

    // 写线程私有，记录当前刚写完但未发布的索引
    int8_t m_realLatestIdx{-1};

    static constexpr uint8_t LATEST_MASK = 0x01;
    static constexpr uint8_t BUSY0_BIT = 0x02;
    static constexpr uint8_t BUSY1_BIT = 0x04;
    static constexpr uint8_t AVAIL0_BIT = 0x08;
    static constexpr uint8_t AVAIL1_BIT = 0x10;
};
#endif // SAFE_ATOMIC_DOUBLE_BUFFER

#endif // ATOMICDOUBLEBUFFER_H

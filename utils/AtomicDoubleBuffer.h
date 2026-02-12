// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ATOMICDOUBLEBUFFER_H
#define ATOMICDOUBLEBUFFER_H

#include "compat/compat.h"

// 读线程可以复用数据
// #define REUSABLE_ATOMIC_DOUBLE_BUFFER

#ifdef _MSC_VER
#pragma warning(disable : 4324)
#endif

#ifdef REUSABLE_ATOMIC_DOUBLE_BUFFER
#include <atomic>
#include <functional>
#include <qdebug.h>
#include <thread>

template <typename T>
class AtomicDoubleBuffer {
public:
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
    alignas(hardware_destructive_interference_size) std::atomic<uint8_t> m_state{0};

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
 * - 覆盖：如果目标 Buffer 未锁定，无论是否被读，都直接覆盖新数据。
 *
 * - 发布：修改 LATEST 镜像位和 READY_BIT
 *
 * 读线程 (Consumer)：
 *
 * - 目标：仅在有数据时尝试读取 LATEST Buffer。
 *
 * - 处理：锁 LATEST Buffer 对应的 BUSYX_BIT，然后执行读。
 *
 * - 释放：解锁，并在无新数据的情况下置空READY_BIT
 *
 * * @tparam T 缓冲区数据类型。
 */
template <typename T>
class AtomicDoubleBuffer {
public:
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
        // READY_BIT 写线程权限为0->1 因此读线程发现可读那就一定有资源
        if (!(oldState & READY_BIT)) {
            return false;
        }

        do {
            // 锁定最新的
            targetIdx = oldState & LATEST_MASK;
            targetBusyBit = targetIdx == 0 ? BUSY0_BIT : BUSY1_BIT;
            lockedState = oldState | targetBusyBit;

            // 如果失败oldState会被写入m_state的最新状态
        } while (!m_state.compare_exchange_weak(oldState, lockedState, std::memory_order_acquire, std::memory_order_relaxed));

        if (!((m_state.load(std::memory_order_relaxed) & 0x06) != 0x06)) {
            exit(-2);
        }

        bool ok = true;
        // 执行用户回调，读访问 / 允许进行修改

        if (func) {
            ok = func(m_buffers[targetIdx], targetIdx);
        }

        // 将对应的 Busy 位清零
        static int ct = 0;
        ct = 0;
        uint8_t expected = m_state.load(std::memory_order_acquire);
        while (true) {
            ct++;
            uint8_t nextState = expected & ~targetBusyBit; // 释放当前的 Busy 位

            // 如果 LATEST 还是我刚才读的那个，说明没有更新的数据
            if ((expected & LATEST_MASK) == targetIdx) {
                nextState &= ~READY_BIT;
            }

            // 尝试更新状态
            if (m_state.compare_exchange_weak(expected, nextState, std::memory_order_release, std::memory_order_acquire)) {
                break;
            }
            // 如果失败（说明写线程翻转了 LATEST），继续尝试释放当前的 Busy 位
            // 由于Busy位被锁定，写线程无法再次反转LATEST
            if (ct == 3) {
                exit(-3);
            }
        }

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
    alignas(hardware_destructive_interference_size) std::atomic<uint8_t> m_state{0};

    // 实际最新的idx,由write()产生/修改，由release()去修改m_state
    int8_t m_realLatestIdx{-1}; // 写线程私有 读线程不可读写

    static constexpr uint8_t LATEST_MASK = 0x01; // 写线程私有 读线程只读
    static constexpr uint8_t BUSY0_BIT = 0x02;   // 写线程只读 读线程私有
    static constexpr uint8_t BUSY1_BIT = 0x04;   // 写线程只读 读线程私有
    static constexpr uint8_t READY_BIT = 0x08;   // 写线程权限0->1 读线程权限1->0
};
#endif // REUSABLE_ATOMIC_DOUBLE_BUFFER

#endif // ATOMICDOUBLEBUFFER_H

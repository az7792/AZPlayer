// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPSCBUFFER_H
#define SPSCBUFFER_H

#include "compat/compat.h"
#include <atomic>
#include <memory>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // 因对齐说明符填充结构是预期的（cache line padding）
#endif

class SPSCBuffer {
    SPSCBuffer(const SPSCBuffer &) = delete;
    SPSCBuffer &operator=(const SPSCBuffer &) = delete;

public:
    // capacity 必须是 2 的幂
    explicit SPSCBuffer(uint64_t capacity);
    ~SPSCBuffer() = default;

    // 写入指定长度的数据，返回实际写入的数据
    [[nodiscard]] uint64_t write(const uint8_t *input, uint64_t len);

    void requestClearOldData();

    // 读取指定长度的数据，返回实际读取的数据
    [[nodiscard]] uint64_t read(uint8_t *output, uint64_t len);

    void unsafeClear();

    // 获取长度函数
    [[nodiscard]] uint64_t readAvailable() const;
    [[nodiscard]] uint64_t writeAvailable() const;
    [[nodiscard]] uint64_t capacity() const;

private:
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_head{0}; // 读指针
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_tail{0}; // 写指针
    alignas(hardware_destructive_interference_size) std::atomic<uint8_t> m_needClear{0};
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_virtualTail{0};
    const uint64_t m_capacity;
    const uint64_t m_mask;
    std::unique_ptr<uint8_t[]> m_buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // SPSCBUFFER_H

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPSCBUFFER_H
#define SPSCBUFFER_H

#include "compat/compat.h"
#include <atomic>

class SPSCBuffer {
    SPSCBuffer(const SPSCBuffer &) = delete;
    SPSCBuffer &operator=(const SPSCBuffer &) = delete;

public:
    explicit SPSCBuffer(uint64_t capacity);

    ~SPSCBuffer();

    // 写入指定长度的数据，返回实际写入的数据
    uint64_t write(const uint8_t *input, uint64_t len);

    void requestClearOldData();

    // 读取指定长度的数据，返回实际读取的数据
    uint64_t read(uint8_t *output, uint64_t len);

    void unsafeClear();

    // 获取长度函数
    uint64_t readAvailable() const;
    uint64_t writeAvailable() const;
    uint64_t capacity() const;

private:
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_head; // 读指针
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_tail; // 写指针
    alignas(hardware_destructive_interference_size) std::atomic<uint8_t> m_needClear;
    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> m_virtualTail;
    const uint64_t m_capacity;
    const uint64_t m_mask;
    uint8_t *m_buffer;
};

#endif // SPSCBUFFER_H

// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spscbuffer.h"
#include <algorithm>
#include <cassert>
#include <cstring>

SPSCBuffer::SPSCBuffer(uint64_t capacity) : m_capacity(capacity), m_mask(capacity - 1) {
    assert((capacity & (capacity - 1)) == 0);
    m_buffer = new uint8_t[m_capacity];
    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
    m_needClear.store(0, std::memory_order_relaxed);
    m_virtualTail.store(0, std::memory_order_relaxed);
}

SPSCBuffer::~SPSCBuffer() { delete[] m_buffer; }

uint64_t SPSCBuffer::write(const uint8_t *input, uint64_t len) {
    if (!input || len == 0)
        return 0;
    uint64_t head = m_head.load(std::memory_order_acquire);
    uint64_t tail = m_tail.load(std::memory_order_relaxed);

    uint64_t avail = m_capacity - (tail - head);
    uint64_t write_len = std::min(len, avail);
    if (write_len == 0)
        return 0;

    uint64_t pos = tail & m_mask;
    uint64_t first_chunk = std::min(write_len, m_capacity - pos);
    std::memcpy(m_buffer + pos, input, first_chunk);
    if (write_len > first_chunk) {
        std::memcpy(m_buffer, input + first_chunk, write_len - first_chunk);
    }

    m_tail.store(tail + write_len, std::memory_order_release);
    return write_len;
}

void SPSCBuffer::requestClearOldData() {
    m_virtualTail.store(m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
    m_needClear.store(1, std::memory_order_release);
}

uint64_t SPSCBuffer::read(uint8_t *output, uint64_t len) {
    if (!output || len == 0)
        return 0;
    uint64_t tail = m_tail.load(std::memory_order_acquire);
    uint64_t head = m_head.load(std::memory_order_relaxed);

    if (m_needClear.load(std::memory_order_acquire)) {
        uint64_t v_tail = m_virtualTail.load(std::memory_order_relaxed);
        // 只有当 v_tail 在 head 之后且在 tail 之前（或等于）时才跳转
        if ((v_tail - head) < m_capacity && (tail - v_tail) < m_capacity) {
            head = v_tail;
            m_head.store(head, std::memory_order_release);
        }
        m_needClear.store(0, std::memory_order_release);
    }

    uint64_t avail = tail - head;
    uint64_t read_len = std::min(len, avail);
    if (read_len == 0)
        return 0;

    uint64_t pos = head & m_mask;
    uint64_t first_chunk = std::min(read_len, m_capacity - pos);
    std::memcpy(output, m_buffer + pos, first_chunk);
    if (read_len > first_chunk) {
        std::memcpy(output + first_chunk, m_buffer, read_len - first_chunk);
    }

    m_head.store(head + read_len, std::memory_order_release);
    return read_len;
}

void SPSCBuffer::unsafeClear() {
    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
}

uint64_t SPSCBuffer::readAvailable() const {
    return m_tail.load(std::memory_order_acquire) - m_head.load(std::memory_order_relaxed);
}

uint64_t SPSCBuffer::writeAvailable() const {
    return m_capacity - (m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_acquire));
}

uint64_t SPSCBuffer::capacity() const {
    return m_capacity;
}

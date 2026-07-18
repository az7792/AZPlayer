// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENUMINDEXARRAY_H
#define ENUMINDEXARRAY_H

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

// 枚举转idx
template <typename E>
[[nodiscard]] constexpr auto to_index(E e) -> std::underlying_type_t<E> {
    return static_cast<std::underlying_type_t<E>>(e);
}


template <typename E, typename = void>
struct has_count : std::false_type {};

template <typename E>
struct has_count<E, std::void_t<decltype(E::Count)>> : std::true_type {};

// E 的枚举值必须从 0 开始连续编号，Count 必须是最后一个枚举项（作为数组大小）
template <typename T, typename E>
class EnumIndexArray {
    static_assert(std::is_enum_v<E>, "E 必须是枚举类型");
    static_assert(has_count<E>::value, "E 必须定义 Count 枚举项作为数组大小");
    std::array<T, to_index(E::Count)> m_data{};

public:
    EnumIndexArray() = default;
    EnumIndexArray(std::initializer_list<T> init) {
        assert(init.size() <= m_data.size());
        std::copy(init.begin(), init.end(), m_data.begin());
    }
    [[nodiscard]] T &operator[](E e) { return m_data[to_index(e)]; }
    [[nodiscard]] T const &operator[](E e) const { return m_data[to_index(e)]; }

    [[nodiscard]] T &operator[](std::size_t i) { return m_data[i]; }
    [[nodiscard]] T const &operator[](std::size_t i) const { return m_data[i]; }

    void fill(T const &v) { m_data.fill(v); }

    [[nodiscard]] constexpr auto begin() noexcept { return m_data.begin(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return m_data.end(); }
    [[nodiscard]] constexpr auto end() const noexcept { return m_data.end(); }
    [[nodiscard]] constexpr auto size() const noexcept { return m_data.size(); }
    [[nodiscard]] constexpr auto data() noexcept { return m_data.data(); }
    [[nodiscard]] constexpr auto data() const noexcept { return m_data.data(); }
};

#endif // ENUMINDEXARRAY_H

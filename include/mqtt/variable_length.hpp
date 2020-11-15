// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIABLE_LENGTH_HPP)
#define MQTT_VARIABLE_LENGTH_HPP

#include <string>

#include <mqtt/namespace.hpp>
#include <mqtt/exception.hpp>

namespace MQTT_NS {

inline std::string
variable_bytes(std::size_t size) {
    std::string bytes;
    if (size > 0xfffffff) return bytes;
    while (size > 127) {
        bytes.push_back(static_cast<char>((size & 0b01111111) | 0b10000000));
        size >>= 7;
    }
    bytes.push_back(size & 0b01111111);
    return bytes;
}

template <typename Container>
inline void
variable_push(Container& c, std::size_t size) {
    if (size > 0xfffffff) return;
    while (size > 127) {
        c.push_back(static_cast<typename Container::value_type>((size & 0b01111111) | 0b10000000));
        size >>= 7;
    }
    c.push_back(size & 0b01111111);
}

template <typename T>
constexpr std::tuple<std::size_t, std::size_t>
variable_length(T const& bytes) {
    std::size_t len = 0;
    std::size_t mul = 1;
    std::size_t consumed = 0;
    for (auto b : bytes) {
        len += (b & 0b01111111) * mul;
        mul *= 128;
        ++consumed;
        if (mul > 128 * 128 * 128 * 128) return {0, 0};
        if (!(b & 0b10000000)) break;
    }
    return {len, consumed};
}

template <typename Iterator>
constexpr std::tuple<std::size_t, std::size_t>
variable_length(Iterator b, Iterator e) {
    std::size_t len = 0;
    std::size_t mul = 1;
    std::size_t consumed = 0;
    for (; b != e; ++b) {
        len += (*b & 0b01111111) * mul;
        mul *= 128;
        ++consumed;
        if (mul > 128 * 128 * 128 * 128) return {0, 0};
        if (!(*b & 0b10000000)) break;
    }
    return {len, consumed};
}

} // namespace MQTT_NS

#endif // MQTT_VARIABLE_LENGTH_HPP

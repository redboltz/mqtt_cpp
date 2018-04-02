// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PUBLISH_HPP)
#define MQTT_PUBLISH_HPP

#include <cstdint>

namespace mqtt {

namespace publish {

inline
constexpr bool is_dup(char v) {
    return (v & 0b00001000) != 0;
}

inline
constexpr std::uint8_t get_qos(char v) {
    return static_cast<std::uint8_t>(v & 0b00000110) >> 1;
}

constexpr bool is_retain(char v) {
    return (v & 0b00000001) != 0;
}

inline
constexpr void set_dup(char& fixed_header, bool dup) {
    if (dup) fixed_header |=  0b00001000;
    else     fixed_header &= ~0b00001000;
}

inline
constexpr void set_qos(char& fixed_header, std::uint8_t qos) {
    fixed_header |= qos << 1;
}

constexpr void set_retain(char& fixed_header, bool retain) {
    if (retain) fixed_header |=  0b00000001;
    else        fixed_header &= ~0b00000001;
}

} // namespace publish

} // namespace mqtt

#endif // MQTT_PUBLISH_HPP

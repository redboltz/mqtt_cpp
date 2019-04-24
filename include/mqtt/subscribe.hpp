// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIBE_HPP)
#define MQTT_SUBSCRIBE_HPP

#include <cstdint>

#include <boost/assert.hpp>

namespace mqtt {

namespace subscribe {

inline
constexpr std::uint8_t get_qos(std::uint8_t v) {
    return v & 0b00000011;
}

inline
constexpr bool is_no_local(std::uint8_t v) {
    return (v & 0b00000100) != 0;
}

inline
constexpr bool is_retain_as_published(std::uint8_t v) {
    return (v & 0b00001000) != 0;
}

inline
constexpr std::uint8_t get_retain_handling(std::uint8_t v) {
    return (v & 0b00110000) >> 4;
}

inline
constexpr void set_qos(std::uint8_t& fixed_header, std::uint8_t qos) {
    BOOST_ASSERT(qos <= 2U);
    fixed_header |= qos;
}

inline
constexpr void set_no_local(std::uint8_t& fixed_header, bool no_local) {
    if (no_local) fixed_header |=  0b00000100;
    else          fixed_header &= static_cast<std::uint8_t>(~0b00000100);
}

inline
constexpr void set_retain_as_published(std::uint8_t& fixed_header, bool retain_as_published) {
    if (retain_as_published) fixed_header |=  0b00001000;
    else                     fixed_header &= static_cast<std::uint8_t>(~0b00001000);
}

inline
constexpr void set_retain_handling(std::uint8_t& fixed_header, std::uint8_t retain_handling) {
    BOOST_ASSERT(retain_handling <= 2U);
    fixed_header |= static_cast<std::uint8_t>(retain_handling << 4);
}

} // namespace subscribe

} // namespace mqtt

#endif // MQTT_SUBSCRIBE_HPP

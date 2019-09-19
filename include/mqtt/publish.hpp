// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PUBLISH_HPP)
#define MQTT_PUBLISH_HPP

#include <cstdint>

#include <boost/assert.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/subscribe_options.hpp>

namespace MQTT_NS {

namespace publish {

constexpr bool is_dup(std::uint8_t v) {
    return (v & 0b00001000) != 0;
}

constexpr qos get_qos(std::uint8_t v) {
    return static_cast<qos>((v & 0b00000110) >> 1);
}

constexpr bool is_retain(std::uint8_t v) {
    return (v & 0b00000001) != 0;
}

constexpr void set_dup(std::uint8_t& fixed_header, bool dup) {
    if (dup) fixed_header |=  0b00001000;
    else     fixed_header &= static_cast<std::uint8_t>(~0b00001000);
}

constexpr void set_qos(std::uint8_t& fixed_header, qos qos_value) {
    BOOST_ASSERT(qos_value == qos::at_most_once || qos_value == qos::at_least_once || qos_value == qos::exactly_once);
    fixed_header |= static_cast<std::uint8_t>(static_cast<std::uint8_t>(qos_value) << 1);
}

constexpr void set_retain(std::uint8_t& fixed_header, bool retain) {
    if (retain) fixed_header |=  0b00000001;
    else        fixed_header &= static_cast<std::uint8_t>(~0b00000001);
}

} // namespace publish

} // namespace MQTT_NS

#endif // MQTT_PUBLISH_HPP

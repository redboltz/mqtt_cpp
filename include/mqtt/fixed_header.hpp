// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_FIXED_HEADER_HPP)
#define MQTT_FIXED_HEADER_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/control_packet_type.hpp>

namespace MQTT_NS {

constexpr std::uint8_t make_fixed_header(control_packet_type type, std::uint8_t flags) {
    return static_cast<std::uint8_t>(type) | (flags & 0x0f);
}

} // namespace MQTT_NS

#endif // MQTT_FIXED_HEADER_HPP

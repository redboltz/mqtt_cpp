// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CONTROL_PACKET_TYPE_HPP)
#define MQTT_CONTROL_PACKET_TYPE_HPP

#include <cstdint>
#include <mqtt/namespace.hpp>

namespace MQTT_NS {

enum class control_packet_type : std::uint8_t {

    // reserved    =  0b0000000,
    connect     = 0b00010000, // 1
    connack     = 0b00100000, // 2
    publish     = 0b00110000, // 3
    puback      = 0b01000000, // 4
    pubrec      = 0b01010000, // 5
    pubrel      = 0b01100000, // 6
    pubcomp     = 0b01110000, // 7
    subscribe   = 0b10000000, // 8
    suback      = 0b10010000, // 9
    unsubscribe = 0b10100000, // 10
    unsuback    = 0b10110000, // 11
    pingreq     = 0b11000000, // 12
    pingresp    = 0b11010000, // 13
    disconnect  = 0b11100000, // 14
    auth        = 0b11110000, // 15

}; // namespace control_packet_type

constexpr control_packet_type get_control_packet_type(std::uint8_t v) {
    return static_cast<control_packet_type>(v & 0b11110000);
}

constexpr
char const* control_packet_type_to_str(control_packet_type v) {
    switch(v)
    {
    // case control_packet_type::reserved:    return "reserved";
    case control_packet_type::connect:    return "connect";
    case control_packet_type::connack:    return "connack";
    case control_packet_type::publish:    return "publish";
    case control_packet_type::puback:     return "puback";
    case control_packet_type::pubrec:     return "pubrec";
    case control_packet_type::pubrel:     return "pubrel";
    case control_packet_type::pubcomp:    return "pubcomp";
    case control_packet_type::subscribe:  return "subscribe";
    case control_packet_type::suback:     return "suback";
    case control_packet_type::pingreq:    return "pingreq";
    case control_packet_type::pingresp:   return "pingresp";
    case control_packet_type::disconnect: return "disconnect";
    case control_packet_type::auth:       return "auth";
    default:                              return "unknown_control_packet_type";
    }
}

template<typename Stream>
Stream & operator<<(Stream & os, control_packet_type val)
{
    os << control_packet_type_to_str(val);
    return os;
}

} // namespace MQTT_NS

#endif // MQTT_CONTROL_PACKET_TYPE_HPP

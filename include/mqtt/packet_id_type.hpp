// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PACKET_ID_TYPE_HPP)
#define MQTT_PACKET_ID_TYPE_HPP

#include <cstdint>
#include <cstdlib>

#include <mqtt/two_byte_util.hpp>
#include <mqtt/four_byte_util.hpp>

namespace mqtt {

template <std::size_t PacketIdBytes>
struct packet_id_type;

template <>
struct packet_id_type<2> {
    using type = std::uint16_t;
};

template <>
struct packet_id_type<4> {
    using type = std::uint32_t;
};

template <std::size_t PacketIdBytes>
struct make_packet_id;

template <>
struct make_packet_id<2> {
    template <typename It>
    static std::uint16_t apply(It b, It e) {
        return make_uint16_t(b, e);
    }
};

template <>
struct make_packet_id<4> {
    template <typename It>
    static std::uint32_t apply(It b, It e) {
        return make_uint32_t(b, e);
    }
};

template <std::size_t PacketIdBytes>
struct add_packet_id_to_buf;

template <>
struct add_packet_id_to_buf<2> {
    template <typename T>
    static void apply(T& buf, std::uint16_t packet_id) {
        add_uint16_t_to_buf(buf, packet_id);
    }
};

template <>
struct add_packet_id_to_buf<4> {
    template <typename T>
    static void apply(T& buf, std::uint32_t packet_id) {
        add_uint32_t_to_buf(buf, packet_id);
    }
};

} // namespace mqtt

#endif // MQTT_PACKET_ID_TYPE_HPP

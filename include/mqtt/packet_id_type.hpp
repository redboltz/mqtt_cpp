// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PACKET_ID_TYPE_HPP)
#define MQTT_PACKET_ID_TYPE_HPP

#include <mqtt/two_or_four_byte_util.hpp>

namespace MQTT_NS {

template <std::size_t PacketIdBytes>
using packet_id_type = two_or_four_byte_type<PacketIdBytes>;

template <std::size_t PacketIdBytes>
using make_packet_id = make_two_or_four_byte<PacketIdBytes>;

template <std::size_t PacketIdBytes>
using add_packet_id_to_buf = add_two_or_four_byte_to_buf<PacketIdBytes>;

} // namespace MQTT_NS

#endif // MQTT_PACKET_ID_TYPE_HPP

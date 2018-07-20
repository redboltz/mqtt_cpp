// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MESSAGE_VARIANT_HPP)
#define MQTT_MESSAGE_VARIANT_HPP

#include <mqtt/message.hpp>
#include <mqtt/v5_message.hpp>
#include <mqtt/variant.hpp>

namespace mqtt {

//  message_variant

template <std::size_t PacketIdBytes>
using basic_message_variant = variant<
    v3_1_1::connect_message,
    v3_1_1::connack_message,
    v3_1_1::basic_publish_message<PacketIdBytes>,
    v3_1_1::basic_puback_message<PacketIdBytes>,
    v3_1_1::basic_pubrec_message<PacketIdBytes>,
    v3_1_1::basic_pubrel_message<PacketIdBytes>,
    v3_1_1::basic_pubcomp_message<PacketIdBytes>,
    v3_1_1::basic_subscribe_message<PacketIdBytes>,
    v3_1_1::basic_suback_message<PacketIdBytes>,
    v3_1_1::basic_unsubscribe_message<PacketIdBytes>,
    v3_1_1::basic_unsuback_message<PacketIdBytes>,
    v3_1_1::pingreq_message,
    v3_1_1::pingresp_message,
    v3_1_1::disconnect_message,
    v5::connect_message,
    v5::connack_message,
    v5::basic_publish_message<PacketIdBytes>,
    v5::basic_puback_message<PacketIdBytes>,
    v5::basic_pubrec_message<PacketIdBytes>,
    v5::basic_pubrel_message<PacketIdBytes>,
    v5::basic_pubcomp_message<PacketIdBytes>,
    v5::basic_subscribe_message<PacketIdBytes>,
    v5::basic_suback_message<PacketIdBytes>,
    v5::basic_unsubscribe_message<PacketIdBytes>,
    v5::basic_unsuback_message<PacketIdBytes>,
    v5::pingreq_message,
    v5::pingresp_message,
    v5::disconnect_message,
    v5::auth_message
>;

using message_variant = basic_message_variant<2>;

namespace detail {

template <typename T>
struct is_shared_ptr {
    static constexpr bool value = false;
};

struct const_buffer_sequence_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<std::vector<as::const_buffer>>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    std::vector<as::const_buffer> operator()(T&& t) const {
        return t.const_buffer_sequence();
    }
};

struct size_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<std::size_t>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    std::size_t operator()(T&& t) const {
        return t.size();
    }
};

struct num_of_const_buffer_sequence_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<std::size_t>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    std::size_t operator()(T&& t) const {
        return t.num_of_const_buffer_sequence();
    }
};

struct continuous_buffer_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<std::string>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    std::string operator()(T&& t) const {
        return t.continuous_buffer();
    }
};

} // namespace detail

template <std::size_t PacketIdBytes>
inline std::vector<as::const_buffer> const_buffer_sequence(
    basic_message_variant<PacketIdBytes> const& mv) {
    return mqtt::visit(detail::const_buffer_sequence_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::size_t size(basic_message_variant<PacketIdBytes> const& mv) {
    return mqtt::visit(detail::size_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::size_t num_of_const_buffer_sequence(
    basic_message_variant<PacketIdBytes> const& mv) {
    return mqtt::visit(detail::num_of_const_buffer_sequence_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::string continuous_buffer(basic_message_variant<PacketIdBytes> const& mv) {
    return mqtt::visit(detail::continuous_buffer_visitor(), mv);
}


//  store_message_variant

template <std::size_t PacketIdBytes>
using basic_store_message_variant = variant<
    v3_1_1::basic_publish_message<PacketIdBytes>,
    v3_1_1::basic_pubrel_message<PacketIdBytes>,
    v5::basic_publish_message<PacketIdBytes>,
    v5::basic_pubrel_message<PacketIdBytes>
>;

using store_message_variant = basic_store_message_variant<2>;

namespace detail {

template <std::size_t PacketIdBytes>
struct basic_message_variant_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<basic_message_variant<PacketIdBytes>>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    basic_message_variant<PacketIdBytes> operator()(T&& t) const {
        return t;
    }
};

} // detail

template <std::size_t PacketIdBytes>
inline
basic_message_variant<PacketIdBytes> get_basic_message_variant(
    basic_store_message_variant<PacketIdBytes> const& smv) {
    return mqtt::visit(detail::basic_message_variant_visitor<PacketIdBytes>(), smv);
}

} // namespace mqtt

#endif // MQTT_MESSAGE_VARIANT_HPP

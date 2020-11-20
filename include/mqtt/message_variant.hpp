// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MESSAGE_VARIANT_HPP)
#define MQTT_MESSAGE_VARIANT_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/message.hpp>
#include <mqtt/v5_message.hpp>
#include <mqtt/variant.hpp>

namespace MQTT_NS {

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

struct const_buffer_sequence_visitor {
    template <typename T>
    std::vector<as::const_buffer> operator()(T&& t) const {
        return t.const_buffer_sequence();
    }
};

struct size_visitor {
    template <typename T>
    std::size_t operator()(T&& t) const {
        return t.size();
    }
};

struct num_of_const_buffer_sequence_visitor {
    template <typename T>
    std::size_t operator()(T&& t) const {
        return t.num_of_const_buffer_sequence();
    }
};

struct continuous_buffer_visitor {
    template <typename T>
    std::string operator()(T&& t) const {
        return std::forward<T>(t).continuous_buffer();
    }
};

} // namespace detail

template <std::size_t PacketIdBytes>
inline std::vector<as::const_buffer> const_buffer_sequence(
    basic_message_variant<PacketIdBytes> const& mv) {
    return MQTT_NS::visit(detail::const_buffer_sequence_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::size_t size(basic_message_variant<PacketIdBytes> const& mv) {
    return MQTT_NS::visit(detail::size_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::size_t num_of_const_buffer_sequence(
    basic_message_variant<PacketIdBytes> const& mv) {
    return MQTT_NS::visit(detail::num_of_const_buffer_sequence_visitor(), mv);
}

template <std::size_t PacketIdBytes>
inline std::string continuous_buffer(basic_message_variant<PacketIdBytes> const& mv) {
    return MQTT_NS::visit(detail::continuous_buffer_visitor(), mv);
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
struct basic_message_variant_visitor {
    template <typename T>
    basic_message_variant<PacketIdBytes> operator()(T const& t) const {
        return t;
    }
};

} // detail

template <std::size_t PacketIdBytes>
inline
basic_message_variant<PacketIdBytes> get_basic_message_variant(
    basic_store_message_variant<PacketIdBytes> smv) {
    return MQTT_NS::visit(detail::basic_message_variant_visitor<PacketIdBytes>(), smv);
}

template <std::size_t PacketIdBytes>
inline std::string continuous_buffer(basic_store_message_variant<PacketIdBytes> const& mv) {
    return MQTT_NS::visit(detail::continuous_buffer_visitor(), mv);
}

} // namespace MQTT_NS

#endif // MQTT_MESSAGE_VARIANT_HPP

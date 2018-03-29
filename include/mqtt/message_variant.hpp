// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIANT_MESSAGE_HPP)
#define MQTT_VARIANT_MESSAGE_HPP

#include <mqtt/message.hpp>
#include <mqtt/variant.hpp>

namespace mqtt {

//  message_variant

using message_variant = variant<
    connect_message,
    connack_message,
    publish_message,
    puback_message,
    pubrec_message,
    pubrel_message,
    pubcomp_message,
    subscribe_message,
    suback_message,
    unsubscribe_message,
    unsuback_message,
    pingreq_message,
    pingresp_message,
    disconnect_message
>;

namespace detail {

template <typename T>
struct is_shared_ptr {
    static constexpr bool value = false;
};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> {
    static constexpr bool value = true;
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

inline std::vector<as::const_buffer> const_buffer_sequence(message_variant const& mv) {
    return visit(detail::const_buffer_sequence_visitor(), mv);
}

inline std::size_t size(message_variant const& mv) {
    return visit(detail::size_visitor(), mv);
}

inline std::string continuous_buffer(message_variant const& mv) {
    return visit(detail::continuous_buffer_visitor(), mv);
}


//  store_message_variant

using store_message_variant = variant<
    publish_message,
    pubrel_message
>;

namespace detail {

struct message_variant_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<message_variant>
#endif // !defined(MQTT_STD_VARIANT)

{
    template <typename T>
    message_variant operator()(T&& t) const {
        return t;
    }
};

} // detail

inline message_variant get_message_variant(store_message_variant const& smv) {
    return visit(detail::message_variant_visitor(), smv);
}

} // namespace mqtt

#endif // MQTT_VARIANT_MESSAGE_HPP

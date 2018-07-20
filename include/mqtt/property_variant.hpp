// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_VARIANT_HPP)
#define MQTT_PROPERTY_VARIANT_HPP

#include <mqtt/property.hpp>
#include <mqtt/variant.hpp>

namespace mqtt {

namespace v5 {
//  property_variant

using property_variant = variant<
    property::payload_format_indicator,
    property::message_expiry_interval,
    property::content_type,
    property::content_type_ref,
    property::response_topic,
    property::response_topic_ref,
    property::correlation_data,
    property::correlation_data_ref,
    property::subscription_identifier,
    property::session_expiry_interval,
    property::assigned_client_identifier,
    property::assigned_client_identifier_ref,
    property::server_keep_alive,
    property::authentication_method,
    property::authentication_method_ref,
    property::authentication_data,
    property::authentication_data_ref,
    property::request_problem_information,
    property::will_delay_interval,
    property::request_response_information,
    property::response_information,
    property::response_information_ref,
    property::server_reference,
    property::server_reference_ref,
    property::reason_string,
    property::reason_string_ref,
    property::receive_maximum,
    property::topic_alias_maximum,
    property::topic_alias,
    property::maximum_qos,
    property::retain_available,
    property::user_property,
    property::user_property_ref,
    property::maximum_packet_size,
    property::wildcard_subscription_available,
    property::subscription_identifier_available,
    property::shared_subscription_available
>;

namespace property {

namespace detail {

struct add_const_buffer_sequence_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<void>
#endif // !defined(MQTT_STD_VARIANT)

{
    add_const_buffer_sequence_visitor(std::vector<as::const_buffer>& v):v(v) {}
    template <typename T>
    void operator()(T&& t) const {
        t.add_const_buffer_sequence(v);
    }
    std::vector<as::const_buffer>& v;
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

template <typename Iterator>
struct fill_visitor

#if !defined(MQTT_STD_VARIANT)
    : boost::static_visitor<void>
#endif // !defined(MQTT_STD_VARIANT)

{
    fill_visitor(Iterator b, Iterator e):b(b), e(e) {}

    template <typename T>
    void operator()(T&& t) const {
        t.fill(b, e);
    }

    Iterator b;
    Iterator e;
};

template <typename Iterator>
inline fill_visitor<Iterator> make_fill_visitor(Iterator b, Iterator e) {
    return fill_visitor<Iterator>(b, e);
}

} // namespace detail

} // namespace property

inline void add_const_buffer_sequence(std::vector<as::const_buffer>& v, property_variant const& pv) {
    mqtt::visit(property::detail::add_const_buffer_sequence_visitor(v), pv);
}

inline std::size_t size(property_variant const& pv) {
    return mqtt::visit(property::detail::size_visitor(), pv);
}

inline std::size_t num_of_const_buffer_sequence(property_variant const& pv) {
    return mqtt::visit(property::detail::num_of_const_buffer_sequence_visitor(), pv);
}


template <typename Iterator>
inline void fill(property_variant const& pv, Iterator b, Iterator e) {
    auto vis = property::detail::make_fill_visitor(b, e);
    mqtt::visit(vis, pv);
}

} // namespace v5

} // namespace mqtt

#endif // MQTT_PROPERTY_VARIANT_HPP

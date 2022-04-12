// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_VARIANT_HPP)
#define MQTT_PROPERTY_VARIANT_HPP

#include <vector>

#include <mqtt/namespace.hpp>
#include <mqtt/property.hpp>
#include <mqtt/variant.hpp>
#include <mqtt/visitor_util.hpp>

namespace MQTT_NS {

namespace v5 {
//  property_variant

using property_variant = variant<
    property::payload_format_indicator,
    property::message_expiry_interval,
    property::content_type,
    property::response_topic,
    property::correlation_data,
    property::subscription_identifier,
    property::session_expiry_interval,
    property::assigned_client_identifier,
    property::server_keep_alive,
    property::authentication_method,
    property::authentication_data,
    property::request_problem_information,
    property::will_delay_interval,
    property::request_response_information,
    property::response_information,
    property::server_reference,
    property::reason_string,
    property::receive_maximum,
    property::topic_alias_maximum,
    property::topic_alias,
    property::maximum_qos,
    property::retain_available,
    property::user_property,
    property::maximum_packet_size,
    property::wildcard_subscription_available,
    property::subscription_identifier_available,
    property::shared_subscription_available
>;

using properties = std::vector<property_variant>;

namespace property {

namespace detail {

struct add_const_buffer_sequence_visitor {
    add_const_buffer_sequence_visitor(std::vector<as::const_buffer>& v):v(v) {}
    template <typename T>
    void operator()(T&& t) const {
        t.add_const_buffer_sequence(v);
    }
    std::vector<as::const_buffer>& v;
};

struct id_visitor {
    template <typename T>
    id operator()(T const& t) const {
        return t.id();
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

template <typename Iterator>
struct fill_visitor {
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
    MQTT_NS::visit(property::detail::add_const_buffer_sequence_visitor(v), pv);
}

inline property::id id(property_variant const& pv) {
    return MQTT_NS::visit(property::detail::id_visitor(), pv);
}

inline std::size_t size(property_variant const& pv) {
    return MQTT_NS::visit(property::detail::size_visitor(), pv);
}

inline std::size_t num_of_const_buffer_sequence(property_variant const& pv) {
    return MQTT_NS::visit(property::detail::num_of_const_buffer_sequence_visitor(), pv);
}


template <typename Iterator>
inline void fill(property_variant const& pv, Iterator b, Iterator e) {
    auto vis = property::detail::make_fill_visitor(b, e);
    MQTT_NS::visit(vis, pv);
}

struct less_than_visitor {
    template <typename T>
    bool operator()(T const& lhs, T const& rhs) const {
        return lhs < rhs;
    }
    template <typename T, typename U>
    bool operator()(T const& lhs, U const& rhs) const {
        return lhs.id() < rhs.id();
    }
};

template <typename PropertyVariant>
inline
std::enable_if_t<
    std::is_same<PropertyVariant, property_variant>::value,
    bool
>
operator<(PropertyVariant const& lhs, PropertyVariant const& rhs) {
    return MQTT_NS::visit(
        less_than_visitor(),
        lhs,
        rhs
    );
}

struct equal_visitor {
    template <typename T>
    bool operator()(T const& lhs, T const& rhs) const {
        return lhs == rhs;
    }
    template <typename T, typename U>
    bool operator()(T const&, U const&) const {
        return false;
    }
};

template <typename PropertyVariant>
inline
std::enable_if_t<
    std::is_same<PropertyVariant, property_variant>::value,
    bool
>
operator==(PropertyVariant const& lhs, PropertyVariant const& rhs) {
    return MQTT_NS::visit(
        equal_visitor(),
        lhs,
        rhs
    );
}

template <typename PropertyVariant>
inline
std::enable_if_t<
    std::is_same<PropertyVariant, property_variant>::value,
    bool
>
operator!=(PropertyVariant const& lhs, PropertyVariant const& rhs) {
    return !MQTT_NS::visit(
        equal_visitor(),
        lhs,
        rhs
    );
}

template <typename... Visitors>
inline
void
visit_prop(property_variant const& prop, Visitors&&... visitors) {
    MQTT_NS::visit(
        make_lambda_visitor(std::forward<Visitors>(visitors)...), prop
    );
}

template <typename... Visitors>
inline
void
visit_props(properties const& props, Visitors&&... visitors) {
    for (auto const& prop : props) {
        visit_prop(
            prop,
            std::forward<Visitors>(visitors)...
        );
    }
}

template <typename... Visitors>
inline
void
visit_prop(property_variant&& prop, Visitors&&... visitors) {
    MQTT_NS::visit(
        make_lambda_visitor(std::forward<Visitors>(visitors)...), force_move(prop)
    );
}

template <typename... Visitors>
inline
void
visit_props(properties&& props, Visitors&&... visitors) {
    for (auto&& prop : force_move(props)) {
        visit_prop(
            force_move(prop),
            std::forward<Visitors>(visitors)...
        );
    }
}


} // namespace v5

} // namespace MQTT_NS

#endif // MQTT_PROPERTY_VARIANT_HPP

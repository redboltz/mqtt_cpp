// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_PROPERTY_UTIL_HPP)
#define MQTT_BROKER_PROPERTY_UTIL_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/visitor_util.hpp>

MQTT_BROKER_NS_BEGIN

template <typename T>
inline optional<T> get_property(v5::properties const& props) {
    optional<T> result;

    auto visitor = make_lambda_visitor(
        [&result](T const& t) { result = t; },
        [](auto const&) { }
    );

    for (auto const& p : props) {
        MQTT_NS::visit(visitor, p);
    }

    return result;
}

template <typename T>
inline void set_property(v5::properties& props, T&& v) {
    auto visitor = make_lambda_visitor(
        [&v](T& t) mutable { t = std::forward<T>(v); },
        [](auto&) { }
    );

    for (auto& p : props) {
        MQTT_NS::visit(visitor, p);
    }
}

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_PROPERTY_UTIL_HPP

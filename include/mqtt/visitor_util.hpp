// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VISITOR_UTIL_HPP)
#define MQTT_VISITOR_UTIL_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/variant.hpp>

namespace MQTT_NS {

template <typename... Lambdas>
struct lambda_visitor;

template <typename Lambda1, typename... Lambdas>
struct lambda_visitor<Lambda1, Lambdas...>
    : Lambda1, lambda_visitor<Lambdas...> {
    using Lambda1::operator();
    using lambda_visitor<Lambdas...>::operator();
    lambda_visitor(Lambda1 lambda1, Lambdas... lambdas)
        : Lambda1(lambda1), lambda_visitor<Lambdas...>(lambdas...) {}
};


template <typename Lambda1>
struct lambda_visitor<Lambda1> : Lambda1 {
    using Lambda1::operator();
    lambda_visitor(Lambda1 lambda1)
        : Lambda1(lambda1) {}
};


template <typename... Lambdas>
inline lambda_visitor<Lambdas...> make_lambda_visitor(Lambdas&&... lambdas) {
    return { std::forward<Lambdas>(lambdas)... };
}

} // namespace MQTT_NS

#endif // MQTT_VISITOR_UTIL_HPP

// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIANT_HPP)
#define MQTT_VARIANT_HPP

#if defined(MQTT_STD_VARIANT)

#include <variant>

#else  // defined(MQTT_STD_VARIANT)

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>

#endif // defined(MQTT_STD_VARIANT)

namespace mqtt {

#if defined(MQTT_STD_VARIANT)

template <typename... Types>
using variant = std::variant<Types...>;

template <typename Visitor, typename... Variants>
constexpr auto visit(Visitor&& vis, Variants&&... vars) {
    return std::visit(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
}

#else  // defined(MQTT_STD_VARIANT)

template <typename... Types>
using variant = boost::variant<Types...>;

template <typename Visitor, typename... Variants>
constexpr auto visit(Visitor&& vis, Variants&&... vars)
    -> decltype(boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...))
{
    return boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
}

#endif // defined(MQTT_STD_VARIANT)


} // namespace mqtt

#endif // MQTT_VARIANT_HPP

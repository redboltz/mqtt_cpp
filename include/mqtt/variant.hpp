// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIANT_HPP)
#define MQTT_VARIANT_HPP

#include <mqtt/config.hpp>
#include <mqtt/namespace.hpp>

#if defined(MQTT_STD_VARIANT)

#include <variant>

namespace MQTT_NS {

using std::variant;

template<typename T, typename U>
decltype(auto) variant_get(U && arg)
{
    return std::get<T>(std::forward<U>(arg));
}

template<typename T>
decltype(auto) variant_idx(T const& arg)
{
    return arg.index();
}

using std::visit;

} // namespace MQTT_NS

#else  // defined(MQTT_STD_VARIANT)

#include <boost/variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/apply_visitor.hpp>

namespace MQTT_NS {

using boost::variant;

template<typename T, typename U>
decltype(auto) variant_get(U && arg)
{
    return boost::get<T>(std::forward<U>(arg));
}

template<typename T>
decltype(auto) variant_idx(T const& arg)
{
    return arg.which();
}

template <typename Visitor, typename... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... vars)
{
    return boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
}

} // namespace MQTT_NS

#endif // defined(MQTT_STD_VARIANT)

#endif // MQTT_VARIANT_HPP

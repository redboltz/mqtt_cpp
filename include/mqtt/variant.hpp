// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIANT_HPP)
#define MQTT_VARIANT_HPP

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

// user intentionally defined BOOST_MPL_LIMIT_LIST_SIZE but size is too small
// NOTE: if BOOST_MPL_LIMIT_LIST_SIZE is not defined, the value is evaluate as 0.
#if defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40

#error BOOST_MPL_LIMIT_LIST_SIZE need to greator or equal to 40

#else  // defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40

// user doesn't define BOOST_MPL_LIMIT_LIST_SIZE intentionally
// but the defult value could be defined

#undef BOOST_MPL_LIMIT_LIST_SIZE
#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_LIMIT_LIST_SIZE 40

#endif // defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40

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

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


#if defined(BOOST_MPL_LIMIT_LIST_SIZE)

#if BOOST_MPL_LIMIT_LIST_SIZE < 40
#error BOOST_MPL_LIMIT_LIST_SIZE need to greator or equal to 40
#endif // BOOST_MPL_LIMIT_LIST_SIZE < 40

#else  // defined(BOOST_MPL_LIMIT_LIST_SIZE)

#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_LIMIT_LIST_SIZE 40

#endif // defined(BOOST_MPL_LIMIT_LIST_SIZE)

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>

#endif // defined(MQTT_STD_VARIANT)

namespace mqtt {

#if defined(MQTT_STD_VARIANT)

using std::variant;

using std::visit;

#else  // defined(MQTT_STD_VARIANT)

using boost::variant;

template <typename Visitor, typename... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... vars)
{
    return boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
}

#endif // defined(MQTT_STD_VARIANT)


} // namespace mqtt

#endif // MQTT_VARIANT_HPP

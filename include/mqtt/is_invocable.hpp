// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_IS_INVOCABLE_HPP)
#define MQTT_IS_INVOCABLE_HPP

#include <type_traits>
#include <boost/callable_traits.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

#if __cplusplus >= 201703L

template <typename Func, typename... Params>
using is_invocable = typename std::is_invocable<Func, Params...>;

#else  // __cplusplus >= 201703L

template <typename Func, typename... Params>
using is_invocable = typename boost::callable_traits::is_invocable<Func, Params...>;

#endif // __cplusplus >= 201703L

} // namespace MQTT_NS

#endif // MQTT_IS_INVOCABLE_HPP

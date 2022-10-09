// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_IS_INVOCABLE_HPP)
#define MQTT_IS_INVOCABLE_HPP

#include <type_traits>
#include <mqtt/namespace.hpp>

#if __cplusplus >= 201703L

namespace MQTT_NS {

template <typename Func, typename... Params>
using is_invocable = typename std::is_invocable<Func, Params...>;

} // namespace MQTT_NS

#else  // __cplusplus >= 201703L

#include <mqtt/move_only_function.hpp>

namespace MQTT_NS {

template <typename Func, typename... Params>
struct is_invocable : std::is_constructible<
    move_only_function<void(Params...)>,
    std::reference_wrapper<typename std::remove_reference<Func>::type>
>
{};

} // namespace MQTT_NS

#endif // __cplusplus >= 201703L

#endif // MQTT_IS_INVOCABLE_HPP

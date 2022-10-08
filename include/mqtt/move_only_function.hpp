// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MOVE_ONLY_FUNCTION_HPP)
#define MQTT_MOVE_ONLY_FUNCTION_HPP

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#endif // defined(__GNUC__)

#include <mqtt/external/function2.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

template <typename... Params>
using move_only_function = fu2::unique_function<Params...>;

} // namespace MQTT_NS

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__)

#endif // MQTT_MOVE_ONLY_FUNCTION_HPP

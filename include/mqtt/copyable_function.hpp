// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_COPYABLE_FUNCTION_HPP)
#define MQTT_COPYABLE_FUNCTION_HPP

#pragma GCC diagnostic push
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Waddress"
#endif // defined(__GNUC__)

#include <mqtt/external/function2.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

template <typename... Params>
using copyable_function = fu2::function<Params...>;

} // namespace MQTT_NS

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__)

#endif // MQTT_COPYABLE_FUNCTION_HPP

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TIME_POINT_T_HPP)
#define MQTT_TIME_POINT_T_HPP

#include <mqtt/config.hpp>

#include <chrono>

namespace MQTT_NS {

using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

} // namespace MQTT_NS

#endif // MQTT_TIME_POINT_T_HPP

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_TIME_POINT_T_HPP)
#define MQTT_BROKER_TIME_POINT_T_HPP

#include <mqtt/config.hpp>

#include <chrono>

#include <mqtt/broker/broker_namespace.hpp>

MQTT_BROKER_NS_BEGIN

using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_TIME_POINT_T_HPP

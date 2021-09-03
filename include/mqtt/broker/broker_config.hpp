// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_CONFIG_HPP)
#define MQTT_BROKER_CONFIG_HPP

#include <mqtt/config.hpp>

#include <chrono>

#include <mqtt/broker/broker_namespace.hpp>

MQTT_BROKER_NS_BEGIN

static constexpr std::chrono::seconds windows_close_delay(3);

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_CONFIG_HPP

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_BROKER_NAMESPACE_HPP)
#define MQTT_BROKER_BROKER_NAMESPACE_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>

#if __cplusplus >= 201703L

#define MQTT_BROKER_NS_BEGIN namespace MQTT_NS::broker {
#define MQTT_BROKER_NS_END }

#else  // __cplusplus >= 201703L

#define MQTT_BROKER_NS_BEGIN namespace MQTT_NS { namespace broker {
#define MQTT_BROKER_NS_END } }

#endif // __cplusplus >= 201703L

#endif // MQTT_BROKER_BROKER_NAMESPACE_HPP

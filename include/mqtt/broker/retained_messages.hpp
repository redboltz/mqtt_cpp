// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_RETAINED_MESSAGES_HPP)
#define MQTT_BROKER_RETAINED_MESSAGES_HPP

#include <mqtt/config.hpp>

#include <functional> // reference_wrapper

#include <mqtt/broker/broker_namespace.hpp>

#include <mqtt/broker/retain_t.hpp>
#include <mqtt/broker/retained_topic_map.hpp>

MQTT_BROKER_NS_BEGIN

using retained_messages = retained_topic_map<retain_t>;

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_RETAINED_MESSAGES_HPP

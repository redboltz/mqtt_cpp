// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SESSION_STATE_FWD_HPP)
#define MQTT_BROKER_SESSION_STATE_FWD_HPP

#include <mqtt/config.hpp>

#include <functional> // reference_wrapper

#include <mqtt/broker/broker_namespace.hpp>

MQTT_BROKER_NS_BEGIN

struct session_state;

using session_state_ref = std::reference_wrapper<session_state>;

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SESSION_STATE_FWD_HPP

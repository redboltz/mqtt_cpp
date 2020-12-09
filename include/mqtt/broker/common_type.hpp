// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_COMMON_TYPE_HPP)
#define MQTT_BROKER_COMMON_TYPE_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/server.hpp>

MQTT_BROKER_NS_BEGIN

using endpoint_t = server<>::endpoint_t;
using con_sp_t = std::shared_ptr<endpoint_t>;
using con_wp_t = std::weak_ptr<endpoint_t>;
using packet_id_t = endpoint_t::packet_id_t;

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_COMMON_TYPE_HPP

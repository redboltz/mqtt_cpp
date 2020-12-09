// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SUB_CON_MAP_HPP)
#define MQTT_BROKER_SUB_CON_MAP_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/broker/subscription_map.hpp>
#include <mqtt/broker/subscription.hpp>

MQTT_BROKER_NS_BEGIN

struct buffer_hasher  {
    std::size_t operator()(buffer const& b) const noexcept {
        std::size_t result = 0;
        boost::hash_combine(result, b);
        return result;
    }
};

using sub_con_map = multiple_subscription_map<buffer, subscription, buffer_hasher>;

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SUB_CON_MAP_HPP

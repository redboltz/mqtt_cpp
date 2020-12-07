// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SUBSCRIPTION_HPP)
#define MQTT_BROKER_SUBSCRIPTION_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/subscribe_options.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/broker/session_state_fwd.hpp>

MQTT_BROKER_NS_BEGIN

struct subscription {
    subscription(
        session_state_ref ss,
        buffer share_name,
        buffer topic_filter,
        subscribe_options subopts,
        optional<std::size_t> sid)
        :ss { ss },
         share_name { force_move(share_name) },
         topic_filter { force_move(topic_filter) },
         subopts { subopts },
         sid { sid }
    {}

    session_state_ref ss;
    buffer share_name;
    buffer topic_filter;
    subscribe_options subopts;
    optional<std::size_t> sid;
};

inline bool operator<(subscription const& lhs, subscription const& rhs) {
    return &lhs.ss.get() < &rhs.ss.get();
}

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SUBSCRIPTION_HPP

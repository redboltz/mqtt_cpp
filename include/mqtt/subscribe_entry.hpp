// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIBE_ENTRY_HPP)
#define MQTT_SUBSCRIBE_ENTRY_HPP

#include <mqtt/config.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/subscribe_options.hpp>

namespace MQTT_NS {

struct subscribe_entry {
    subscribe_entry(
        buffer share_name,
        buffer topic_filter,
        subscribe_options subopts)
        : share_name { force_move(share_name) },
          topic_filter { force_move(topic_filter) },
          subopts { subopts }
        {}

    subscribe_entry(
        buffer topic_filter,
        subscribe_options subopts)
        : topic_filter { force_move(topic_filter) },
          subopts { subopts }
        {}

    // empty share name means no share name
    // $share//topic_filter is protocol error
    //
    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901250
    // A Shared Subscription's Topic Filter MUST start with $share/ and MUST contain
    // a ShareName that is at least one character long [MQTT-4.8.2-1].
    buffer share_name;
    buffer topic_filter;
    subscribe_options subopts;
};

struct unsubscribe_entry {
    unsubscribe_entry(
        buffer share_name,
        buffer topic_filter)
        : share_name { force_move(share_name) },
          topic_filter { force_move(topic_filter) }
        {}

    unsubscribe_entry(
        buffer topic_filter)
        : topic_filter { force_move(topic_filter) }
        {}

    buffer share_name;
    buffer topic_filter;
};

} // namespace MQTT_NS

#endif // MQTT_SUBSCRIBE_ENTRY_HPP

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#if !defined(MQTT_SHARED_SUBSCRIPTIONS_HPP)
#define MQTT_SHARED_SUBSCRIPTIONS_HPP

#include <mqtt/config.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/move.hpp>

namespace MQTT_NS {

struct share_name_topic_filter {
    share_name_topic_filter(buffer share_name, buffer topic_filter)
        : share_name { force_move(share_name) }, topic_filter{ force_move(topic_filter) }
    {}
    buffer share_name;
    buffer topic_filter;
};

inline bool operator<(share_name_topic_filter const& lhs, share_name_topic_filter const& rhs) {
    if (lhs.share_name < rhs.share_name) return true;
    if (rhs.share_name < lhs.share_name) return false;
    return lhs.topic_filter < rhs.topic_filter;
}

inline bool operator==(share_name_topic_filter const& lhs, share_name_topic_filter const& rhs) {
    return lhs.share_name == rhs.share_name && lhs.topic_filter == rhs.topic_filter;
}

inline bool operator!=(share_name_topic_filter const& lhs, share_name_topic_filter const& rhs) {
    return !(lhs == rhs);
}


inline share_name_topic_filter parse_shared_subscription(buffer whole_topic_filter) {
    auto const shared_prefix = string_view("$share/");
    if (whole_topic_filter.substr(0, shared_prefix.size()) != shared_prefix) {
        return { buffer{}, force_move(whole_topic_filter) };
    }

    // Remove $share/
    whole_topic_filter.remove_prefix(shared_prefix.size());

    // This is the '/' seperating the subscription group from the actual topic_filter.
    auto const idx = whole_topic_filter.find_first_of('/');

    // We return the share and the topic as buffers that point to the same
    // storage. So we grab the substr for "share", and then remove it from whole_topic_filter.
    auto share = whole_topic_filter.substr(0, idx);
    whole_topic_filter.remove_prefix(idx + 1);

    return { force_move(share), force_move(whole_topic_filter) };
}

} // namespace MQTT_NS

#endif // MQTT_SHARED_SUBSCRIPTIONS_HPP

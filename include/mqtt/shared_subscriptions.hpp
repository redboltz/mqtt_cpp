// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#if !defined(MQTT_SHARED_SUBSCRIPTIONS_HPP)
#define MQTT_SHARED_SUBSCRIPTIONS_HPP

#include <mqtt/config.hpp>

#include <utility>
#include <type_traits>

#include <boost/assert.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/move.hpp>
#include <mqtt/optional.hpp>

namespace MQTT_NS {

struct share_name_topic_filter {
    share_name_topic_filter(buffer share_name, buffer topic_filter)
        : share_name { force_move(share_name) }, topic_filter{ force_move(topic_filter) }
    {
        BOOST_ASSERT(!topic_filter.empty());
    }

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


inline optional<share_name_topic_filter> parse_shared_subscription(buffer whole_topic_filter) {
    auto const shared_prefix = string_view("$share/");
    if (whole_topic_filter.substr(0, shared_prefix.size()) != shared_prefix) {
        return share_name_topic_filter{ buffer{}, force_move(whole_topic_filter) };
    }

    // Remove $share/
    whole_topic_filter.remove_prefix(shared_prefix.size());

    // This is the '/' seperating the subscription group from the actual topic_filter.
    auto const idx = whole_topic_filter.find_first_of('/');
    if (idx == string_view::npos) return nullopt;

    // We return the share_name and the topic_filter as buffers that point to the same
    // storage. So we grab the substr for "share", and then remove it from whole_topic_filter.
    auto share_name = whole_topic_filter.substr(0, idx);
    whole_topic_filter.remove_prefix(std::min(idx + 1, whole_topic_filter.size()));

    if (share_name.empty() || whole_topic_filter.empty()) return nullopt;
    return share_name_topic_filter{ force_move(share_name), force_move(whole_topic_filter) };
}

namespace detail {

template <typename T, typename U>
inline buffer create_topic_filter_buffer(T const& share_name, U const& topic_filter) {
    string_view prefix = "$share/";
    // 1 is the length of '/' between share_name and topic_filter
    auto spa = make_shared_ptr_array(prefix.size() + share_name.size() + 1 + topic_filter.size());
    auto it = spa.get();
    auto start = it;
    std::copy(prefix.begin(), prefix.end(), it);
    it += prefix.size();
    std::copy(share_name.begin(), share_name.end(), it);
    it += share_name.size();
    *it++ = '/';
    std::copy(topic_filter.begin(), topic_filter.end(), it);
    it += topic_filter.size();
    return buffer(string_view(start, static_cast<std::size_t>(it - start)), force_move(spa));
}

} // namespace detail

inline buffer create_topic_filter_buffer(string_view  share_name, string_view topic_filter) {
    if (share_name.empty()) return allocate_buffer(topic_filter);
    return detail::create_topic_filter_buffer(share_name, topic_filter);
}
inline buffer create_topic_filter_buffer(string_view share_name, buffer topic_filter) {
    if (share_name.empty()) return topic_filter;
    return detail::create_topic_filter_buffer(share_name, topic_filter);
}

} // namespace MQTT_NS

#endif // MQTT_SHARED_SUBSCRIPTIONS_HPP

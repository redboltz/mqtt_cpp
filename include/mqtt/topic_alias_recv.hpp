// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TOPIC_ALIAS_RECV_HPP)
#define MQTT_TOPIC_ALIAS_RECV_HPP

#include <string>
#include <map>
#include <array>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/constant.hpp>
#include <mqtt/type.hpp>
#include <mqtt/log.hpp>

namespace MQTT_NS {

using topic_alias_recv_map_t = std::map<topic_alias_t, std::string>;

inline void register_topic_alias(topic_alias_recv_map_t& m,  string_view topic, topic_alias_t alias) {
    BOOST_ASSERT(alias > 0); //alias <= topic_alias_max is always true

    MQTT_LOG("mqtt_impl", info)
        << MQTT_ADD_VALUE(address, &m)
        << "register_topic_alias"
        << " topic:" << topic
        << " alias:" << alias;

    if (topic.empty()) {
        m.erase(alias);
    }
    else {
        m[alias] = std::string(topic); // overwrite
    }
}

inline std::string find_topic_by_alias(topic_alias_recv_map_t const& m,  topic_alias_t alias) {
    BOOST_ASSERT(alias > 0); //alias <= topic_alias_max is always true

    std::string topic;
    auto it = m.find(alias);
    if (it != m.end()) topic = it->second;

    MQTT_LOG("mqtt_impl", info)
        << MQTT_ADD_VALUE(address, &m)
        << "find_topic_by_alias"
        << " alias:" << alias
        << " topic:" << topic;

    return topic;
}

inline void clear_topic_alias(topic_alias_recv_map_t& m) {
    MQTT_LOG("mqtt_impl", info)
        << MQTT_ADD_VALUE(address, &m)
        << "clear_topic_alias";

    m.clear();
}

} // namespace MQTT_NS

#endif // MQTT_TOPIC_ALIAS_RECV_HPP

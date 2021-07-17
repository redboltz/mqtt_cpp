// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TOPIC_ALIAS_RECV_HPP)
#define MQTT_TOPIC_ALIAS_RECV_HPP

#include <string>
#include <unordered_map>
#include <array>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/constant.hpp>
#include <mqtt/type.hpp>
#include <mqtt/move.hpp>
#include <mqtt/log.hpp>

namespace MQTT_NS {

namespace mi = boost::multi_index;

class topic_alias_recv {
public:
    topic_alias_recv(topic_alias_t max)
        :max_{max} {}

    void insert_or_update(string_view topic, topic_alias_t alias) {
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "topic_alias_recv insert"
            << " topic:" << topic
            << " alias:" << alias;
        BOOST_ASSERT(!topic.empty() && alias >= min_ && alias <= max_);
        auto it = aliases_.lower_bound(alias);
        if (it == aliases_.end() || it->alias != alias) {
            aliases_.emplace_hint(it, std::string(topic), alias);
        }
        else {
            aliases_.modify(
                it,
                [&](entry& e) {
                    e.topic = std::string{topic};
                },
                [](auto&) { BOOST_ASSERT(false); }
            );

        }
    }

    std::string find(topic_alias_t alias) const {
        BOOST_ASSERT(alias >= min_ && alias <= max_);
        std::string topic;
        auto it = aliases_.find(alias);
        if (it != aliases_.end()) topic = it->topic;

        MQTT_LOG("mqtt_impl", info)
            << MQTT_ADD_VALUE(address, this)
            << "find_topic_by_alias"
            << " alias:" << alias
            << " topic:" << topic;

        return topic;
    }

    void clear() {
        MQTT_LOG("mqtt_impl", info)
            << MQTT_ADD_VALUE(address, this)
            << "clear_topic_alias";
        aliases_.clear();
    }

    topic_alias_t max() const { return max_; }

private:
    static constexpr topic_alias_t min_ = 1;
    topic_alias_t max_;

    struct entry {
        entry(std::string topic, topic_alias_t alias)
            : topic{force_move(topic)}, alias{alias} {}

        std::string topic;
        topic_alias_t alias;
    };
    using mi_topic_alias = mi::multi_index_container<
        entry,
        mi::indexed_by<
            mi::ordered_unique<
                BOOST_MULTI_INDEX_MEMBER(entry, topic_alias_t, alias)
            >
        >
    >;

    mi_topic_alias aliases_;
};

} // namespace MQTT_NS

#endif // MQTT_TOPIC_ALIAS_RECV_HPP

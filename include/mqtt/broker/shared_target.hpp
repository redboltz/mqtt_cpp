// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SHARED_TARGET_HPP)
#define MQTT_BROKER_SHARED_TARGET_HPP

#include <mqtt/config.hpp>

#include <set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <mqtt/buffer.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/time_point_t.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/broker/session_state_fwd.hpp>
#include <mqtt/broker/tags.hpp>
#include <mqtt/broker/mutex.hpp>

MQTT_BROKER_NS_BEGIN

namespace mi = boost::multi_index;

class shared_target {
public:
    void insert(buffer share_name, buffer topic_filter, session_state& ss);
    void erase(buffer share_name, buffer topic_filter, session_state const& ss);
    void erase(session_state const& ss);
    optional<session_state_ref> get_target(buffer const& share_name, buffer const& topic_filter);

private:
    struct entry {
        entry(buffer share_name, session_state& ss, time_point_t tp);

        buffer const& client_id() const;
        buffer share_name;
        session_state_ref ssr;
        time_point_t tp;
        std::set<buffer> topic_filters;
    };

    using mi_shared_target = mi::multi_index_container<
        entry,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_cid_sn>,
                mi::composite_key<
                    entry,
                    BOOST_MULTI_INDEX_CONST_MEM_FUN(entry, buffer const&, client_id),
                    BOOST_MULTI_INDEX_MEMBER(entry, buffer, share_name)
                >
            >,
            mi::ordered_non_unique<
                mi::tag<tag_sn_tp>,
                mi::composite_key<
                    entry,
                    BOOST_MULTI_INDEX_MEMBER(entry, buffer, share_name),
                    BOOST_MULTI_INDEX_MEMBER(entry, time_point_t, tp)
                >
            >
        >
    >;

    mutable mutex mtx_targets_;
    mi_shared_target targets_;
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SHARED_TARGET_HPP

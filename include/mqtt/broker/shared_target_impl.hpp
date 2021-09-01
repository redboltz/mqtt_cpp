// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SHARED_TARGET_IMPL_HPP)
#define MQTT_BROKER_SHARED_TARGET_IMPL_HPP

#include <mqtt/broker/shared_target.hpp>
#include <mqtt/broker/session_state.hpp>

MQTT_BROKER_NS_BEGIN

inline void shared_target::insert(buffer share_name, buffer topic_filter, session_state& ss) {
    std::lock_guard<mutex> g{mtx_targets_};
    auto& idx = targets_.get<tag_cid_sn>();
    auto it = idx.lower_bound(std::make_tuple(ss.client_id(), share_name));
    if (it == idx.end() || (it->share_name != share_name || it->client_id() != ss.client_id())) {
        it = idx.emplace_hint(it, force_move(share_name), ss, std::chrono::steady_clock::now());

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& st = const_cast<entry&>(*it);
        bool inserted;
        std::tie(std::ignore, inserted) = st.topic_filters.insert(force_move(topic_filter));
        BOOST_ASSERT(inserted);
    }
    else {
        // entry exists

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& st = const_cast<entry&>(*it);
        st.topic_filters.insert(force_move(topic_filter)); // ignore overwrite
    }
}

inline void shared_target::erase(buffer share_name, buffer topic_filter, session_state const& ss) {
    std::lock_guard<mutex> g{mtx_targets_};
    auto& idx = targets_.get<tag_cid_sn>();
    auto it = idx.find(std::make_tuple(ss.client_id(), share_name));
    if (it == idx.end()) {
        MQTT_LOG("mqtt_broker", warning)
            << "attempt to erase non exist entry"
            << " share_name:" << share_name
            << " topic_filtere:" << topic_filter
            << " client_id:" << ss.client_id();
        return;
    }

    // entry exists

    // const_cast is appropriate here
    // See https://github.com/boostorg/multi_index/issues/50
    auto& st = const_cast<entry&>(*it);
    st.topic_filters.erase(topic_filter);
    if (it->topic_filters.empty()) {
        idx.erase(it);
    }
}

inline void shared_target::erase(session_state const& ss) {
    std::lock_guard<mutex> g{mtx_targets_};
    auto& idx = targets_.get<tag_cid_sn>();
    auto r = idx.equal_range(ss.client_id());
    idx.erase(r.first, r.second);
}

inline optional<session_state_ref> shared_target::get_target(buffer const& share_name, buffer const& topic_filter) {
    std::lock_guard<mutex> g{mtx_targets_};
    // get share_name matched range ordered by timestamp (ascending)
    auto& idx = targets_.get<tag_sn_tp>();
    auto r = idx.equal_range(share_name);
    for (; r.first != r.second; ++r.first) {
        auto const& elem = *r.first;
        auto it = elem.topic_filters.find(topic_filter);

        // no share_name/topic_filter matched
        if (it == elem.topic_filters.end()) continue;

        // matched
        // update timestamp (timestamp is key)
        idx.modify(r.first, [](auto& e) { e.tp = std::chrono::steady_clock::now(); });
        return elem.ssr;
    }
    return nullopt;
}

inline shared_target::entry::entry(
    buffer share_name,
    session_state& ss,
    time_point_t tp)
    : share_name { force_move(share_name) },
      ssr { ss },
      tp { force_move(tp) }
{}

inline buffer const& shared_target::entry::client_id() const {
    return ssr.get().client_id();
}

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SHARED_TARGET_IMPL_HPP

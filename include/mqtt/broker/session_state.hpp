// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SESSION_STATE_HPP)
#define MQTT_BROKER_SESSION_STATE_HPP

#include <mqtt/config.hpp>

#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <mqtt/broker/broker_namespace.hpp>

#include <mqtt/broker/common_type.hpp>
#include <mqtt/broker/sub_con_map.hpp>
#include <mqtt/broker/shared_target.hpp>
#include <mqtt/broker/property_util.hpp>
#include <mqtt/broker/tags.hpp>
#include <mqtt/broker/inflight_message.hpp>
#include <mqtt/broker/offline_message.hpp>

MQTT_BROKER_NS_BEGIN

namespace as = boost::asio;
namespace mi = boost::multi_index;

class session_states;

/**
 * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Session_State
 *
 * 4.1 Session State
 * In order to implement QoS 1 and QoS 2 protocol flows the Client and Server need to associate state with the Client Identifier, this is referred to as the Session State. The Server also stores the subscriptions as part of the Session State.
 * The session can continue across a sequence of Network Connections. It lasts as long as the latest Network Connection plus the Session Expiry Interval.
 * The Session State in the Server consists of:
 * · The existence of a Session, even if the rest of the Session State is empty.
 * · The Clients subscriptions, including any Subscription Identifiers.
 * · QoS 1 and QoS 2 messages which have been sent to the Client, but have not been completely acknowledged.
 * · QoS 1 and QoS 2 messages pending transmission to the Client and OPTIONALLY QoS 0 messages pending transmission to the Client.
 * · QoS 2 messages which have been received from the Client, but have not been completely acknowledged.
 * · The Will Message and the Will Delay Interval
 * · If the Session is currently not connected, the time at which the Session will end and Session State will be discarded.
 *
 * Retained messages do not form part of the Session State in the Server, they are not deleted as a result of a Session ending.
 */
struct session_state {
    // TODO: Currently not fully implemented...
    session_state(
        as::io_context& ioc,
        sub_con_map& subs_map,
        shared_target& shared_targets,
        con_sp_t con,
        buffer client_id,
        optional<will> will,
        optional<std::chrono::steady_clock::duration> will_expiry_interval,
        optional<std::chrono::steady_clock::duration> session_expiry_interval = nullopt)
        :ioc_(ioc),
         subs_map_(subs_map),
         shared_targets_(shared_targets),
         con_(force_move(con)),
         client_id_(force_move(client_id)),
         session_expiry_interval_(force_move(session_expiry_interval))
    {
        update_will(ioc, will, will_expiry_interval);
    }

    session_state(session_state&&) = default;

    ~session_state() {
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "session destroy";
        clean();
    }

    bool online() const {
        return bool(con_);
    }

    template <typename SessionExpireHandler>
    void become_offline(SessionExpireHandler&& h) {
        BOOST_ASSERT(con_);
        con_->for_each_store_with_life_keeper(
            [this] (store_message_variant msg, any life_keeper) {
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "store inflight message";

                std::shared_ptr<as::steady_timer> tim_message_expiry;

                MQTT_NS::visit(
                    make_lambda_visitor(
                        [&](v5::basic_publish_message<sizeof(packet_id_t)> const& m) {
                            auto v = get_property<v5::property::message_expiry_interval>(m.props());
                            if (v) {
                                tim_message_expiry =
                                    std::make_shared<as::steady_timer>(ioc_, std::chrono::seconds(v.value().val()));
                                tim_message_expiry->async_wait(
                                    [this, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)]
                                    (error_code ec) {
                                        if (auto sp = wp.lock()) {
                                            if (!ec) {
                                                erase_inflight_message_by_expiry(sp);
                                            }
                                        }
                                    }
                                );
                            }
                        },
                        [&](auto const&) {}
                    ),
                    msg
                );

                insert_inflight_message(
                    force_move(msg),
                    force_move(life_keeper),
                    force_move(tim_message_expiry)
                );
            }
        );

        reset_con();

        if (session_expiry_interval_ &&
            session_expiry_interval_.value() != std::chrono::seconds(session_never_expire)) {

            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "session expiry interval timer set";

            tim_session_expiry_ = std::make_shared<as::steady_timer>(ioc_, session_expiry_interval_.value());
            tim_session_expiry_->async_wait(
                [this, wp = std::weak_ptr<as::steady_timer>(tim_session_expiry_), h = std::forward<SessionExpireHandler>(h)]
                (error_code ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            MQTT_LOG("mqtt_broker", info)
                                << MQTT_ADD_VALUE(address, this)
                                << "session expired";
                            h(sp);
                        }
                    }
                }
            );
        }
    }

    void renew_session_expiry(optional<std::chrono::steady_clock::duration> v) {
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "renew_session expiry";
        session_expiry_interval_ = force_move(v);
        tim_session_expiry_.reset();
    }

    std::shared_ptr<as::steady_timer> const& tim_session_expiry() const {
        return tim_session_expiry_;
    }

    void publish(
        as::io_context& ioc,
        buffer pub_topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {

        BOOST_ASSERT(online());

        if (offline_messages_.empty()) {
            auto qos_value = pubopts.get_qos();
            if (qos_value == qos::at_least_once ||
                qos_value == qos::exactly_once) {
                if (auto pid = con_->acquire_unique_packet_id_no_except()) {
                    // TODO: Probably this should be switched to async_publish?
                    //       Given the async_client / sync_client seperation
                    //       and the way they have different function names,
                    //       it wouldn't be possible for broker.hpp to be
                    //       used with some hypothetical "async_server" in the future.
                    con_->publish(pid.value(), pub_topic, contents, pubopts, props);
                    return;
                }
            }
            else {
                con_->publish(pub_topic, contents, pubopts, props);
                return;
            }
        }

        // offline_messages_ is not empty or packet_id_exhausted
        offline_messages_.push_back(
            ioc,
            force_move(pub_topic),
            force_move(contents),
            pubopts,
            force_move(props)
        );
    }

    void deliver(
        as::io_context& ioc,
        buffer pub_topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {

        if (online()) {
            publish(
                ioc,
                force_move(pub_topic),
                force_move(contents),
                pubopts,
                force_move(props)
            );
        }
        else {
            offline_messages_.push_back(
                ioc,
                force_move(pub_topic),
                force_move(contents),
                pubopts,
                force_move(props)
            );
        }
    }

    void clean() {
        inflight_messages_.clear();
        offline_messages_.clear();
        qos2_publish_processed_.clear();
        shared_targets_.erase(*this);
        unsubscribe_all();
    }

    void exactly_once_start(packet_id_t packet_id) {
        qos2_publish_processed_.insert(packet_id);
    }

    bool exactly_once_processing(packet_id_t packet_id) const {
        return qos2_publish_processed_.find(packet_id) != qos2_publish_processed_.end();
    }

    void exactly_once_finish(packet_id_t packet_id) {
        qos2_publish_processed_.erase(packet_id);
    }

    template <typename PublishRetainHandler>
    void subscribe(
        buffer share_name,
        buffer topic_filter,
        subscribe_options subopts,
        PublishRetainHandler&& h,
        optional<std::size_t> sid = nullopt
    ) {
        if (!share_name.empty()) {
            shared_targets_.insert(share_name, topic_filter, *this);
        }
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "subscribe"
            << " share_name:" << share_name
            << " topic_filter:" << topic_filter
            << " qos:" << subopts.get_qos();

        subscription sub {*this, force_move(share_name), topic_filter, subopts, sid };
        auto handle_ret = subs_map_.insert_or_assign(
            force_move(topic_filter),
            client_id_,
            force_move(sub)
        );

        auto rh = subopts.get_retain_handling();

        if (handle_ret.second) { // insert
            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "subscription inserted";

            handles_.insert(handle_ret.first);
            if (rh == retain_handling::send ||
                rh == retain_handling::send_only_new_subscription) {
                std::forward<PublishRetainHandler>(h)();
            }
        }
        else { // update
            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "subscription updated";

            if (rh == retain_handling::send) {
                std::forward<PublishRetainHandler>(h)();
            }
        }
    }

    void unsubscribe(buffer const& share_name, buffer const& topic_filter) {
        if (!share_name.empty()) {
            shared_targets_.erase(share_name, topic_filter, *this);
        }
        auto handle = subs_map_.lookup(topic_filter);
        if (handle) {
            handles_.erase(handle.value());
            subs_map_.erase(handle.value(), client_id_);
        }
    }

    void unsubscribe_all() {
        for (auto const& h : handles_) {
            subs_map_.erase(h, client_id_);
        }
        handles_.clear();
    }

    void update_will(
        as::io_context& ioc,
        optional<MQTT_NS::will> will,
        optional<std::chrono::steady_clock::duration> will_expiry_interval) {
        tim_will_expiry_.reset();
        will_value_ = force_move(will);

        if (will_value_ && will_expiry_interval) {
            tim_will_expiry_ = std::make_shared<as::steady_timer>(ioc, will_expiry_interval.value());
            tim_will_expiry_->async_wait(
                [this, wp = std::weak_ptr<as::steady_timer>(tim_will_expiry_)]
                (error_code ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            reset_will();
                        }
                    }
                }
            );
        }
    }

    void reset_will() {
        tim_will_expiry_.reset();
        will_value_ = nullopt;
    }

    void insert_inflight_message(
        store_message_variant msg,
        any life_keeper,
        std::shared_ptr<as::steady_timer> tim_message_expiry
    ) {
        inflight_messages_.insert(
            force_move(msg),
            force_move(life_keeper),
            force_move(tim_message_expiry)
        );
    }

    void send_inflight_messages() {
        BOOST_ASSERT(con_);
        inflight_messages_.send_all_messages(*con_);
    }

    void erase_inflight_message_by_expiry(std::shared_ptr<as::steady_timer> const& sp) {
        inflight_messages_.get<tag_tim>().erase(sp);
    }

    void erase_inflight_message_by_packet_id(packet_id_t packet_id) {
        auto& idx = inflight_messages_.get<tag_pid>();
        idx.erase(packet_id);
    }

    void send_all_offline_messages() {
        BOOST_ASSERT(con_);
        offline_messages_.send_all(*con_);
    }

    void send_offline_messages_by_packet_id_release() {
        BOOST_ASSERT(con_);
        offline_messages_.send_by_packet_id_release(*con_);
    }

    buffer const& client_id() const {
        return client_id_;
    }

    void reset_con() {
        con_.reset();
    }

    void reset_con(con_sp_t con) {
        con_ = force_move(con);
    }

    con_sp_t const& con() const {
        return con_;
    }

    optional<std::chrono::steady_clock::duration> session_expiry_interval() const {
        return session_expiry_interval_;
    }

    optional<MQTT_NS::will>& will() { return will_value_; }
    optional<MQTT_NS::will> const& will() const { return will_value_; }

    std::shared_ptr<as::steady_timer>& get_tim_will_expiry() { return tim_will_expiry_; }

private:
    friend class session_states;

    as::io_context& ioc_;
    std::shared_ptr<as::steady_timer> tim_will_expiry_;
    optional<MQTT_NS::will> will_value_;

    sub_con_map& subs_map_;
    shared_target& shared_targets_;
    con_sp_t con_;
    buffer client_id_;

    optional<std::chrono::steady_clock::duration> will_delay_;
    optional<std::chrono::steady_clock::duration> session_expiry_interval_;
    std::shared_ptr<as::steady_timer> tim_session_expiry_;

    inflight_messages inflight_messages_;
    std::set<packet_id_t> qos2_publish_processed_;

    offline_messages offline_messages_;

    std::set<sub_con_map::handle> handles_; // to efficient remove
};

class session_states {
public:
    template <typename Tag>
    decltype(auto) get() {
        return entries_.get<Tag>();
    }

    template <typename Tag>
    decltype(auto) get() const {
        return entries_.get<Tag>();
    }

    void clear() {
        entries_.clear();
    }

private:
    // The mi_session_online container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_session_state = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            // non is nullable
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(session_state, con_sp_t, con_)
            >,
            mi::ordered_unique<
                mi::tag<tag_cid>,
                BOOST_MULTI_INDEX_MEMBER(session_state, buffer, client_id_)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(session_state, std::shared_ptr<as::steady_timer>, tim_session_expiry_)
            >
        >
>;

    mi_session_state entries_;
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SESSION_STATE_HPP

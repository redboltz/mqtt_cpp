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
#include <mqtt/broker/mutex.hpp>

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
    using will_sender_t = std::function<
        void(
            session_state const& source_ss,
            buffer topic,
            buffer contents,
            publish_options pubopts,
            v5::properties props
        )
    >;

    session_state(
        as::io_context& timer_ioc,
        mutex& mtx_subs_map,
        sub_con_map& subs_map,
        shared_target& shared_targets,
        con_sp_t con,
        buffer client_id,
        std::string const& username,
        optional<will> will,
        will_sender_t will_sender,
        optional<std::chrono::steady_clock::duration> will_expiry_interval,
        optional<std::chrono::steady_clock::duration> session_expiry_interval)
        :timer_ioc_(timer_ioc),
         mtx_subs_map_(mtx_subs_map),
         subs_map_(subs_map),
         shared_targets_(shared_targets),
         con_(force_move(con)),
         version_(con_->get_protocol_version()),
         client_id_(force_move(client_id)),
         username_(username),
         session_expiry_interval_(force_move(session_expiry_interval)),
         tim_will_delay_(timer_ioc_),
         will_sender_(force_move(will_sender)),
         remain_after_close_(
            [&] {
                if (version_ == protocol_version::v3_1_1) {
                    return !con_->clean_session();
                }
                else {
                    BOOST_ASSERT(version_ == protocol_version::v5);
                    return
                        session_expiry_interval_ &&
                        session_expiry_interval_.value() != std::chrono::steady_clock::duration::zero();
                }
            } ()
         )
    {
        update_will(timer_ioc, will, will_expiry_interval);
    }

    ~session_state() {
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "session destroy";
        send_will_impl();
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
                                    std::make_shared<as::steady_timer>(timer_ioc_, std::chrono::seconds(v.value().val()));
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
        qos2_publish_handled_ = con_->get_qos2_publish_handled_pids();
        con_.reset();

        if (session_expiry_interval_ &&
            session_expiry_interval_.value() != std::chrono::seconds(session_never_expire)) {

            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "session expiry interval timer set";

            std::lock_guard<mutex> g(mtx_tim_session_expiry_);
            tim_session_expiry_ = std::make_shared<as::steady_timer>(timer_ioc_, session_expiry_interval_.value());
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
        std::lock_guard<mutex> g(mtx_tim_session_expiry_);
        tim_session_expiry_.reset();
    }

    void publish(
        as::io_context& timer_ioc,
        buffer pub_topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {

        BOOST_ASSERT(online());

        std::lock_guard<mutex> g(mtx_offline_messages_);
        if (offline_messages_.empty()) {
            auto qos_value = pubopts.get_qos();
            if (qos_value == qos::at_least_once ||
                qos_value == qos::exactly_once) {
                if (auto pid = con_->acquire_unique_packet_id_no_except()) {
                    con_->async_publish(
                        pid.value(),
                        force_move(pub_topic),
                        force_move(contents),
                        pubopts,
                        force_move(props),
                        any{},
                        [con = con_]
                        (error_code ec) {
                            if (ec) {
                                MQTT_LOG("mqtt_broker", warning)
                                    << MQTT_ADD_VALUE(address, con.get())
                                    << ec.message();
                            }
                        }
                    );
                    return;
                }
            }
            else {
                con_->async_publish(
                    force_move(pub_topic),
                    force_move(contents),
                    pubopts,
                    force_move(props),
                    any{},
                    [con = con_]
                    (error_code ec) {
                        if (ec) {
                            MQTT_LOG("mqtt_broker", warning)
                                << MQTT_ADD_VALUE(address, con.get())
                                << ec.message();
                        }
                    }
                );
                return;
            }
        }

        // offline_messages_ is not empty or packet_id_exhausted
        offline_messages_.push_back(
            timer_ioc,
            force_move(pub_topic),
            force_move(contents),
            pubopts,
            force_move(props)
        );
    }

    void deliver(
        as::io_context& timer_ioc,
        buffer pub_topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {

        if (online()) {
            publish(
                timer_ioc,
                force_move(pub_topic),
                force_move(contents),
                pubopts,
                force_move(props)
            );
        }
        else {
            std::lock_guard<mutex> g(mtx_offline_messages_);
            offline_messages_.push_back(
                timer_ioc,
                force_move(pub_topic),
                force_move(contents),
                pubopts,
                force_move(props)
            );
        }
    }

    void set_clean_handler(std::function<void()> handler) {
        clean_handler_ = force_move(handler);
    }

    void clean() {
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "clean";
        if (clean_handler_) clean_handler_();
        {
            std::lock_guard<mutex> g(mtx_inflight_messages_);
            inflight_messages_.clear();
        }
        {
            std::lock_guard<mutex> g(mtx_offline_messages_);
            offline_messages_.clear();
        }
        shared_targets_.erase(*this);
        unsubscribe_all();
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
        auto handle_ret =
            [&] {
                std::lock_guard<mutex> g{mtx_subs_map_};
                return subs_map_.insert_or_assign(
                    force_move(topic_filter),
                    client_id_,
                    force_move(sub)
                );
            } ();

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
        std::lock_guard<mutex> g{mtx_subs_map_};
        auto handle = subs_map_.lookup(topic_filter);
        if (handle) {
            handles_.erase(handle.value());
            subs_map_.erase(handle.value(), client_id_);
        }
    }

    void unsubscribe_all() {
        {
            std::lock_guard<mutex> g{mtx_subs_map_};
            for (auto const& h : handles_) {
                subs_map_.erase(h, client_id_);
            }
        }
        handles_.clear();
    }

    void update_will(
        as::io_context& timer_ioc,
        optional<MQTT_NS::will> will,
        optional<std::chrono::steady_clock::duration> will_expiry_interval) {
        std::lock_guard<mutex> g(mtx_tim_will_expiry_);
        tim_will_expiry_.reset();
        will_value_ = force_move(will);

        if (will_value_ && will_expiry_interval) {
            tim_will_expiry_ = std::make_shared<as::steady_timer>(timer_ioc, will_expiry_interval.value());
            tim_will_expiry_->async_wait(
                [this, wp = std::weak_ptr<as::steady_timer>(tim_will_expiry_)]
                (error_code ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            clear_will();
                        }
                    }
                }
            );
        }
    }

    void clear_will() {
        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "clear will. cid:" << client_id_;
        std::lock_guard<mutex> g(mtx_tim_will_expiry_);
        tim_will_expiry_.reset();
        will_value_ = nullopt;
    }

    void send_will() {
        if (!will_value_) return;

        auto wd_sec =
            [&] () -> std::size_t {
                if (auto wd_opt = get_property<v5::property::will_delay_interval>(
                        will_value_.value().props()
                    )
                ) {
                    return wd_opt.value().val();
                }
                return 0;
            } ();

        if (remain_after_close_ && wd_sec != 0) {
            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "set will_delay. cid:" << client_id_ << " delay:" << wd_sec;
            tim_will_delay_.expires_after(std::chrono::seconds(wd_sec));
            tim_will_delay_.async_wait(
                [this]
                (error_code ec) {
                    if (!ec) {
                        send_will_impl();
                    }
                }
            );
        }
        else {
            send_will_impl();
        }
    }

    void insert_inflight_message(
        store_message_variant msg,
        any life_keeper,
        std::shared_ptr<as::steady_timer> tim_message_expiry
    ) {
        std::lock_guard<mutex> g(mtx_inflight_messages_);
        inflight_messages_.insert(
            force_move(msg),
            force_move(life_keeper),
            force_move(tim_message_expiry)
        );
    }

    void send_inflight_messages() {
        BOOST_ASSERT(con_);
        std::lock_guard<mutex> g(mtx_inflight_messages_);
        inflight_messages_.send_all_messages(*con_);
    }

    void erase_inflight_message_by_expiry(std::shared_ptr<as::steady_timer> const& sp) {
        std::lock_guard<mutex> g(mtx_inflight_messages_);
        inflight_messages_.get<tag_tim>().erase(sp);
    }

    void erase_inflight_message_by_packet_id(packet_id_t packet_id) {
        std::lock_guard<mutex> g(mtx_inflight_messages_);
        auto& idx = inflight_messages_.get<tag_pid>();
        idx.erase(packet_id);
    }

    void send_all_offline_messages() {
        BOOST_ASSERT(con_);
        std::lock_guard<mutex> g(mtx_offline_messages_);
        offline_messages_.send_until_fail(*con_);
    }

    void send_offline_messages_by_packet_id_release() {
        BOOST_ASSERT(con_);
        std::lock_guard<mutex> g(mtx_offline_messages_);
        offline_messages_.send_until_fail(*con_);
    }

    protocol_version get_protocol_version() const {
        return version_;
    }

    buffer const& client_id() const {
        return client_id_;
    }

    void set_username(std::string const& username) {
        username_ = username;
    }
    std::string const& get_username() const {
        return username_;
    }

    void renew(con_sp_t con, bool clean_start) {
        tim_will_delay_.cancel();
        if (clean_start) {
            // send previous will
            send_will_impl();
            qos2_publish_handled_.clear();
        }
        else {
            // cancel will
            clear_will();
            con->restore_qos2_publish_handled_pids(qos2_publish_handled_);
        }
        con_ = force_move(con);
    }

    con_sp_t const& con() const {
        return con_;
    }

    optional<std::chrono::steady_clock::duration> session_expiry_interval() const {
        return session_expiry_interval_;
    }

    void set_response_topic(std::string topic) {
        response_topic_.emplace(force_move(topic));
    }

    optional<std::string> get_response_topic() const {
        return response_topic_;
    }

private:
    void send_will_impl() {
        if (!will_value_) return;

        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "send will. cid:" << client_id_;

        auto topic = force_move(will_value_.value().topic());
        auto payload = force_move(will_value_.value().message());
        auto opts = will_value_.value().get_qos() | will_value_.value().get_retain();
        auto props = force_move(will_value_.value().props());
        will_value_ = nullopt;
        {
            std::shared_lock<mutex> g(mtx_tim_will_expiry_);
            if (tim_will_expiry_) {
                auto d =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        tim_will_expiry_->expiry() - std::chrono::steady_clock::now()
                    ).count();
                if (d < 0) d = 0;
                set_property<v5::property::message_expiry_interval>(
                    props,
                    v5::property::message_expiry_interval(
                        static_cast<uint32_t>(d)
                    )
                );
            }
        }
        if (will_sender_) {
            will_sender_(
                *this,
                force_move(topic),
                force_move(payload),
                opts,
                force_move(props)
            );
        }
    }

private:
    friend class session_states;

    as::io_context& timer_ioc_;
    mutex mtx_tim_will_expiry_;
    std::shared_ptr<as::steady_timer> tim_will_expiry_;
    optional<MQTT_NS::will> will_value_;

    mutex& mtx_subs_map_;
    sub_con_map& subs_map_;
    shared_target& shared_targets_;
    con_sp_t con_;
    protocol_version version_;
    buffer client_id_;

    std::string username_;

    optional<std::chrono::steady_clock::duration> session_expiry_interval_;
    mutex mtx_tim_session_expiry_;
    std::shared_ptr<as::steady_timer> tim_session_expiry_;

    mutable mutex mtx_inflight_messages_;
    inflight_messages inflight_messages_;

    mutable mutex mtx_offline_messages_;
    offline_messages offline_messages_;

    std::set<sub_con_map::handle> handles_; // to efficient remove

    as::steady_timer tim_will_delay_;
    will_sender_t will_sender_;
    bool remain_after_close_;

    std::set<packet_id_t> qos2_publish_handled_;

    optional<std::string> response_topic_;
    std::function<void()> clean_handler_;
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
                mi::composite_key<
                    session_state,
                    BOOST_MULTI_INDEX_MEMBER(session_state, std::string, username_),
                    BOOST_MULTI_INDEX_MEMBER(session_state, buffer, client_id_)
                >
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

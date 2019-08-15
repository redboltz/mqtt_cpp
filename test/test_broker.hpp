// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_BROKER_HPP)
#define MQTT_TEST_BROKER_HPP

#include <iostream>
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt_server_cpp.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/visitor_util.hpp>

#include "test_settings.hpp"


namespace mi = boost::multi_index;
namespace as = boost::asio;

/**
 * Allow the broker to support incoming connections of
 * any supported type of mqtt::server.
 */
using con_sp_t = std::shared_ptr<mqtt::server<>::endpoint_t>;

class test_broker {
public:
    test_broker(as::io_service& ios)
        :ios_(ios),
         tim_disconnect_(ios_)
    {}

    // [begin] for test setting
    /**
     * @brief set_disconnect_delay adds a delay to disconnect operations.
     *
     * This makes the broker wait the specified amount between when a disconnect
     * is received from a client, and when the connection is actually closed in
     * the broker.
     *
     * @param delay - the amount to delay by
     */
    void set_disconnect_delay(boost::posix_time::time_duration const& delay) {
        delay_disconnect_ = delay;
    }
    // [end] for test setting

    /**
     * @brief handle_accept
     *
     * Call this function when an mqtt::server (of whatever kind) has accepted a raw
     * connection from an MQTT client. By 'raw connection', this might be raw TCP sockets
     * or websockets, or completed a TLS handshake, or any other underlying transport
     * type, but what is not meant is that the mqtt client on the other end of the endpoint
     * has initiated the MQTT application protocol connection sequence with CONNECT or CONACK
     * messages being sent or received.
     *
     * This function will assign several event handlers into mqtt::server (of whatever kind)
     * that is provided as a parameter. This includes connection handlers, disconnection handlers
     * and various handlers for a variety of of MQTT message types.
     *
     * @param ep - The mqtt::server (of whichever kind) to accept a connection on.
     */
    template <typename Endpoint>
    void handle_accept(Endpoint& ep) {
        auto sp = ep.shared_from_this();
        ep.socket().lowest_layer().set_option(as::ip::tcp::no_delay(true));
        ep.set_auto_pub_response(false);
        ep.start_session(
            [sp] // keeping ep's lifetime as sp until session finished
            (boost::system::error_code const& /*ec*/) {
            }
        );

        // set connection (lower than MQTT) level handlers
        ep.set_close_handler(
            [&]
            (){
                close_proc(ep, true);
            });
        ep.set_error_handler(
            [&]
            (boost::system::error_code const& /*ec*/){
                close_proc(ep, true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [&]
            (mqtt::buffer client_id,
             mqtt::optional<mqtt::buffer> username,
             mqtt::optional<mqtt::buffer> password,
             mqtt::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                return
                    connect_handler(
                        ep,
                        client_id,
                        username,
                        password,
                        std::move(will),
                        clean_session,
                        keep_alive,
                        {}
                    );
            }
        );
        ep.set_v5_connect_handler(
            [&]
            (mqtt::buffer client_id,
             mqtt::optional<mqtt::buffer> username,
             mqtt::optional<mqtt::buffer> password,
             mqtt::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t keep_alive,
             std::vector<mqtt::v5::property_variant> props) {
                return
                    connect_handler(
                        ep,
                        client_id,
                        username,
                        password,
                        std::move(will),
                        clean_session,
                        keep_alive,
                        std::move(props)
                    );
            }
        );
        ep.set_disconnect_handler(
            [&]
            (){
                return
                    disconnect_handler(ep);
            }
        );
        ep.set_v5_disconnect_handler(
            [&]
            (std::uint8_t /*reason_code*/, std::vector<mqtt::v5::property_variant> props) {
                if (h_disconnect_props_) h_disconnect_props_(std::move(props));
                return
                    disconnect_handler(ep);
            }
        );
        ep.set_puback_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_puback_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/,
             std::uint8_t /*reason_code*/,
             std::vector<mqtt::v5::property_variant> /*props*/){
                return true;
            });
        ep.set_pubrec_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id){
                ep.pubrel(packet_id);
                return true;
            });
        ep.set_v5_pubrec_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::uint8_t /*reason_code*/,
             std::vector<mqtt::v5::property_variant> /*props*/){
                ep.pubrel(packet_id, mqtt::v5::reason_code::success, pubrel_props_);
                return true;
            });
        ep.set_pubrel_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id){
                ep.pubcomp(packet_id);
                return true;
            });
        ep.set_v5_pubrel_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::uint8_t /*reason_code*/,
             std::vector<mqtt::v5::property_variant> /*props*/){
                ep.pubcomp(packet_id, mqtt::v5::reason_code::success, pubcomp_props_);
                return true;
            });
        ep.set_pubcomp_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_pubcomp_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/,
             std::uint8_t /*reason_code*/,
             std::vector<mqtt::v5::property_variant> /*props*/){
                return true;
            });
        ep.set_publish_handler(
            [&]
            (std::uint8_t header,
             mqtt::optional<typename Endpoint::packet_id_t> packet_id,
             mqtt::buffer topic_name,
             mqtt::buffer contents){
                return publish_handler(
                    ep,
                    header,
                    packet_id,
                    topic_name,
                    contents,
                    {}
                );
            });
        ep.set_v5_publish_handler(
            [&]
            (std::uint8_t header,
             mqtt::optional<typename Endpoint::packet_id_t> packet_id,
             mqtt::buffer topic_name,
             mqtt::buffer contents,
             std::vector<mqtt::v5::property_variant> props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                return publish_handler(
                    ep,
                    header,
                    packet_id,
                    topic_name,
                    contents,
                    std::move(props)
                );
            });
        ep.set_subscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::tuple<mqtt::buffer, std::uint8_t>> entries) {
                return subscribe_handler(
                    ep,
                    packet_id,
                    std::move(entries),
                    {}
                );
            }
        );
        ep.set_v5_subscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::tuple<mqtt::buffer, std::uint8_t>> entries,
             std::vector<mqtt::v5::property_variant> props
            ) {
                return subscribe_handler(
                    ep,
                    packet_id,
                    std::move(entries),
                    std::move(props)
                );
            }
        );
        ep.set_unsubscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<mqtt::buffer> topics) {
                return unsubscribe_handler(
                    ep,
                    packet_id,
                    std::move(topics),
                    {}
                );
            }
        );
        ep.set_v5_unsubscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<mqtt::buffer> topics,
             std::vector<mqtt::v5::property_variant> props
            ) {
                return unsubscribe_handler(
                    ep,
                    packet_id,
                    std::move(topics),
                    std::move(props)
                );
            }
        );
        ep.set_pingreq_handler(
            [&] {
                ep.pingresp();
                return true;
            }
        );
        ep.set_v5_auth_handler(
            [&]
            (std::uint8_t /*reason_code*/,
             std::vector<mqtt::v5::property_variant> props
            ) {
                if (h_auth_props_) h_auth_props_(std::move(props));
                return true;
            }
        );
    }

    void set_connack_props(std::vector<mqtt::v5::property_variant> props) {
        connack_props_ = std::move(props);
    }

    void set_suback_props(std::vector<mqtt::v5::property_variant> props) {
        suback_props_ = std::move(props);
    }

    void set_unsuback_props(std::vector<mqtt::v5::property_variant> props) {
        unsuback_props_ = std::move(props);
    }

    void set_puback_props(std::vector<mqtt::v5::property_variant> props) {
        puback_props_ = std::move(props);
    }

    void set_pubrec_props(std::vector<mqtt::v5::property_variant> props) {
        pubrec_props_ = std::move(props);
    }

    void set_pubrel_props(std::vector<mqtt::v5::property_variant> props) {
        pubrel_props_ = std::move(props);
    }

    void set_pubcomp_props(std::vector<mqtt::v5::property_variant> props) {
        pubcomp_props_ = std::move(props);
    }

    void set_connect_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_connect_props_ = std::move(h);
    }

    void set_disconnect_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_disconnect_props_ = std::move(h);
    }

    void set_publish_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_publish_props_ = std::move(h);
    }

    void set_puback_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_puback_props_ = std::move(h);
    }

    void set_pubrec_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_pubrec_props_ = std::move(h);
    }

    void set_pubrel_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_pubrel_props_ = std::move(h);
    }

    void set_pubcomp_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_pubcomp_props_ = std::move(h);
    }

    void set_subscribe_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_subscribe_props_ = std::move(h);
    }

    void set_unsubscribe_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_unsubscribe_props_ = std::move(h);
    }

    void set_auth_props_handler(std::function<void(std::vector<mqtt::v5::property_variant> const&)> h) {
        h_auth_props_ = std::move(h);
    }

private:
    /**
     * @brief connect_proc Process an incoming CONNECT packet
     *
     * This is called by the connect_handler function, which is registered
     * on mqtt connections where the raw transport (tcp / tls / websocket / etc)
     * is established, but the CONNECT message has not been sent / received by
     * the mqtt client on the other end of the connection.
     *
     * When the CONNECT message is received, this function is called after some
     * basic pre-connection logic, to setup the record keeping that this broker
     * class needs to handle the connection and process subscriptions and publishing.
     *
     * @param clean_session - if the clean-session flag is set on the CONNECT message.
     * @param spep - mqtt::varient of shared pointers to underlying connection type.
     * @param client_id - the id that the client wants to use
     * @param will - the last-will-and-testiment of the connection, if any.
     */
    template <typename Endpoint>
    bool connect_handler(
        Endpoint& ep,
        mqtt::buffer client_id,
        mqtt::optional<mqtt::buffer> const& /*username*/,
        mqtt::optional<mqtt::buffer> const& /*password*/,
        mqtt::optional<mqtt::will> will,
        bool clean_session,
        std::uint16_t /*keep_alive*/,
        std::vector<mqtt::v5::property_variant> props
    ) {
        if (ep.get_protocol_version() == mqtt::protocol_version::v5 && h_connect_props_) h_connect_props_(props);

        // If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
        // If it's a not a clean session, but no client id is provided, we would have no way to map this
        // connection's session to a new connection later. So the connection must be rejected.
        if (client_id.empty() && !clean_session) {
            ep.connack(false, mqtt::connect_return_code::identifier_rejected);
            return false;
        }

        auto spep = ep.shared_from_this();

        // Find any sessions that have the same client_id
        auto & act_sess_idx = active_sessions_.get<tag_client_id>();
        auto act_sess_it = act_sess_idx.find(client_id);

        auto & non_act_sess_idx = non_active_sessions_.get<tag_client_id>();
        auto non_act_sess_it = non_act_sess_idx.find(client_id);

        // It's a hard error for a client_id to have a session object
        // in both the active and non active session containers.
        BOOST_ASSERT((act_sess_idx.end() == act_sess_it) || (non_act_sess_idx.end() == non_act_sess_it));

        // Reply to the connect message.
        switch (ep.get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                mqtt::connect_return_code::accepted
            );
            break;
        case mqtt::protocol_version::v5:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                mqtt::v5::reason_code::success,
                connack_props_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        /**
         * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Toc514345311
         * 3.1.2.4 Clean Start
         * If a CONNECT packet is received with Clean Start is set to 1, the Client and Server MUST
         * discard any existing Session and start a new Session [MQTT-3.1.2-4]. Consequently,
         *  the Session Present flag in CONNACK is always set to 0 if Clean Start is set to 1.
         */
        if(clean_session && (non_act_sess_it != non_act_sess_idx.end())) {
            non_act_sess_idx.erase(non_act_sess_it);
            BOOST_ASSERT(non_act_sess_idx.end() == non_act_sess_idx.find(client_id));

            non_act_sess_it = non_act_sess_idx.end();
        }

        if(act_sess_it == act_sess_idx.end()) {
            // If we have a saved session, we can transfer the state from it
            // to the active_session container.
            if(non_act_sess_it != non_act_sess_idx.end()) {
                session_state state;
                non_act_sess_idx.modify(non_act_sess_it, [&](session_state & val) { state = std::move(val); });
                state.con = spep;
                non_act_sess_idx.erase(non_act_sess_it);
                BOOST_ASSERT(non_act_sess_idx.end() == non_act_sess_idx.find(client_id));

                auto const& ret = active_sessions_.insert(std::move(state));
                act_sess_it = ret.first;
                BOOST_ASSERT(ret.second);
                BOOST_ASSERT(*(act_sess_it->client_id) == client_id);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
                BOOST_ASSERT(active_sessions_.project<tag_con>(act_sess_it) == active_sessions_.get<tag_con>().find(spep));
            }
            else {
                std::shared_ptr<std::string> sp_clientId = std::make_shared<std::string>(client_id);
                auto const& ret = active_sessions_.emplace(sp_clientId, spep, std::move(will));
                act_sess_it = ret.first;
                BOOST_ASSERT(ret.second);
                BOOST_ASSERT(act_sess_it->client_id == sp_clientId);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
            }
        }
        else {
            // Disconnect the existing connection

            // Completely cut this endpoint off from the broker!
            act_sess_it->con->set_close_handler();
            act_sess_it->con->set_error_handler();
            act_sess_it->con->set_v5_connect_handler();
            act_sess_it->con->set_v5_disconnect_handler();
            act_sess_it->con->set_v5_puback_handler();
            act_sess_it->con->set_v5_pubrec_handler();
            act_sess_it->con->set_v5_pubrel_handler();
            act_sess_it->con->set_v5_pubcomp_handler();
            act_sess_it->con->set_v5_publish_handler();
            act_sess_it->con->set_v5_subscribe_handler();
            act_sess_it->con->set_v5_unsubscribe_handler();
            act_sess_it->con->set_pingreq_handler();
            act_sess_it->con->set_pingresp_handler();
            act_sess_it->con->set_v5_auth_handler();

            // Force disconnect the client.
            // This shuts down the socket directly.
            act_sess_it->con->force_disconnect();

            // Replace it with the new connection
            // Nothing more should need to be done
            // ...
            // But potentially we need to re-send
            // messages marked as retain?
            // TODO: Verify from standard.
            {
                auto & subs_idx = subs_.get<tag_con>();
                {
                    auto const& range = boost::make_iterator_range(subs_idx.equal_range(act_sess_it->con));
                    for(auto it = range.begin(); it != range.end(); std::advance(it, 1)) {
                        subs_idx.modify_key(it,
                                            [&](con_sp_t & val) { val = spep; },
                                            [&](con_sp_t&) { BOOST_ASSERT(false); });
                    }
                }
                BOOST_ASSERT(subs_idx.count(act_sess_it->con) == 0);
            }
            active_sessions_.get<tag_con>().modify_key(active_sessions_.project<tag_con>(act_sess_it),
                                                       [&](con_sp_t & val) { val = spep; },
                                                       [&](con_sp_t&) { BOOST_ASSERT(false); });
        }

        if (clean_session) {
            saved_subs_.get<tag_client_id>().erase(client_id);
            BOOST_ASSERT(saved_subs_.get<tag_client_id>().count(client_id) == 0);
        }
        else {
            // If it's not a clean session, then all of the
            // saved subscriptions are moved to the new session
            // object so they can be used immediately.
            auto & idx = saved_subs_.get<tag_client_id>();
            auto const& range = boost::make_iterator_range(idx.equal_range(client_id));
            for(auto const& item : range) {
                // Send the saved messages out on the wire.
                for (auto & d : item.messages) {
                    // But *only* for this connection
                    // Not every connection in the broker.
                    ep.publish(
                        as::buffer(*(item.topic)),
                        as::buffer(*d.contents),
                        std::make_pair(item.topic, d.contents),
                        std::min(item.qos, d.qos),
                        true, // TODO: why is this 'retain'?
                        *(d.props)
                        );
                }
                subs_.emplace(item.topic, spep, item.qos);
            }
            idx.erase(range.begin(), range.end());
            BOOST_ASSERT(idx.count(client_id) == 0);
        }
        return true;
    }

    template <typename Endpoint>
    void disconnect_handler(
        Endpoint& ep
    ) {
        if (delay_disconnect_) {
            std::weak_ptr<std::remove_reference_t<decltype(ep)>> wp(ep.shared_from_this());
            tim_disconnect_.expires_from_now(delay_disconnect_.value());
            tim_disconnect_.async_wait(
                [&, wp](boost::system::error_code const& ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            close_proc(*sp, false);
                        }
                    }
                }
            );
        }
        else {
            close_proc(ep, false);
        }
    }

    template <typename Endpoint>
    bool publish_handler(
        Endpoint& ep,
        std::uint8_t header,
        mqtt::optional<typename Endpoint::packet_id_t> packet_id,
        mqtt::buffer topic_name,
        mqtt::buffer contents,
        std::vector<mqtt::v5::property_variant> props) {

        std::uint8_t qos = mqtt::publish::get_qos(header);
        bool is_retain = mqtt::publish::is_retain(header);
        do_publish(
            std::make_shared<std::string>(std::move(topic_name)),
            std::make_shared<std::string>(std::move(contents)),
            qos,
            is_retain,
            std::move(props));

        switch (ep.get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            switch (qos) {
            case mqtt::qos::at_least_once:
                ep.puback(packet_id.value());
                break;
            case mqtt::qos::exactly_once:
                ep.pubrec(packet_id.value());
                break;
            default:
                break;
            }
            break;
        case mqtt::protocol_version::v5:
            switch (qos) {
            case mqtt::qos::at_least_once:
                ep.puback(packet_id.value(), mqtt::v5::reason_code::success, puback_props_);
                break;
            case mqtt::qos::exactly_once:
                ep.pubrec(packet_id.value(), mqtt::v5::reason_code::success, pubrec_props_);
                break;
            default:
                break;
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        return true;
    }

    template <typename Endpoint>
    bool subscribe_handler(
        Endpoint& ep,
        typename Endpoint::packet_id_t packet_id,
        std::vector<std::tuple<mqtt::buffer, std::uint8_t>> entries,
        std::vector<mqtt::v5::property_variant> props) {

        // An in-order list of qos settings, used to send the reply.
        // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
        // allows the server to grant a lower QOS than requested
        // So we reply with the QOS setting that was granted
        // not the one requested.
        std::vector<std::uint8_t> res;
        res.reserve(entries.size());
        for (auto const& e : entries) {
            mqtt::buffer topic = std::get<0>(e);
            std::uint8_t qos = std::get<1>(e);
            res.emplace_back(qos);
            // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
            // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
            subs_.emplace(std::make_shared<std::string>(topic), ep.shared_from_this(), qos);
        }
        switch (ep.get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, res);
            break;
        case mqtt::protocol_version::v5:
            if (h_subscribe_props_) h_subscribe_props_(props);
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, res, suback_props_);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        for (auto const& e : entries) {
            mqtt::buffer topic = std::get<0>(e);
            std::uint8_t qos = std::get<1>(e);
            // Publish any retained messages that match the newly subscribed topic.
            auto it = retains_.find(std::string(topic));
            if (it != retains_.end()) {
                ep.publish(
                    as::buffer(*it->topic),
                    as::buffer(*it->contents),
                    std::make_pair(it->topic, it->contents),
                    std::min(it->qos, qos),
                    true,
                    it->props);
            }
        }
        return true;
    }

    template <typename Endpoint>
    bool unsubscribe_handler(
        Endpoint& ep,
        typename Endpoint::packet_id_t packet_id,
        std::vector<mqtt::buffer> topics,
        std::vector<mqtt::v5::property_variant> props) {

        auto spep = ep.shared_from_this();

        // For each subscription that this connection has
        // Compare against the list of topics, and remove
        // the subscription if the topic is in the list.
        {
            auto & idx = subs_.get<tag_con>();
            auto const& range = boost::make_iterator_range(idx.equal_range(spep));
            for(auto it = range.begin(); it != range.end(); ) {
                bool match = false;
                for(auto const& topic : topics) {
                    if(*(it->topic) == topic) {
                        /*
                         * Advance the iterator using the return from erase.
                         * The returned it may be equal to the next topic in
                         * the list, so we continue comparing. When the loop
                         * finishes, however, we can't blindly advance the
                         * iterator, because that might result in skipping
                         * an iterator that hasn't been checked. Further, we
                         * can't only rely on the inner loop to advance the
                         * iterator because we might not have a match, and
                         * thus would infinitely loop on the current iterator.
                         */
                        it = idx.erase(it);
                        match = true;
                        break;
                    }
                }
                if( ! match)
                {
                    std::advance(it, 1);
                }
            }
        }
        for(auto const& item : boost::make_iterator_range( subs_.get<tag_con>().equal_range(spep))) {
            for(auto const& topic : topics) {
                BOOST_ASSERT(*(item.topic) != topic);
            }
        }
        switch (ep.get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            ep.unsuback(packet_id);
            break;
        case mqtt::protocol_version::v5:
            if (h_unsubscribe_props_) h_unsubscribe_props_(props);
            ep.unsuback(packet_id, std::vector<std::uint8_t>(topics.size(), mqtt::v5::reason_code::success), unsuback_props_);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        return true;
    }

    /**
     * @brief do_publish Publish a message to any subscribed clients.
     *
     * @param topic - The topic to publish the message on.
     * @param contents - The contents of the message.
     * @param qos - The QOS setting to use for the published message.
     * @param is_retain - Whether the message should be retained so it can
     *                    be sent to newly added subscriptions in the future.\
     */
    void do_publish(
        std::shared_ptr<std::string> const& topic,
        std::shared_ptr<std::string> const& contents,
        std::uint8_t qos,
        bool is_retain,
        std::vector<mqtt::v5::property_variant> props) {
        // For each active subscription registered for this topic
        for(auto const& sub : boost::make_iterator_range(subs_.get<tag_topic>().equal_range(*topic))) {
            // publish the message to subscribers.
            // TODO: Probably this should be switched to async_publish?
            //       Given the async_client / sync_client seperation
            //       and the way they have different function names,
            //       it wouldn't be possible for test_broker.hpp to be
            //       used with some hypothetical "async_server" in the future.
            sub.con->publish(
                as::buffer(*topic),
                as::buffer(*contents),
                std::make_pair(topic, contents),
                std::min(sub.qos, qos),
                false,
                props // TODO: Copying the properties vector for each subscription.
            );
        }

        {
            // For each saved subscription, add this message to
            // the list to be sent out when a connection resumes
            // a lost session.
            //
            // TODO: This does not properly handle wildcards!
            auto & idx = saved_subs_.get<tag_topic>();
            auto range = boost::make_iterator_range(idx.equal_range(*topic));
            if( ! range.empty()) {
                auto sp_props = std::make_shared<std::vector<mqtt::v5::property_variant>>(props);
                for(auto it = range.begin(); it != range.end(); std::advance(it, 1)) {
                    idx.modify(it,
                               [&](session_subscription & val)
                               {
                                   val.messages.emplace_back(
                                       contents,
                                       sp_props,
                                       std::min(it->qos, qos));
                               },
                               [&](session_subscription&)
                               {
                                   BOOST_ASSERT(false);
                               });
                }
            }
        }

        /*
         * If the message is marked as being retained, then we
         * keep it in case a new subscription is added that matches
         * this topic.
         *
         * @note: The MQTT standard 3.3.1.3 RETAIN makes it clear that
         *        retained messages are global based on the topic, and
         *        are not scoped by the client id. So any client may
         *        publish a retained message on any topic, and the most
         *        recently published retained message on a particular
         *        topic is the message that is stored on the server.
         *
         * @note: The standard doesn't make it clear that publishing
         *        a message with zero length, but the retain flag not
         *        set, does not result in any existing retained message
         *        being removed. However, internet searching indicates
         *        that most brokers have opted to keep retained messages
         *        when receiving contents of zero bytes, unless the so
         *        received message has the retain flag set, in which case
         *        the retained message is removed.
         */
        if (is_retain) {
            if (contents->empty()) {
                retains_.erase(*topic);
                BOOST_ASSERT(retains_.count(*topic) == 0);
            }
            else {
                auto const& it = retains_.find(*topic);
                if(it == retains_.end()) {
                    auto const& ret = retains_.emplace(topic, contents, std::move(props), qos);
                    BOOST_ASSERT(ret.second);
                    BOOST_ASSERT(ret.first->topic == topic);
                    BOOST_ASSERT(ret.first->contents == contents);
                }
                else {
                    retains_.modify(it,
                                    [&](retain& val)
                                    {
                                        val.qos = qos;
                                        val.props = std::move(props);
                                        val.contents = contents;
                                    },
                                    [&](retain&)
                                    {
                                        BOOST_ASSERT(false);
                                    });
                }
            }
        }
    }

    /**
     * @brief close_proc - clean up a connection that has been closed.
     *
     * @param ep - The underlying mqtt::server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     */
    // TODO: Maybe change the name of this function.
    template <typename Endpoint>
    void close_proc(Endpoint& ep, bool send_will) {
        // Hold the lifetime of this endpoint until the function exits.
        auto spep = ep.shared_from_this();

        // Completely cut this endpoint off from the broker!
        ep.set_close_handler();
        ep.set_error_handler();
        ep.set_v5_connect_handler();
        ep.set_v5_disconnect_handler();
        ep.set_v5_puback_handler();
        ep.set_v5_pubrec_handler();
        ep.set_v5_pubrel_handler();
        ep.set_v5_pubcomp_handler();
        ep.set_v5_publish_handler();
        ep.set_v5_subscribe_handler();
        ep.set_v5_unsubscribe_handler();
        ep.set_pingreq_handler();
        ep.set_pingresp_handler();
        ep.set_v5_auth_handler();

        auto & act_sess_idx = active_sessions_.get<tag_con>();
        auto act_sess_it = act_sess_idx.find(spep);

        // It shouldn't be possible for this function to be called
        // multiple times with the same endpoint, as we've severed
        // this endpoint's connection with the broker above.
        BOOST_ASSERT(act_sess_it != act_sess_idx.end());

        std::shared_ptr<std::string> sp_client_id;
        mqtt::optional<mqtt::will> will;
        if(ep.clean_session() && (ep.get_protocol_version() == mqtt::protocol_version::v3_1_1)) {
            sp_client_id = act_sess_it->client_id;
            will = act_sess_it->will;
            act_sess_idx.erase(act_sess_it);

            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(*sp_client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(*sp_client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(*sp_client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(*sp_client_id) == non_active_sessions_.get<tag_client_id>().end());
        }
        else
        {
            session_state state = *act_sess_it;
            sp_client_id = state.client_id;
            will = state.will;

            // TODO: Should yank out the messages from this connection object and store it in the session_state object??
            state.con.reset(); // clear the shared pointer, so it doesn't stay alive after this funciton ends.
            act_sess_idx.erase(act_sess_it);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(*sp_client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(*sp_client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(*sp_client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(*sp_client_id) == non_active_sessions_.get<tag_client_id>().end());

            auto const& ret = non_active_sessions_.insert(std::move(state));
            BOOST_ASSERT(ret.second);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(*sp_client_id) == 1);
            BOOST_ASSERT(ret.first->client_id == sp_client_id);
            BOOST_ASSERT(ret.first == non_active_sessions_.get<tag_client_id>().find(*sp_client_id));
        }

        // Before sending the will, either erase, or save, any subscriptions that
        // match this connection. It's possible that none match, and that's fine.
        {
            auto& idx = subs_.get<tag_con>();
            auto const& range = boost::make_iterator_range(idx.equal_range(spep));
            // In v3_1_1, we need to clear any subscriptions at the end of the session.
            // In v5, we always save them, and clear them at the beginning of a session.
            if (ep.clean_session() && (ep.get_protocol_version() == mqtt::protocol_version::v3_1_1)) {
                // Remove all subscriptions for this clientid
                idx.erase(range.begin(), range.end());
            }
            else if( ! range.empty()) {
                // Save all the subscriptions for this clientid for later.
                for(auto const& item : range) {
                    auto const& ret = saved_subs_.emplace(sp_client_id,
                                                          item.topic,
                                                          item.qos);
                    BOOST_ASSERT(ret.second);
                    BOOST_ASSERT(ret.first == saved_subs_.find(*sp_client_id));
                }
                idx.erase(range.begin(), range.end());
            }
            BOOST_ASSERT(idx.count(spep) == 0);
        }

        if(send_will && will) {
            // TODO: This should be triggered by the will delay
            // Not sent immediately.
            do_publish(
                std::make_shared<std::string>(will.value().topic()),
                std::make_shared<std::string>(will.value().message()),
                will.value().qos(),
                will.value().retain(),
                will.value().props());
        }
    }

private:
    struct tag_con {};
    struct tag_topic {};
    struct tag_client_id {};

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
        session_state(std::shared_ptr<std::string> client_id, con_sp_t con, mqtt::optional<mqtt::will> will)
            :client_id(std::move(client_id)), con(std::move(con)), will(std::move(will)) {}

        session_state() = default;
        session_state(session_state &&) = default;
        session_state(session_state const&) = default;
        session_state& operator=(session_state &&) = default;
        session_state& operator=(session_state const&) = default;

        mqtt::string_view get_client_id() const { return *client_id; }

        std::shared_ptr<std::string> client_id;
        con_sp_t con;

        // TODO:
        // Messages sent to client, but not acknowledged.
        // Messages pending transmission to client
        // messages received from client, but not acknowledged
        mqtt::optional<mqtt::will> will;
        mqtt::optional<boost::posix_time::time_duration> will_delay;
        mqtt::optional<boost::posix_time::time_duration> session_expiry_interval;
    };

    // The mi_active_sessions container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_active_sessions = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(session_state, mqtt::string_view, session_state::get_client_id)
            >,
            mi::ordered_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(session_state, con_sp_t, con)
            >
        >
    >;

    // The mi_active_sessions container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_non_active_sessions = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(session_state, mqtt::string_view, session_state::get_client_id)
            >
        >
    >;

    // Mapping between connection object and subscription topics
    struct sub_con {
        sub_con(
            std::shared_ptr<std::string> topic,
            con_sp_t con,
            std::uint8_t qos)
            :topic(std::move(topic)), con(std::move(con)), qos(qos) {}
        mqtt::string_view get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        con_sp_t con;
        std::uint8_t qos;
    };
    using mi_sub_con = mi::multi_index_container<
        sub_con,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_con, mqtt::string_view, sub_con::get_topic)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
            >,
            // Don't allow the same connection object to have the same topic multiple times.
            // Note that this index does not get used by any code in the broker
            // other than to enforce the uniqueness constraints.
            // Potentially this can be enabled only in debug builds.
            mi::ordered_unique<
                mi::composite_key<
                    sub_con,
                    BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con),
                    BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_con, mqtt::string_view, sub_con::get_topic)
                >
            >
        >
    >;

    // A collection of topics that have been retained in
    // case clients add a new subscription to the topic.
    struct retain {
        retain(
            std::shared_ptr<std::string> topic,
            std::shared_ptr<std::string> contents,
            std::vector<mqtt::v5::property_variant> props,
            std::uint8_t qos)
            :topic(std::move(topic)), contents(std::move(contents)), props(std::move(props)), qos(qos) {}
        mqtt::string_view get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        std::shared_ptr<std::string> contents;
        std::vector<mqtt::v5::property_variant> props;
        std::uint8_t qos;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(retain, mqtt::string_view, retain::get_topic)
            >
        >
    >;

    // The saved_message structure holds messages that have been published on a
    // topic that a not-currently-connected client is subscribed to.
    // When a new connection is made with the client id for this saved data,
    // these messages will be published to that client, and only that client.
    struct saved_message {
        saved_message(
            std::shared_ptr<std::string> contents,
            std::shared_ptr<std::vector<mqtt::v5::property_variant>> props,
            std::uint8_t qos)
            : contents(std::move(contents)), props(std::move(props)), qos(qos) {}
        std::shared_ptr<std::string> contents;
        std::shared_ptr<std::vector<mqtt::v5::property_variant>> props;
        std::uint8_t qos;
    };

    // Each instance of session_subscription describes a subscription that the associated client id has made
    // and a collection of data associated with that subscription to be sent when the client reconnects.
    struct session_subscription {
        session_subscription(
            std::shared_ptr<std::string> client_id,
            std::shared_ptr<std::string> topic,
            std::uint8_t qos)
            :client_id(std::move(client_id)), topic(std::move(topic)), qos(qos) {}
        mqtt::string_view get_client_id() const {
            return *client_id;
        }
        mqtt::string_view get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> client_id;
        std::shared_ptr<std::string> topic;
        std::vector<saved_message> messages;
        std::uint8_t qos;
    };

    using mi_session_subscription = mi::multi_index_container<
        session_subscription,
        mi::indexed_by<
            // Allow multiple client id's for the same topic
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(session_subscription, mqtt::string_view, session_subscription::get_client_id)
            >,
            // Allow multiple topics for the same client id
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(session_subscription, mqtt::string_view, session_subscription::get_topic)
            >,
            // Don't allow the same client id to have the same topic multiple times.
            // Note that this index does not get used by any code in the broker
            // other than to enforce the uniqueness constraints.
            // Potentially this can be enabled only in debug builds.
            mi::ordered_unique<
                mi::composite_key<
                    session_subscription,
                    BOOST_MULTI_INDEX_CONST_MEM_FUN(session_subscription, mqtt::string_view, session_subscription::get_client_id),
                    BOOST_MULTI_INDEX_CONST_MEM_FUN(session_subscription, mqtt::string_view, session_subscription::get_topic)
                >
            >
        >
    >;

    as::io_service& ios_; ///< The boost asio context to run this broker on.
    as::deadline_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    mqtt::optional<boost::posix_time::time_duration> delay_disconnect_; ///< Used to delay disconnect handling for testing

    mi_active_sessions active_sessions_; ///< Map of active client id and connections
    mi_non_active_sessions non_active_sessions_; ///< Storage for sessions not currently active. Indexed by client id.
    mi_sub_con subs_; ///< Map of topic subscriptions to client ids
    mi_session_subscription saved_subs_; ///< Topics and associated messages for clientids that are currently disconnected
    mi_retain retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.

    // MQTTv5 members
    std::vector<mqtt::v5::property_variant> connack_props_;
    std::vector<mqtt::v5::property_variant> suback_props_;
    std::vector<mqtt::v5::property_variant> unsuback_props_;
    std::vector<mqtt::v5::property_variant> puback_props_;
    std::vector<mqtt::v5::property_variant> pubrec_props_;
    std::vector<mqtt::v5::property_variant> pubrel_props_;
    std::vector<mqtt::v5::property_variant> pubcomp_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_connect_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_disconnect_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_publish_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_puback_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_pubrec_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_pubrel_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_pubcomp_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_subscribe_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_unsubscribe_props_;
    std::function<void(std::vector<mqtt::v5::property_variant> const&)> h_auth_props_;
};

#endif // MQTT_TEST_BROKER_HPP

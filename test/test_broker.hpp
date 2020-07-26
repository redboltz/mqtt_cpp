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

using endpoint_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<endpoint_t>;
using con_wp_t = std::weak_ptr<endpoint_t>;
using packet_id_t = endpoint_t::packet_id_t;

class test_broker {
public:
    test_broker(as::io_context& ioc)
        :ioc_(ioc),
         tim_disconnect_(ioc_)
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
    void set_disconnect_delay(std::chrono::steady_clock::duration delay) {
        delay_disconnect_ = MQTT_NS::force_move(delay);
    }

    /**
     * @brief set pingresp send operaton
     *
     * @param b - if true, send pingresp when pingreq is received.
     *            if false, doesn't send pingrespp for test.
     */
    void set_pingresp(bool b) {
        pingresp_ = b;
    }
    // [end] for test setting

    /**
     * @brief handle_accept
     *
     * Call this function when an MQTT_NS::server (of whatever kind) has accepted a raw
     * connection from an MQTT client. By 'raw connection', this might be raw TCP sockets
     * or websockets, or completed a TLS handshake, or any other underlying transport
     * type, but what is not meant is that the mqtt client on the other end of the endpoint
     * has initiated the MQTT application protocol connection sequence with CONNECT or CONACK
     * messages being sent or received.
     *
     * This function will assign several event handlers into MQTT_NS::server (of whatever kind)
     * that is provided as a parameter. This includes connection handlers, disconnection handlers
     * and various handlers for a variety of of MQTT message types.
     *
     * @param ep - The MQTT_NS::server (of whichever kind) to accept a connection on.
     */
    void handle_accept(con_sp_t spep) {
        con_wp_t wp(spep);
        endpoint_t& ep = *spep;

        ep.socket().lowest_layer().set_option(as::ip::tcp::no_delay(true));
        ep.set_auto_pub_response(false);
        // Pass spep to keep lifetime.
        // It makes sure wp.lock() never return nullptr in the handlers below
        // including close_handler and error_handler.
        ep.start_session(spep);

        // set connection (lower than MQTT) level handlers
        ep.set_close_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                close_proc(MQTT_NS::force_move(sp), true);
            });
        ep.set_error_handler(
            [this, wp]
            (MQTT_NS::error_code /*ec*/){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                close_proc(MQTT_NS::force_move(sp), true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [this, wp]
            (MQTT_NS::buffer client_id,
             MQTT_NS::optional<MQTT_NS::buffer> username,
             MQTT_NS::optional<MQTT_NS::buffer> password,
             MQTT_NS::optional<MQTT_NS::will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    MQTT_NS::force_move(sp),
                    MQTT_NS::force_move(client_id),
                    MQTT_NS::force_move(username),
                    MQTT_NS::force_move(password),
                    MQTT_NS::force_move(will),
                    clean_session,
                    keep_alive,
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_connect_handler(
            [this, wp]
            (MQTT_NS::buffer client_id,
             MQTT_NS::optional<MQTT_NS::buffer> username,
             MQTT_NS::optional<MQTT_NS::buffer> password,
             MQTT_NS::optional<MQTT_NS::will> will,
             bool clean_session,
             std::uint16_t keep_alive,
             MQTT_NS::v5::properties props) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    MQTT_NS::force_move(sp),
                    MQTT_NS::force_move(client_id),
                    MQTT_NS::force_move(username),
                    MQTT_NS::force_move(password),
                    MQTT_NS::force_move(will),
                    clean_session,
                    keep_alive,
                    MQTT_NS::force_move(props)
                );
            }
        );
        ep.set_disconnect_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(MQTT_NS::force_move(sp));
            }
        );
        ep.set_v5_disconnect_handler(
            [this, wp]
            (MQTT_NS::v5::disconnect_reason_code /*reason_code*/, MQTT_NS::v5::properties props) {
                if (h_disconnect_props_) h_disconnect_props_(MQTT_NS::force_move(props));
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(MQTT_NS::force_move(sp));
            }
        );
        ep.set_puback_handler(
            []
            (packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_puback_handler(
            []
            (packet_id_t /*packet_id*/,
             MQTT_NS::v5::puback_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                return true;
            });
        ep.set_pubrec_handler(
            [wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubrel(packet_id);
                return true;
            });
        ep.set_v5_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id,
             MQTT_NS::v5::pubrec_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubrel(packet_id, MQTT_NS::v5::pubrel_reason_code::success, pubrel_props_);
                return true;
            });
        ep.set_pubrel_handler(
            [wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubcomp(packet_id);
                return true;
            });
        ep.set_v5_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id,
             MQTT_NS::v5::pubrel_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubcomp(packet_id, MQTT_NS::v5::pubcomp_reason_code::success, pubcomp_props_);
                return true;
            });
        ep.set_pubcomp_handler(
            []
            (packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_pubcomp_handler(
            []
            (packet_id_t /*packet_id*/,
             MQTT_NS::v5::pubcomp_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                return true;
            });
        ep.set_publish_handler(
            [this, wp]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic_name,
             MQTT_NS::buffer contents){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    pubopts,
                    MQTT_NS::force_move(topic_name),
                    MQTT_NS::force_move(contents),
                    MQTT_NS::v5::properties{}
                );
            });
        ep.set_v5_publish_handler(
            [this, wp]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic_name,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    pubopts,
                    MQTT_NS::force_move(topic_name),
                    MQTT_NS::force_move(contents),
                    MQTT_NS::force_move(props)
                );
            });
        ep.set_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(entries),
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries,
             MQTT_NS::v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(entries),
                    MQTT_NS::force_move(props)
                );
            }
        );
        ep.set_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<MQTT_NS::buffer> topics) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(topics),
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<MQTT_NS::buffer> topics,
             MQTT_NS::v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(topics),
                    MQTT_NS::force_move(props)
                );
            }
        );
        ep.set_pingreq_handler(
            [this, wp] {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                if (pingresp_) sp->pingresp();
                return true;
            }
        );
        ep.set_v5_auth_handler(
            [this]
            (MQTT_NS::v5::auth_reason_code /*reason_code*/,
             MQTT_NS::v5::properties props
            ) {
                if (h_auth_props_) h_auth_props_(MQTT_NS::force_move(props));
                return true;
            }
        );
    }

    void set_connack_props(MQTT_NS::v5::properties props) {
        connack_props_ = MQTT_NS::force_move(props);
    }

    void set_suback_props(MQTT_NS::v5::properties props) {
        suback_props_ = MQTT_NS::force_move(props);
    }

    void set_unsuback_props(MQTT_NS::v5::properties props) {
        unsuback_props_ = MQTT_NS::force_move(props);
    }

    void set_puback_props(MQTT_NS::v5::properties props) {
        puback_props_ = MQTT_NS::force_move(props);
    }

    void set_pubrec_props(MQTT_NS::v5::properties props) {
        pubrec_props_ = MQTT_NS::force_move(props);
    }

    void set_pubrel_props(MQTT_NS::v5::properties props) {
        pubrel_props_ = MQTT_NS::force_move(props);
    }

    void set_pubcomp_props(MQTT_NS::v5::properties props) {
        pubcomp_props_ = MQTT_NS::force_move(props);
    }

    void set_connect_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_connect_props_ = MQTT_NS::force_move(h);
    }

    void set_disconnect_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_disconnect_props_ = MQTT_NS::force_move(h);
    }

    void set_publish_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_publish_props_ = MQTT_NS::force_move(h);
    }

    void set_puback_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_puback_props_ = MQTT_NS::force_move(h);
    }

    void set_pubrec_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubrec_props_ = MQTT_NS::force_move(h);
    }

    void set_pubrel_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubrel_props_ = MQTT_NS::force_move(h);
    }

    void set_pubcomp_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubcomp_props_ = MQTT_NS::force_move(h);
    }

    void set_subscribe_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_subscribe_props_ = MQTT_NS::force_move(h);
    }

    void set_unsubscribe_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_unsubscribe_props_ = MQTT_NS::force_move(h);
    }

    void set_auth_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_auth_props_ = MQTT_NS::force_move(h);
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
     * @param spep - MQTT_NS::varient of shared pointers to underlying connection type.
     * @param client_id - the id that the client wants to use
     * @param will - the last-will-and-testiment of the connection, if any.
     */
    bool connect_handler(
        con_sp_t spep,
        MQTT_NS::buffer client_id,
        MQTT_NS::optional<MQTT_NS::buffer> /*username*/,
        MQTT_NS::optional<MQTT_NS::buffer> /*password*/,
        MQTT_NS::optional<MQTT_NS::will> will,
        bool clean_session,
        std::uint16_t /*keep_alive*/,
        MQTT_NS::v5::properties props
    ) {
        auto& ep = *spep;

        MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval;

        if (ep.get_protocol_version() == MQTT_NS::protocol_version::v5) {
            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&session_expiry_interval](MQTT_NS::v5::property::session_expiry_interval const& t) {
                            if (t.val() != 0) {
                                session_expiry_interval.emplace(std::chrono::seconds(t.val()));
                            }
                        },
                        [](auto&& ...) {
                        }
                    ),
                    p
                );
            }

            if (h_connect_props_) {
                h_connect_props_(props);
            }
        }

        // If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
        // If it's a not a clean session, but no client id is provided, we would have no way to map this
        // connection's session to a new connection later. So the connection must be rejected.
        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            if (client_id.empty() && !clean_session) {
                ep.connack(false, MQTT_NS::connect_return_code::identifier_rejected);
                return false;
            }
            break;
        case MQTT_NS::protocol_version::v5:
            if (client_id.empty() && !clean_session) {
                ep.connack(false, MQTT_NS::v5::connect_reason_code::client_identifier_not_valid);
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

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
        case MQTT_NS::protocol_version::v3_1_1:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                MQTT_NS::connect_return_code::accepted
            );
            break;
        case MQTT_NS::protocol_version::v5:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                MQTT_NS::v5::connect_reason_code::success,
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
            if(non_act_sess_it == non_act_sess_idx.end()) {
                auto const& ret = active_sessions_.emplace(spep,
                                                           client_id,
                                                           MQTT_NS::force_move(will),
                                                           MQTT_NS::force_move(session_expiry_interval));
                BOOST_ASSERT(ret.second);
                act_sess_it = active_sessions_.project<tag_client_id>(ret.first);
                BOOST_ASSERT(act_sess_it->client_id == client_id);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
            }
            else {
                session_state state;
                non_act_sess_idx.modify(non_act_sess_it,
                                        [&](session_state & val) { state = val; },
                                        [](session_state&) { BOOST_ASSERT(false); });
                state.con = spep;
                non_act_sess_idx.erase(non_act_sess_it);
                BOOST_ASSERT(non_act_sess_idx.end() == non_act_sess_idx.find(client_id));

                auto const& ret = active_sessions_.insert(MQTT_NS::force_move(state));
                BOOST_ASSERT(ret.second);
                act_sess_it = active_sessions_.project<tag_client_id>(ret.first);
                BOOST_ASSERT(act_sess_it->client_id == client_id);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
                BOOST_ASSERT(active_sessions_.project<tag_con>(act_sess_it) == active_sessions_.get<tag_con>().find(spep));
            }
        }
        else {
            // Disconnect the existing connection

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
                                            [](con_sp_t&) { BOOST_ASSERT(false); });
                    }
                }
                BOOST_ASSERT(subs_idx.count(act_sess_it->con) == 0);
            }
            active_sessions_.get<tag_con>().modify_key(active_sessions_.project<tag_con>(act_sess_it),
                                                       [&](con_sp_t & val) { val = spep; },
                                                       [](con_sp_t&) { BOOST_ASSERT(false); });
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
                        as::buffer(item.topic),
                        as::buffer(d.contents),
                        // TODO: why is this 'retain'?
                        std::min(item.qos_value, d.qos_value) | MQTT_NS::retain::yes,
                        *(d.props),
                        std::make_tuple(item.topic, d.contents, *(d.props))
                        );
                }
                subs_.emplace(item.topic, spep, item.qos_value, item.rap_value);
            }
            idx.erase(range.begin(), range.end());
            BOOST_ASSERT(idx.count(client_id) == 0);
        }
        return true;
    }

    void disconnect_handler(
        con_sp_t spep
    ) {
        if (delay_disconnect_) {
            tim_disconnect_.expires_after(delay_disconnect_.value());
            tim_disconnect_.async_wait(
                [&, wp = con_wp_t(MQTT_NS::force_move(spep))](MQTT_NS::error_code ec) {
                    if (!ec) {
                        if (con_sp_t sp = wp.lock()) {
                            close_proc(MQTT_NS::force_move(sp), false);
                        }
                    }
                }
            );
        }
        else {
            close_proc(MQTT_NS::force_move(spep), false);
        }
    }

    bool publish_handler(
        con_sp_t spep,
        MQTT_NS::optional<packet_id_t> packet_id,
        MQTT_NS::publish_options pubopts,
        MQTT_NS::buffer topic_name,
        MQTT_NS::buffer contents,
        MQTT_NS::v5::properties props) {

        auto& ep = *spep;
        do_publish(
            MQTT_NS::force_move(topic_name),
            MQTT_NS::force_move(contents),
            pubopts.get_qos() | pubopts.get_retain(), // remove dup flag
            MQTT_NS::force_move(props));

        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            switch (pubopts.get_qos()) {
            case MQTT_NS::qos::at_least_once:
                ep.puback(packet_id.value());
                break;
            case MQTT_NS::qos::exactly_once:
                ep.pubrec(packet_id.value());
                break;
            default:
                break;
            }
            break;
        case MQTT_NS::protocol_version::v5:
            switch (pubopts.get_qos()) {
            case MQTT_NS::qos::at_least_once:
                ep.puback(packet_id.value(), MQTT_NS::v5::puback_reason_code::success, puback_props_);
                break;
            case MQTT_NS::qos::exactly_once:
                ep.pubrec(packet_id.value(), MQTT_NS::v5::pubrec_reason_code::success, pubrec_props_);
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

    bool subscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries,
        MQTT_NS::v5::properties props) {

        auto& ep = *spep;

        // An in-order list of qos settings, used to send the reply.
        // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
        // allows the server to grant a lower QOS than requested
        // So we reply with the QOS setting that was granted
        // not the one requested.
        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
        {
            std::vector<MQTT_NS::suback_return_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                MQTT_NS::buffer topic = std::get<0>(e);
                MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                res.emplace_back(MQTT_NS::qos_to_suback_return_code(qos_value)); // converts to granted_qos_x
                // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
                // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
                subs_.emplace(MQTT_NS::force_move(topic), spep, qos_value);
            }
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, MQTT_NS::force_move(res));
            break;
        }
        case MQTT_NS::protocol_version::v5:
        {
            std::vector<MQTT_NS::v5::suback_reason_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                MQTT_NS::buffer topic = std::get<0>(e);
                MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                MQTT_NS::rap rap_value = std::get<1>(e).get_rap();
                res.emplace_back(MQTT_NS::v5::qos_to_suback_reason_code(qos_value)); // converts to granted_qos_x
                // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
                // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
                subs_.emplace(MQTT_NS::force_move(topic), spep, qos_value, rap_value);
            }
            if (h_subscribe_props_) h_subscribe_props_(props);
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, MQTT_NS::force_move(res), suback_props_);
            break;
        }
        default:
            BOOST_ASSERT(false);
            break;
        }

        for (auto const& e : entries) {
            MQTT_NS::buffer const& topic = std::get<0>(e);
            MQTT_NS::subscribe_options options = std::get<1>(e);
            // Publish any retained messages that match the newly subscribed topic.
            auto it = retains_.find(topic);
            if (it != retains_.end()) {
                ep.publish(
                    as::buffer(it->topic),
                    as::buffer(it->contents),
                    std::min(it->qos_value, options.get_qos()) | MQTT_NS::retain::yes,
                    it->props,
                    std::make_pair(it->topic, it->contents)
                );
            }
        }
        return true;
    }

    bool unsubscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<MQTT_NS::buffer> topics,
        MQTT_NS::v5::properties props) {

        auto& ep = *spep;

        // For each subscription that this connection has
        // Compare against the list of topics, and remove
        // the subscription if the topic is in the list.
        {
            auto & idx = subs_.get<tag_con>();
            auto const& range = boost::make_iterator_range(idx.equal_range(spep));
            for(auto it = range.begin(); it != range.end(); ) {
                bool match = false;
                for(auto const& topic : topics) {
                    if(it->topic == topic) {
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
            (void)item;
            for(auto const& topic : topics) {
                (void)topic;
                BOOST_ASSERT(item.topic != topic);
            }
        }

        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            ep.unsuback(packet_id);
            break;
        case MQTT_NS::protocol_version::v5:
            if (h_unsubscribe_props_) h_unsubscribe_props_(props);
            ep.unsuback(packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code>(topics.size(), MQTT_NS::v5::unsuback_reason_code::success), unsuback_props_);
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
        MQTT_NS::buffer topic,
        MQTT_NS::buffer contents,
        MQTT_NS::publish_options pubopts,
        MQTT_NS::v5::properties props) {
        // For each active subscription registered for this topic
        for(auto const& sub : boost::make_iterator_range(subs_.get<tag_topic>().equal_range(topic))) {
            // publish the message to subscribers.
            // TODO: Probably this should be switched to async_publish?
            //       Given the async_client / sync_client seperation
            //       and the way they have different function names,
            //       it wouldn't be possible for test_broker.hpp to be
            //       used with some hypothetical "async_server" in the future.

            // retain is delivered as the original only if rap_value is rap::retain.
            // On MQTT v3.1.1, rap_value is always rap::dont.
            auto retain =
                [&] {
                    if (sub.rap_value == MQTT_NS::rap::retain) {
                        return pubopts.get_retain();
                    }
                    return MQTT_NS::retain::no;
                } ();
            sub.con->publish(
                topic,
                contents,
                std::min(sub.qos_value, pubopts.get_qos()) | retain,
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
            auto range = boost::make_iterator_range(idx.equal_range(topic));
            if( ! range.empty()) {
                auto sp_props = std::make_shared<MQTT_NS::v5::properties>(props);
                for(auto it = range.begin(); it != range.end(); std::advance(it, 1)) {
                    idx.modify(it,
                               [&](session_subscription & val)
                               {
                                   val.messages.emplace_back(
                                       contents,
                                       sp_props,
                                       std::min(it->qos_value, pubopts.get_qos()));
                               },
                               [](session_subscription&) { BOOST_ASSERT(false); });
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
        if (pubopts.get_retain() == MQTT_NS::retain::yes) {
            if (contents.empty()) {
                retains_.erase(topic);
                BOOST_ASSERT(retains_.count(topic) == 0);
            }
            else {
                auto const& it = retains_.find(topic);
                if(it == retains_.end()) {
                    auto const& ret = retains_.emplace(MQTT_NS::force_move(topic),
                                                       MQTT_NS::force_move(contents),
                                                       MQTT_NS::force_move(props),
                                                       pubopts.get_qos());
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                }
                else {
                    retains_.modify(it,
                                    [&](retain& val)
                                    {
                                        val.qos_value = pubopts.get_qos();
                                        val.props = MQTT_NS::force_move(props);
                                        val.contents = MQTT_NS::force_move(contents);
                                    },
                                    [](retain&) { BOOST_ASSERT(false); });
                }
            }
        }
    }

    /**
     * @brief close_proc - clean up a connection that has been closed.
     *
     * @param ep - The underlying MQTT_NS::server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     */
    // TODO: Maybe change the name of this function.
    void close_proc(con_sp_t spep, bool send_will) {
        endpoint_t& ep = *spep;

        auto & act_sess_idx = active_sessions_.get<tag_con>();
        auto act_sess_it = act_sess_idx.find(spep);

        // act_sess_it == act_sess_idx.end() could happen if broker accepts
        // the session from client but the client closes the session  before sending
        // MQTT `CONNECT` message.
        // In this case, do nothing is correct behavior.
        if (act_sess_it == act_sess_idx.end()) return;

        MQTT_NS::buffer client_id;
        MQTT_NS::optional<MQTT_NS::will> will;
        bool session_clear = !act_sess_it->session_expiry_interval;
        if (ep.clean_session() && session_clear) {
            client_id = std::move(act_sess_it->client_id);
            will = std::move(act_sess_it->will);

            act_sess_idx.erase(act_sess_it);

            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(client_id) == non_active_sessions_.get<tag_client_id>().end());
        }
        else {
            session_state state = std::move(*act_sess_it);
            client_id = state.client_id;
            will = std::move(state.will);

            // TODO: Should yank out the messages from this connection object and store it in the session_state object??
            state.con.reset(); // clear the shared pointer, so it doesn't stay alive after this funciton ends.

            act_sess_idx.erase(act_sess_it);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(client_id) == non_active_sessions_.get<tag_client_id>().end());

            auto const& ret = non_active_sessions_.insert(MQTT_NS::force_move(state));
            (void)ret;
            BOOST_ASSERT(ret.second);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 1);
            BOOST_ASSERT(ret.first->client_id == client_id);
            BOOST_ASSERT(ret.first == non_active_sessions_.get<tag_client_id>().find(client_id));
        }

        // Before sending the will, either erase, or save, any subscriptions that
        // match this connection. It's possible that none match, and that's fine.
        {
            auto& idx = subs_.get<tag_con>();
            auto const& range = boost::make_iterator_range(idx.equal_range(spep));
            // In v3_1_1, session_expiry_interval is not set. So clean on close.
            if (ep.clean_session() && session_clear) {
                // Remove all subscriptions for this clientid
                idx.erase(range.begin(), range.end());
            }
            else if( ! range.empty()) {
                // Save all the subscriptions for this clientid for later.
                for(auto const& item : range) {
                    auto const& ret = saved_subs_.emplace(client_id,
                                                          item.topic,
                                                          item.qos_value,
                                                          item.rap_value);
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                    BOOST_ASSERT(ret.first == saved_subs_.find(client_id));
                }
                idx.erase(range.begin(), range.end());
            }
            BOOST_ASSERT(idx.count(spep) == 0);
        }

        if(send_will && will) {
            // TODO: This should be triggered by the will delay
            // Not sent immediately.
            do_publish(
                MQTT_NS::force_move(will.value().topic()),
                MQTT_NS::force_move(will.value().message()),
                will.value().get_qos() | will.value().get_retain(),
                MQTT_NS::force_move(will.value().props()));
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
        session_state(
            con_sp_t con,
            MQTT_NS::buffer client_id,
            MQTT_NS::optional<MQTT_NS::will> will,
            MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval = MQTT_NS::nullopt)
            :con(MQTT_NS::force_move(con)),
             client_id(MQTT_NS::force_move(client_id)),
             will(MQTT_NS::force_move(will)),
             session_expiry_interval(MQTT_NS::force_move(session_expiry_interval))
        {}

        session_state() = default;
        session_state(session_state &&) = default;
        session_state(session_state const&) = default;
        session_state& operator=(session_state &&) = default;
        session_state& operator=(session_state const&) = default;

        con_sp_t con;
        MQTT_NS::buffer client_id;

        // TODO:
        // Messages sent to client, but not acknowledged.
        // Messages pending transmission to client
        // messages received from client, but not acknowledged
        MQTT_NS::optional<MQTT_NS::will> will;
        MQTT_NS::optional<std::chrono::steady_clock::duration> will_delay;
        MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval;
    };

    // The mi_active_sessions container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_active_sessions = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(session_state, con_sp_t, con)
            >,
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(session_state, MQTT_NS::buffer, client_id)
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
                BOOST_MULTI_INDEX_MEMBER(session_state, MQTT_NS::buffer, client_id)
            >
        >
    >;

    // Mapping between connection object and subscription topics
    struct sub_con {
        sub_con(
            MQTT_NS::buffer topic,
            con_sp_t con,
            MQTT_NS::qos qos_value,
            MQTT_NS::rap rap_value = MQTT_NS::rap::dont)
            :topic(MQTT_NS::force_move(topic)), con(MQTT_NS::force_move(con)), qos_value(qos_value), rap_value(rap_value) {}
        MQTT_NS::buffer topic;
        con_sp_t con;
        MQTT_NS::qos qos_value;
        MQTT_NS::rap rap_value;
    };
    using mi_sub_con = mi::multi_index_container<
        sub_con,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
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
                    BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
                >
            >
        >
    >;

    // A collection of messages that have been retained in
    // case clients add a new subscription to the associated topics.
    struct retain {
        retain(
            MQTT_NS::buffer topic,
            MQTT_NS::buffer contents,
            MQTT_NS::v5::properties props,
            MQTT_NS::qos qos_value)
            :topic(MQTT_NS::force_move(topic)),
             contents(MQTT_NS::force_move(contents)),
             props(MQTT_NS::force_move(props)),
             qos_value(qos_value)
        { }
        MQTT_NS::buffer topic;
        MQTT_NS::buffer contents;
        MQTT_NS::v5::properties props;
        MQTT_NS::qos qos_value;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(retain, MQTT_NS::buffer, topic)
            >
        >
    >;

    // The saved_message structure holds messages that have been published on a
    // topic that a not-currently-connected client is subscribed to.
    // When a new connection is made with the client id for this saved data,
    // these messages will be published to that client, and only that client.
    struct saved_message {
        saved_message(
            MQTT_NS::buffer contents,
            std::shared_ptr<MQTT_NS::v5::properties> props,
            MQTT_NS::qos qos_value)
            : contents(MQTT_NS::force_move(contents)), props(MQTT_NS::force_move(props)), qos_value(qos_value) {}
        MQTT_NS::buffer contents;
        std::shared_ptr<MQTT_NS::v5::properties> props;
        MQTT_NS::qos qos_value;
    };

    // Each instance of session_subscription describes a subscription that the associated client id has made
    // and a collection of data associated with that subscription to be sent when the client reconnects.
    struct session_subscription {
        session_subscription(
            MQTT_NS::buffer client_id,
            MQTT_NS::buffer topic,
            MQTT_NS::qos qos_value,
            MQTT_NS::rap rap_value)
            :client_id(MQTT_NS::force_move(client_id)), topic(MQTT_NS::force_move(topic)), qos_value(qos_value), rap_value(rap_value) {}
        MQTT_NS::buffer client_id;
        MQTT_NS::buffer topic;
        std::vector<saved_message> messages;
        MQTT_NS::qos qos_value;
        MQTT_NS::rap rap_value;
    };

    using mi_session_subscription = mi::multi_index_container<
        session_subscription,
        mi::indexed_by<
            // Allow multiple client id's for the same topic
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, client_id)
            >,
            // Allow multiple topics for the same client id
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, topic)
            >,
            // Don't allow the same client id to have the same topic multiple times.
            // Note that this index does not get used by any code in the broker
            // other than to enforce the uniqueness constraints.
            // Potentially this can be enabled only in debug builds.
            mi::ordered_unique<
                mi::composite_key<
                    session_subscription,
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, topic),
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, client_id)
                >
            >
        >
    >;

    as::io_context& ioc_; ///< The boost asio context to run this broker on.
    as::steady_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    MQTT_NS::optional<std::chrono::steady_clock::duration> delay_disconnect_; ///< Used to delay disconnect handling for testing

    mi_active_sessions active_sessions_; ///< Map of active client id and connections
    mi_non_active_sessions non_active_sessions_; ///< Storage for sessions not currently active. Indexed by client id.
    mi_sub_con subs_; ///< Map of topic subscriptions to client ids
    mi_session_subscription saved_subs_; ///< Topics and associated messages for clientids that are currently disconnected
    mi_retain retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.

    // MQTTv5 members
    MQTT_NS::v5::properties connack_props_;
    MQTT_NS::v5::properties suback_props_;
    MQTT_NS::v5::properties unsuback_props_;
    MQTT_NS::v5::properties puback_props_;
    MQTT_NS::v5::properties pubrec_props_;
    MQTT_NS::v5::properties pubrel_props_;
    MQTT_NS::v5::properties pubcomp_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_connect_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_disconnect_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_publish_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_puback_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubrec_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubrel_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubcomp_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_subscribe_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_unsubscribe_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_auth_props_;
    bool pingresp_ = true;
};

#endif // MQTT_TEST_BROKER_HPP

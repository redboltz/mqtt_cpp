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

#include "test_settings.hpp"
#include "visitor_util.hpp"


namespace mi = boost::multi_index;
namespace as = boost::asio;

/**
 * Allow the broker to support incoming connections of
 * any supported type of mqtt::server.
 */
using con_sp_t = mqtt::variant<
    std::shared_ptr<mqtt::server<>::endpoint_t>
#if !defined(MQTT_NO_TLS)
    ,
    std::shared_ptr<mqtt::server_tls<>::endpoint_t>
#endif // !defined(MQTT_NO_TLS)
#if defined(MQTT_USE_WS)
    ,
    std::shared_ptr<mqtt::server_ws<>::endpoint_t>
#if !defined(MQTT_NO_TLS)
    ,
    std::shared_ptr<mqtt::server_tls_ws<>::endpoint_t>
#endif // !defined(MQTT_NO_TLS)
#endif // defined(MQTT_USE_WS)
>;

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
        ep.socket()->lowest_layer().set_option(as::ip::tcp::no_delay(true));
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
            (std::string const& client_id,
             mqtt::optional<std::string> const& /*username*/,
             mqtt::optional<std::string> const& /*password*/,
             mqtt::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t /*keep_alive*/) {
                // If it's a not a clean session, but no client id
                // is provided, we would have no way to map this
                // connection's session to a new connection later.
                // so the connection must be rejected.
                if (client_id.empty() && !clean_session) {
                    ep.connack(false, mqtt::connect_return_code::identifier_rejected);
                    return false;
                }
                auto spep = ep.shared_from_this();
                auto emplace =
                    [&] {
                        return cons_.emplace(
                            client_id,
                            spep,
                            [this, spep] () {
                                // send will no keep
                                spep->force_disconnect();
                                close_proc(*spep, true);
                            }
                        );
                    };

                auto it_ret = emplace();
                // if the connection that has the same client_id exists, then overwrite it
                auto const& it = std::get<0>(it_ret);
                auto const& emplaced = std::get<1>(it_ret);
                if (!emplaced) {
                    it->before_overwrite();
                    auto it_ret = emplace();
                    // should be emplaced successfully because erased older one just before
                    BOOST_ASSERT(std::get<1>(it_ret));
                }
                connect_proc(clean_session, spep, client_id, std::move(will));
                return true;
            }
        );
        ep.set_disconnect_handler(
            [&]
            (){
                if (delay_disconnect_) {
                    std::weak_ptr<Endpoint> wp(ep.shared_from_this());
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
            });
        ep.set_puback_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_pubrec_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_pubrel_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_pubcomp_handler(
            [&]
            (typename Endpoint::packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_publish_handler(
            [&]
            (std::uint8_t header,
             mqtt::optional<typename Endpoint::packet_id_t> /*packet_id*/,
             std::string topic_name,
             std::string contents){
                std::uint8_t qos = mqtt::publish::get_qos(header);
                bool is_retain = mqtt::publish::is_retain(header);
                do_publish(
                    std::make_shared<std::string>(std::move(topic_name)),
                    std::make_shared<std::string>(std::move(contents)),
                    qos,
                    is_retain);
                return true;
            });
        ep.set_subscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::tuple<std::string, std::uint8_t>> entries) {
                // An in-order list of qos settings, used to send the reply.
                // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
                // allows the server to grant a lower QOS than requested
                // So we reply with the QOS setting that was granted
                // not the one requested.
                std::vector<std::uint8_t> res;
                res.reserve(entries.size());

                for (auto const& e : entries) {
                    std::string const& topic = std::get<0>(e);
                    std::uint8_t qos = std::get<1>(e);
                    res.emplace_back(qos);
                    // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
                    // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
                    subs_.emplace(std::make_shared<std::string>(topic), ep.shared_from_this(), qos);
                }

                // Acknowledge the subscriptions, and the registered QOS settings
                ep.suback(packet_id, std::move(res));
                for (auto const& e : entries) {
                    std::string const& topic = std::get<0>(e);
                    std::uint8_t qos = std::get<1>(e);
                    // Publish any retained messages that match the newly subscribed topic.
                    auto it = retains_.find(topic);
                    if (it != retains_.end()) {
                        ep.publish(
                            as::buffer(*it->topic),
                            as::buffer(*it->contents),
                            [t = it->topic, c = it->contents] {},
                            std::min(it->qos, qos),
                            true);
                    }
                }
                return true;
            }
        );
        ep.set_unsubscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::string> topics) {
                for (auto const& topic : topics) {
                    subs_.erase(topic);
                }
                ep.unsuback(packet_id);
                return true;
            }
        );
        ep.set_pingreq_handler(
            [&] {
                ep.pingresp();
                return true;
            }
        );
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
    void connect_proc(
        bool clean_session,
        con_sp_t const& spep,
        std::string const& client_id,
        mqtt::optional<mqtt::will> will) {
        auto it = sessions_.find(client_id);
        mqtt::visit(
            make_lambda_visitor<void>(
                [&](auto& con) {
                    con->connack(!clean_session && it != sessions_.end(), mqtt::connect_return_code::accepted);
                }
            ),
            spep
        );
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }

        if (clean_session) {
            subsessions_.erase(client_id);
        }
        else {
            // If it's not a clean session, then all of the
            // saved subscriptions are moved to the new session
            // object so they can be used immediately.
            auto r = subsessions_.equal_range(client_id);
            std::vector<session_data> data;
            if (r.first != r.second) {
                data = std::move(r.first->s->data);
            }
            while (r.first != r.second) {
                subs_.emplace(r.first->topic, spep, r.first->qos);
                r.first = subsessions_.erase(r.first);
            }
            // Any, of course, any saved messages need to go out as well.
            for (auto const& d : data) {
                mqtt::visit(
                    make_lambda_visitor<void>(
                        [&](auto& con) {
                            // But *only* for this connection
                            // Not every connection in the broker.
                            con->publish(
                                as::buffer(*d.topic),
                                as::buffer(*d.contents),
                                [t = d.topic, c = d.contents] {},
                                d.qos,
                                true
                            );
                        }
                    ),
                    spep
                );
            }
        }
        // save the last-will-and-testiment of this connection, if there is one.
        if (will) {
            will_.emplace(spep, std::move(will.value()));
        }
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
        bool is_retain) {
        {
            // For each subscription registered for this topic
            auto const& idx = subs_.get<tag_topic>();
            auto r = idx.equal_range(*topic);
            for (; r.first != r.second; ++r.first) {
                mqtt::visit(
                    make_lambda_visitor<void>(
                        [&](auto& con) {
                            // publish the message to subscribers.
                            // TODO: Probably this should be switched to async_publish?
                            //       Given the async_client / sync_client seperation
                            //       and the way they have different function names,
                            //       it wouldn't be possible for test_broker.hpp to be
                            //       used with some hypothetical "async_server" in the future.
                            con->publish(
                                as::buffer(*topic),
                                as::buffer(*contents),
                                [topic, contents] {},
                                std::min(r.first->qos, qos),
                                false
                            );
                        }
                    ),
                    r.first->con
                );
            }
        }
        {
            // For each saved subscription, add this message to
            // the list to be sent out when a connection resumes
            // a lost session.
            auto const& idx = subsessions_.get<tag_topic>();
            auto r = idx.equal_range(*topic);
            for (; r.first != r.second; ++r.first) {
                r.first->s->data.emplace_back(
                    topic,
                    contents,
                    std::min(r.first->qos, qos)
                );
            }
        }
        // If the message is marked as being retained, then we
        // keep it in case a new subscription is added that matches
        // this topic.
        // TODO: Shouldn't an empty contents (regardless of the retain flag)
        //       result in removing the retain message?
        if (is_retain) {
            if (contents->empty()) {
                retains_.erase(*topic);
            }
            else {
                retains_.erase(*topic);
                retains_.emplace(topic, contents, qos);
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
        auto spep = ep.shared_from_this();
        auto& idx = cons_.get<tag_con>();
        auto it = idx.find(spep);
        if (it == idx.end()) return;

        auto cs = ep.clean_session();
        auto client_id = ep.client_id();

        {   // will processing
            auto it = will_.find(spep);
            if (it != will_.end()) {
                if (send_will) {
                    do_publish(
                        std::make_shared<std::string>(std::move(it->will.topic())),
                        std::make_shared<std::string>(std::move(it->will.message())),
                        it->will.qos(),
                        it->will.retain());
                }
                will_.erase(it);
            }
        }

        // Remove the client from the clientid <-> connection map.
        idx.erase(it);

        {
            auto& idx = subs_.get<tag_con>();
            auto r = idx.equal_range(spep);
            if (cs) {
                // Remove all subscriptions for this clientid
                idx.erase(r.first, r.second);
            }
            else {
                // Save all the subscriptions for this clientid for later.
                sessions_.emplace(client_id);
                auto s = std::make_shared<session>(client_id);
                while (r.first != r.second) {
                    subsessions_.emplace(r.first->topic, s, r.first->qos);
                    r.first = idx.erase(r.first);
                }
            }
        }
    }

private:

    struct tag_topic {};
    struct tag_con {};
    struct tag_client_id {};

    // Mapping between clientid and underlying connection.
    // Used as a basic sanity check on whether a particular clientid is currently connected.
    struct cid_con {
        cid_con(std::string cid, con_sp_t con, std::function<void()> before_overwrite)
            :cid(std::move(cid)), con(std::move(con)), before_overwrite(std::move(before_overwrite)) {}
        std::string cid;
        con_sp_t con;
        std::function<void()> before_overwrite;
    };

    using mi_cid_con = mi::multi_index_container<
        cid_con,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(cid_con, std::string, cid)
            >,
            mi::ordered_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(cid_con, con_sp_t, con)
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
        std::string const& get_topic() const {
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
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_con, std::string const&, sub_con::get_topic)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
            >
        >
    >;

    // A collection of topics that have been retained for
    // clients that add a new subscription to the topic.
    struct retain {
        retain(
            std::shared_ptr<std::string> topic,
            std::shared_ptr<std::string> contents,
            std::uint8_t qos)
            :topic(std::move(topic)), contents(std::move(contents)), qos(qos) {}
        std::string const& get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        std::shared_ptr<std::string> contents;
        std::uint8_t qos;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(retain, std::string const&, retain::get_topic)
            >
        >
    >;

    // It's not clear what this does. Is it related to retained messages perhaps?
    struct session_data {
        session_data(
            std::shared_ptr<std::string> topic,
            std::shared_ptr<std::string> contents,
            std::uint8_t qos)
            : topic(std::move(topic)), contents(std::move(contents)), qos(qos) {}
        std::shared_ptr<std::string> topic;
        std::shared_ptr<std::string> contents;
        std::uint8_t qos;
    };
    struct session {
        explicit session(std::string client_id)
            :client_id(std::move(client_id)) {}
        std::string client_id;
        std::vector<session_data> data;
    };
    struct sub_session {
        sub_session(
            std::shared_ptr<std::string> topic,
            std::shared_ptr<session> s,
            std::uint8_t qos)
            :topic(std::move(topic)), s(std::move(s)), qos(qos) {}
        std::string const& get_client_id() const {
            return s->client_id;
        }
        std::string const& get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        std::shared_ptr<session> s;
        std::uint8_t qos;
    };

    // TODO: It'd be nice to allow duplicate clientids and duplicate topics, while still disallowing identical [clientid, topic] pairings
    // TODO: Unless that's allowed by the standard? Is it?
    using mi_sub_session = mi::multi_index_container<
        sub_session,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_session, std::string const&, sub_session::get_client_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_session, std::string const&, sub_session::get_topic)
            >
        >
    >;

    // Mapping between connections and last-will-and-testimates. If there is no will for a connection
    // it won't be found in this container.
    struct con_will {
        con_will(con_sp_t con, mqtt::will will):con(std::move(con)), will(std::move(will)) {}
        con_sp_t con;
        mqtt::will will;
    };
    using mi_con_will = mi::multi_index_container<
        con_will,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(con_will, con_sp_t, con)
            >
        >
    >;

    as::io_service& ios_; ///< The boost asio context to run this broker on.
    as::deadline_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    mqtt::optional<boost::posix_time::time_duration> delay_disconnect_; ///< Used to delay disconnect handling for testing
    mi_cid_con cons_; ///< Map of client id and connections
    mi_sub_con subs_; ///< Map of subscriptions to client ids
    std::set<std::string> sessions_; ///< A list of clientIDs ? TODO: It's not clear how this is different than cons_, 2 lines above.
    mi_sub_session subsessions_; ///< TODO: It's not clear what this is for.
    mi_retain retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.
    mi_con_will will_; ///< Map of last-wills and their associated connection objects.
};

#endif // MQTT_TEST_BROKER_HPP

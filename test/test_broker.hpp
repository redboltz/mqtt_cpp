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
                close_proc(std::forward<Endpoint>(ep), true);
            });
        ep.set_error_handler(
            [&]
            (boost::system::error_code const& /*ec*/){
                close_proc(std::forward<Endpoint>(ep), true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [&]
            (std::string const& client_id,
             mqtt::optional<std::string> const& username,
             mqtt::optional<std::string> const& password,
             mqtt::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                return
                    connect_handler(
                        std::forward<Endpoint>(ep),
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
            (std::string const& client_id,
             mqtt::optional<std::string> const& username,
             mqtt::optional<std::string> const& password,
             mqtt::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t keep_alive,
             std::vector<mqtt::v5::property_variant> props) {
                return
                    connect_handler(
                        std::forward<Endpoint>(ep),
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
                    disconnect_handler(std::forward<Endpoint>(ep));
            }
        );
        ep.set_v5_disconnect_handler(
            [&]
            (std::uint8_t /*reason_code*/, std::vector<mqtt::v5::property_variant> props) {
                if (h_disconnect_props_) h_disconnect_props_(std::move(props));
                return
                    disconnect_handler(std::forward<Endpoint>(ep));
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
             std::string topic_name,
             std::string contents){
                return publish_handler(
                    std::forward<Endpoint>(ep),
                    header,
                    packet_id,
                    std::move(topic_name),
                    std::move(contents),
                    {}
                );
            });
        ep.set_v5_publish_handler(
            [&]
            (std::uint8_t header,
             mqtt::optional<typename Endpoint::packet_id_t> packet_id,
             std::string topic_name,
             std::string contents,
             std::vector<mqtt::v5::property_variant> props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                return publish_handler(
                    std::forward<Endpoint>(ep),
                    header,
                    packet_id,
                    std::move(topic_name),
                    std::move(contents),
                    std::move(props)
                );
            });
        ep.set_subscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::tuple<std::string, std::uint8_t>> entries) {
                return subscribe_handler(
                    std::forward<Endpoint>(ep),
                    packet_id,
                    std::move(entries),
                    {}
                );
            }
        );
        ep.set_v5_subscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::tuple<std::string, std::uint8_t>> entries,
             std::vector<mqtt::v5::property_variant> props
            ) {
                return subscribe_handler(
                    std::forward<Endpoint>(ep),
                    packet_id,
                    std::move(entries),
                    std::move(props)
                );
            }
        );
        ep.set_unsubscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::string> topics) {
                return unsubscribe_handler(
                    std::forward<Endpoint>(ep),
                    packet_id,
                    std::move(topics),
                    {}
                );
            }
        );
        ep.set_v5_unsubscribe_handler(
            [&]
            (typename Endpoint::packet_id_t packet_id,
             std::vector<std::string> topics,
             std::vector<mqtt::v5::property_variant> props
            ) {
                return unsubscribe_handler(
                    std::forward<Endpoint>(ep),
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
    template <typename Endpoint>
    bool connect_handler(
        Endpoint&& ep,
        std::string const& client_id,
        mqtt::optional<std::string> const& /*username*/,
        mqtt::optional<std::string> const& /*password*/,
        mqtt::optional<mqtt::will> will,
        bool clean_session,
        std::uint16_t /*keep_alive*/,
        std::vector<mqtt::v5::property_variant> props
    ) {
        if (ep.get_protocol_version() == mqtt::protocol_version::v5 && h_connect_props_) h_connect_props_(props);
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
        connect_proc(std::forward<Endpoint>(ep), clean_session, spep, client_id, std::move(will));
        return true;
    }

    template <typename Endpoint>
    void disconnect_handler(
        Endpoint&& ep
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
        Endpoint&& ep,
        std::uint8_t header,
        mqtt::optional<typename Endpoint::packet_id_t> packet_id,
        std::string topic_name,
        std::string contents,
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
        Endpoint&& ep,
        typename Endpoint::packet_id_t packet_id,
        std::vector<std::tuple<std::string, std::uint8_t>> entries,
        std::vector<mqtt::v5::property_variant> props) {

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
            std::string const& topic = std::get<0>(e);
            std::uint8_t qos = std::get<1>(e);
            // Publish any retained messages that match the newly subscribed topic.
            auto it = retains_.find(topic);
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
        Endpoint&& ep,
        typename Endpoint::packet_id_t packet_id,
        std::vector<std::string> topics,
        std::vector<mqtt::v5::property_variant> props) {

        for (auto const& topic : topics) {
            subs_.erase(topic);
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
    void connect_proc(
        Endpoint&& ep,
        bool clean_session,
        con_sp_t const& spep,
        std::string const& client_id,
        mqtt::optional<mqtt::will> will) {
        auto it = sessions_.find(client_id);
        mqtt::visit(
            mqtt::make_lambda_visitor<void>(
                [&](auto& con) {
                    switch (ep.get_protocol_version()) {
                    case mqtt::protocol_version::v3_1_1:
                        con->connack(
                            !clean_session && it != sessions_.end(),
                            mqtt::connect_return_code::accepted
                        );
                        break;
                    case mqtt::protocol_version::v5:
                        con->connack(
                            !clean_session && it != sessions_.end(),
                            mqtt::v5::reason_code::success,
                            connack_props_
                        );
                        break;
                    default:
                        BOOST_ASSERT(false);
                        break;
                    }
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
                    mqtt::make_lambda_visitor<void>(
                        [&](auto& con) {
                            // But *only* for this connection
                            // Not every connection in the broker.
                            con->publish(
                                as::buffer(*d.topic),
                                as::buffer(*d.contents),
                                std::make_pair(d.topic, d.contents),
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
        bool is_retain,
        std::vector<mqtt::v5::property_variant> props) {
        {
            // For each subscription registered for this topic
            auto const& idx = subs_.get<tag_topic>();
            auto r = idx.equal_range(*topic);
            for (; r.first != r.second; ++r.first) {
                mqtt::visit(
                    mqtt::make_lambda_visitor<void>(
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
                                std::make_pair(topic, contents),
                                std::min(r.first->qos, qos),
                                false,
                                props
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
            }
            else {
                auto it = retains_.find(*topic);
                if(it == retains_.end()) {
                    retains_.emplace_hint(it, topic, contents, std::move(props), qos);
                }
                else {
                    retains_.modify(it,
                                    [&](retain& val)
                                    {
                                        val.qos = qos;
                                        val.props = std::move(props);
                                        val.contents = contents;
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
    void close_proc(Endpoint&& ep, bool send_will) {
        auto spep = ep.shared_from_this();
        auto& idx = cons_.get<tag_con>();
        auto it = idx.find(spep);
        if (it == idx.end()) return;

        auto cs = ep.clean_session();
        auto client_id = it->cid;

        {   // will processing
            auto it = will_.find(spep);
            if (it != will_.end()) {
                if (send_will) {
                    do_publish(
                        std::make_shared<std::string>(std::move(it->will.topic())),
                        std::make_shared<std::string>(std::move(it->will.message())),
                        it->will.qos(),
                        it->will.retain(),
                        it->will.props());
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
            std::vector<mqtt::v5::property_variant> props,
            std::uint8_t qos)
            :topic(std::move(topic)), contents(std::move(contents)), props(std::move(props)), qos(qos) {}
        std::string const& get_topic() const {
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

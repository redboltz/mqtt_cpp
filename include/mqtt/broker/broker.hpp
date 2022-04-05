// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_BROKER_HPP)
#define MQTT_BROKER_BROKER_HPP

#include <mqtt/config.hpp>

#include <set>

#include <boost/lexical_cast.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property.hpp>
#include <mqtt/visitor_util.hpp>

#include <mqtt/broker/session_state.hpp>
#include <mqtt/broker/sub_con_map.hpp>
#include <mqtt/broker/retained_messages.hpp>

#include <mqtt/broker/retained_topic_map.hpp>
#include <mqtt/broker/shared_target_impl.hpp>
#include <mqtt/broker/mutex.hpp>
#include <mqtt/broker/uuid.hpp>

#include <mqtt/broker/constant.hpp>
#include <mqtt/broker/security.hpp>

MQTT_BROKER_NS_BEGIN

namespace mi = boost::multi_index;
namespace as = boost::asio;

class broker_t {
public:
    broker_t(as::io_context& timer_ioc)
        :timer_ioc_(timer_ioc),
         tim_disconnect_(timer_ioc_) {
        security.default_config();
    }

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
        delay_disconnect_ = force_move(delay);
    }

    /**
     * @brief set pingresp send operaton
     *
     * @param b - if true, send pingresp when pingreq is received.
     *            if false, doesn't send pingresp for test.
     */
    void set_pingresp(bool b) {
        pingresp_ = b;
    }

    /**
     * @brief set pingresp send operaton
     *
     * @param b - if true, send connack when connect is received.
     *            if false, doesn't send connack for test.
     */
    void set_connack(bool b) {
        connack_ = b;
    }

    /**
     * @brief configure the security settings
     */
    void set_security(broker::security&& security) {
        this->security = force_move(security);
    }

    // [end] for test setting

    /**
     * @brief handle_accept
     *
     * Call this function when an server (of whatever kind) has accepted a raw
     * connection from an MQTT client. By 'raw connection', this might be raw TCP sockets
     * or websockets, or completed a TLS handshake, or any other underlying transport
     * type, but what is not meant is that the mqtt client on the other end of the endpoint
     * has initiated the MQTT application protocol connection sequence with CONNECT or CONACK
     * messages being sent or received.
     *
     * This function will assign several event handlers into server (of whatever kind)
     * that is provided as a parameter. This includes connection handlers, disconnection handlers
     * and various handlers for a variety of of MQTT message types.
     *
     * @param ep - The server (of whichever kind) to accept a connection on.
     */
    void handle_accept(con_sp_t spep) {
        con_wp_t wp(spep);
        endpoint_t& ep = *spep;

        ep.socket().lowest_layer().set_option(as::ip::tcp::no_delay(true));
        ep.set_auto_pub_response(false);
        ep.set_async_operation(true);
        ep.set_topic_alias_maximum(MQTT_NS::topic_alias_max);

        // set connection (lower than MQTT) level handlers
        ep.set_close_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                close_proc(force_move(sp), true);
            });
        ep.set_error_handler(
            [this, wp]
            (error_code ec){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto ver = sp->get_protocol_version();
                MQTT_LOG("mqtt_broker", info)
                    << MQTT_ADD_VALUE(address, this)
                    << " error_handler is called. ec:" << ec.message() << " protocol_version:" << ver;

                auto send_response =
                    [&](auto ec) {
                        if (sp->connected()) {
                            auto rc =
                                [&] () -> MQTT_NS::optional<v5::disconnect_reason_code> {
                                    if (ec == boost::system::errc::protocol_error) {
                                        return MQTT_NS::v5::disconnect_reason_code::protocol_error;
                                    }
                                    else if (ec == boost::system::errc::bad_message) {
                                        return MQTT_NS::v5::disconnect_reason_code::malformed_packet;
                                    }
                                    return MQTT_NS::nullopt;
                                }();
                            if (rc) {
                                MQTT_LOG("mqtt_broker", trace)
                                    << MQTT_ADD_VALUE(address, this)
                                    << "send DISCONNECT reason_code:" << rc.value();
                                sp->async_disconnect(
                                    rc.value(),
                                    v5::properties{},
                                    [sp]
                                    (error_code ec) {
                                        if (ec) {
                                            MQTT_LOG("mqtt_broker", info)
                                                << MQTT_ADD_VALUE(address, sp.get())
                                                << ec.message();
                                        }
                                    }
                                );
                            }
                        }
                        else if (sp->underlying_connected()){
                            // underlying layer connected, mqtt connecting
                            // and protocol_version has already been determind as v5
                            auto rc =
                                [&] () -> MQTT_NS::optional<v5::connect_reason_code> {
                                    if (ec ==boost::system::errc::protocol_error) {
                                        return MQTT_NS::v5::connect_reason_code::protocol_error;
                                    }
                                    else if (ec == boost::system::errc::bad_message) {
                                        return MQTT_NS::v5::connect_reason_code::malformed_packet;
                                    }
                                    return MQTT_NS::nullopt;
                                }();
                            if (rc) {
                                MQTT_LOG("mqtt_broker", trace)
                                    << MQTT_ADD_VALUE(address, this)
                                    << "send CONNACK reason_code:" << rc.value();
                                if (connack_) sp->async_connack(
                                    false,
                                    rc.value(),
                                    [sp]
                                    (error_code ec) {
                                        if (ec) {
                                            MQTT_LOG("mqtt_broker", info)
                                                << MQTT_ADD_VALUE(address, sp.get())
                                                << ec.message();
                                        }
                                    }
                                );
                            }
                        }
                    };

                switch (ver) {
                case MQTT_NS::protocol_version::v5:
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#S4_13_Errors

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205
                    //
                    // The DISCONNECT packet is the final MQTT Control Packet sent from the Client or
                    // the Server.
                    send_response(ec);
                    break;
                case MQTT_NS::protocol_version::v3_1_1:
                    // DISCONNECT can't be sent by broker on v3.1.1
                    //
                    // http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718090
                    //
                    // The DISCONNECT Packet is the final Control Packet sent from the Client to the Server.
                    // It indicates that the Client is disconnecting cleanly.
                    //
                    // At the MQTT connecting, there is no appropriate Connect Return Code on v3.1.1
                    // http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718035
                    break;
                default:
                    // The protocol_version is in the CONNECT packet.
                    // Protocol error could happen before the protocol_version is parsed.
                    break;
                }
                close_proc(force_move(sp), true);
            }
        );

        // set MQTT level handlers
        ep.set_connect_handler(
            [this, wp]
            (buffer client_id,
             optional<buffer> username,
             optional<buffer> password,
             optional<will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return connect_handler(
                        force_move(sp),
                        force_move(client_id),
                        force_move(username),
                        force_move(password),
                        force_move(will),
                        clean_session,
                        keep_alive,
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_connect_handler(
            [this, wp]
            (buffer client_id,
             optional<buffer> username,
             optional<buffer> password,
             optional<will> will,
             bool clean_start,
             std::uint16_t keep_alive,
             v5::properties props) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return connect_handler(
                        force_move(sp),
                        force_move(client_id),
                        force_move(username),
                        force_move(password),
                        force_move(will),
                        clean_start,
                        keep_alive,
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_disconnect_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    disconnect_handler(force_move(sp));
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                }
            }
        );
        ep.set_v5_disconnect_handler(
            [this, wp]
            (v5::disconnect_reason_code /*reason_code*/, v5::properties props) {
                if (h_disconnect_props_) h_disconnect_props_(force_move(props));
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    disconnect_handler(force_move(sp));
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                }
            }
        );
        ep.set_puback_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return puback_handler(
                        force_move(sp),
                        packet_id,
                        v5::puback_reason_code::success,
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_puback_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::puback_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return puback_handler(
                        force_move(sp),
                        packet_id,
                        reason_code,
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubrec_handler(
                        force_move(sp),
                        packet_id,
                        v5::pubrec_reason_code::success,
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubrec_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubrec_handler(
                        force_move(sp),
                        packet_id,
                        reason_code,
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubrel_handler(
                        force_move(sp),
                        packet_id,
                        v5::pubrel_reason_code::success,
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubrel_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubrel_handler(
                        force_move(sp),
                        packet_id,
                        reason_code,
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_pubcomp_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubcomp_handler(
                        force_move(sp),
                        packet_id,
                        v5::pubcomp_reason_code::success,
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_pubcomp_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubcomp_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return pubcomp_handler(
                        force_move(sp),
                        packet_id,
                        reason_code,
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_publish_handler(
            [this, wp]
            (optional<packet_id_t> packet_id,
             publish_options pubopts,
             buffer topic_name,
             buffer contents){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return publish_handler(
                        force_move(sp),
                        packet_id,
                        pubopts,
                        force_move(topic_name),
                        force_move(contents),
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_publish_handler(
            [this, wp]
            (optional<packet_id_t> packet_id,
             publish_options pubopts,
             buffer topic_name,
             buffer contents,
             v5::properties props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return publish_handler(
                        force_move(sp),
                        packet_id,
                        pubopts,
                        force_move(topic_name),
                        force_move(contents),
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<subscribe_entry> entries) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return subscribe_handler(
                        force_move(sp),
                        packet_id,
                        force_move(entries),
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<subscribe_entry> entries,
             v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return subscribe_handler(
                        force_move(sp),
                        packet_id,
                        force_move(entries),
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<unsubscribe_entry> entries) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return unsubscribe_handler(
                        force_move(sp),
                        packet_id,
                        force_move(entries),
                        v5::properties{}
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_v5_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<unsubscribe_entry> entries,
             v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                auto p = sp.get();
                try {
                    return unsubscribe_handler(
                        force_move(sp),
                        packet_id,
                        force_move(entries),
                        force_move(props)
                    );
                }
                catch (std::exception const& ex) {
                    MQTT_LOG("mqtt_broker", error)
                        << MQTT_ADD_VALUE(address, p)
                        << ex.what();
                    return true;
                }
            }
        );
        ep.set_pingreq_handler(
            [this, wp] {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                if (pingresp_) {
                    auto p = sp.get();
                    p->async_pingresp(
                        [sp = force_move(sp)]
                        (error_code ec) {
                            if (ec) {
                                MQTT_LOG("mqtt_broker", info)
                                    << MQTT_ADD_VALUE(address, sp.get())
                                    << ec.message();
                            }
                        }
                    );
                }
                return true;
            }
        );
        ep.set_v5_auth_handler(
            [this]
            (v5::auth_reason_code /*reason_code*/,
             v5::properties props
            ) {
                if (h_auth_props_) h_auth_props_(force_move(props));
                return true;
            }
        );

        // Pass spep to keep lifetime.
        // It makes sure wp.lock() never return nullptr in the handlers below
        // including close_handler and error_handler.
        ep.start_session(spep);
    }

    void set_connack_props(v5::properties props) {
        connack_props_ = force_move(props);
    }

    void set_suback_props(v5::properties props) {
        suback_props_ = force_move(props);
    }

    void set_unsuback_props(v5::properties props) {
        unsuback_props_ = force_move(props);
    }

    void set_puback_props(v5::properties props) {
        puback_props_ = force_move(props);
    }

    void set_pubrec_props(v5::properties props) {
        pubrec_props_ = force_move(props);
    }

    void set_pubrel_props(v5::properties props) {
        pubrel_props_ = force_move(props);
    }

    void set_pubcomp_props(v5::properties props) {
        pubcomp_props_ = force_move(props);
    }

    void set_connect_props_handler(std::function<void(v5::properties const&)> h) {
        h_connect_props_ = force_move(h);
    }

    void set_disconnect_props_handler(std::function<void(v5::properties const&)> h) {
        h_disconnect_props_ = force_move(h);
    }

    void set_publish_props_handler(std::function<void(v5::properties const&)> h) {
        h_publish_props_ = force_move(h);
    }

    void set_puback_props_handler(std::function<void(v5::properties const&)> h) {
        h_puback_props_ = force_move(h);
    }

    void set_pubrec_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubrec_props_ = force_move(h);
    }

    void set_pubrel_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubrel_props_ = force_move(h);
    }

    void set_pubcomp_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubcomp_props_ = force_move(h);
    }

    void set_subscribe_props_handler(std::function<void(v5::properties const&)> h) {
        h_subscribe_props_ = force_move(h);
    }

    void set_unsubscribe_props_handler(std::function<void(v5::properties const&)> h) {
        h_unsubscribe_props_ = force_move(h);
    }

    void set_auth_props_handler(std::function<void(v5::properties const&)> h) {
        h_auth_props_ = force_move(h);
    }

    void clear_all_sessions() {
        std::lock_guard<mutex> g(mtx_sessions_);
        sessions_.clear();
    }

    void clear_all_retained_topics() {
        std::lock_guard<mutex> g(mtx_retains_);
        retains_.clear();
    }

private:
    static void force_disconnect(con_sp_t spep) {
        auto p = spep.get();
        p->async_force_disconnect(
            [spep = force_move(spep)]
            (error_code ec) {
                if (ec) {
                    MQTT_LOG("mqtt_broker", info)
                        << MQTT_ADD_VALUE(address, spep.get())
                        << ec.message();
                }
            }
        );
    };

    static void disconnect_and_force_disconnect(con_sp_t spep, v5::disconnect_reason_code rc) {
        auto p = spep.get();
        p->async_disconnect(
            rc,
            v5::properties{},
            [spep = force_move(spep)]
            (error_code) mutable {
                force_disconnect(force_move(spep));
            }
        );
    };

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
     * @param clean_start - if the clean-start flag is set on the CONNECT message.
     * @param spep - varient of shared pointers to underlying connection type.
     * @param req_client_id - the id that the client wants to use (username will be prepended)
     * @param will - the last-will-and-testiment of the connection, if any.
     */
    bool connect_handler(
        con_sp_t spep,
        buffer client_id,
        optional<buffer> noauth_username,
        optional<buffer> password,
        optional<will> will,
        bool clean_start,
        std::uint16_t /*keep_alive*/,
        v5::properties props
    ) {
        auto& ep = *spep;

        optional<std::string> username;
        if (ep.get_preauthed_user_name()) {
            if (security.login_cert(ep.get_preauthed_user_name().value())) {
                username = ep.get_preauthed_user_name();
            }
        }
        else if (!noauth_username && !password) {
            username = security.login_anonymous();
        }
        else if (noauth_username && password) {
            username = security.login(*noauth_username, *password);
        }

        // If login fails, try the unauthenticated user
        if (!username) username = security.login_unauthenticated();

        v5::properties connack_props;
        connect_param cp = handle_connect_props(ep, props, will);

        if (!username) {
            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "User failed to login: "
                << (noauth_username ? std::string(*noauth_username) : std::string("anonymous user"));

            send_connack(
                ep,
                false, // session present
                false, // authenticated
                force_move(connack_props),
                [spep](error_code) {
                    disconnect_and_force_disconnect(spep, v5::disconnect_reason_code::not_authorized);
                }
            );

            return true;
        }

        if (client_id.empty()) {
            if (!handle_empty_client_id(spep, client_id, clean_start, connack_props)) {
                return false;
            }
            // A new client id was generated
            client_id = buffer(string_view(spep->get_client_id()));
        }

        MQTT_LOG("mqtt_broker", trace)
            << MQTT_ADD_VALUE(address, this)
            << "User logged in as: '" << *username << "', client_id: " << client_id;

        /**
         * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Toc514345311
         * 3.1.2.4 Clean Start
         * If a CONNECT packet is received with Clean Start is set to 1, the Client and Server MUST
         * discard any existing Session and start a new Session [MQTT-3.1.2-4]. Consequently,
         *  the Session Present flag in CONNACK is always set to 0 if Clean Start is set to 1.
         */

        // Find any sessions that have the same client_id
        std::lock_guard<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_cid>();
        auto it = idx.lower_bound(std::make_tuple(*username, client_id));
        if (it == idx.end() ||
            it->client_id() != client_id ||
            it->get_username() != *username
        ) {
            // new connection
            MQTT_LOG("mqtt_broker", trace)
                << MQTT_ADD_VALUE(address, this)
                << "cid:" << client_id
                << " new connection inserted.";
            it = idx.emplace_hint(
                it,
                timer_ioc_,
                mtx_subs_map_,
                subs_map_,
                shared_targets_,
                spep,
                client_id,
                *username,
                force_move(will),
                // will_sender
                [this](auto&&... params) {
                    do_publish(std::forward<decltype(params)>(params)...);
                },
                force_move(cp.will_expiry_interval),
                force_move(cp.session_expiry_interval)
            );
            if (cp.response_topic_requested) {
                // set_response_topic never modify key part
                set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
            }
            send_connack(
                ep,
                false, // session present
                true,  // authenticated
                force_move(connack_props)
            );
        }
        else if (it->online()) {
            // online overwrite
            if (close_proc_no_lock(it->con(), true, v5::disconnect_reason_code::session_taken_over)) {
                // remain offline
                if (clean_start) {
                    // discard offline session
                    MQTT_LOG("mqtt_broker", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "cid:" << client_id
                        << "online connection exists, discard old one due to new one's clean_start and renew";
                    if (cp.response_topic_requested) {
                        // set_response_topic never modify key part
                        set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
                    }
                    send_connack(
                        ep,
                        false, // session present
                        true,  // authenticated
                        force_move(connack_props)
                    );
                    idx.modify(
                        it,
                        [&](auto& e) {
                            e.clean();
                            e.update_will(timer_ioc_, force_move(will), cp.will_expiry_interval);
                            e.set_username(*username);
                            // renew_session_expiry updates index
                            e.renew_session_expiry(force_move(cp.session_expiry_interval));
                        },
                        [](auto&) { BOOST_ASSERT(false); }
                    );
                }
                else {
                    // inherit online session if previous session's session exists
                    MQTT_LOG("mqtt_broker", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "cid:" << client_id
                        << "online connection exists, inherit old one and renew";
                    if (cp.response_topic_requested) {
                        // set_response_topic never modify key part
                        set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
                    }
                    send_connack(
                        ep,
                        true, // session present
                        true, // authenticated
                        force_move(connack_props),
                        [
                            this,
                            &idx,
                            it,
                            will = force_move(will),
                            clean_start,
                            spep,
                            will_expiry_interval = cp.will_expiry_interval,
                            session_expiry_interval = cp.session_expiry_interval,
                            username
                        ]
                        (error_code ec) mutable {
                            if (ec) {
                                MQTT_LOG("mqtt_broker", trace)
                                    << MQTT_ADD_VALUE(address, this)
                                    << ec.message();
                                return;
                            }
                            idx.modify(
                                it,
                                [&](auto& e) {
                                    e.renew(spep, clean_start);
                                    e.set_username(*username);
                                    e.update_will(timer_ioc_, force_move(will), will_expiry_interval);
                                    // renew_session_expiry updates index
                                    e.renew_session_expiry(force_move(session_expiry_interval));
                                    e.send_inflight_messages();
                                    e.send_all_offline_messages();
                                },
                                [](auto&) { BOOST_ASSERT(false); }
                            );
                        }
                    );
                }
            }
            else {
                // new connection
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "cid:" << client_id
                    << "online connection exists, discard old one due to session_expiry and renew";
                bool inserted;
                std::tie(it, inserted) = idx.emplace(
                    timer_ioc_,
                    mtx_subs_map_,
                    subs_map_,
                    shared_targets_,
                    spep,
                    client_id,
                    *username,
                    force_move(will),
                    // will_sender
                    [this](auto&&... params) {
                        do_publish(std::forward<decltype(params)>(params)...);
                    },
                    force_move(cp.will_expiry_interval),
                    force_move(cp.session_expiry_interval)
                );
                BOOST_ASSERT(inserted);
                if (cp.response_topic_requested) {
                    // set_response_topic never modify key part
                    set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
                }
                send_connack(
                    ep,
                    false, // session present
                    true,  // authenticated
                    force_move(connack_props)
                );
            }
        }
        else {
            // offline -> online
            if (clean_start) {
                // discard offline session
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "cid:" << client_id
                    << "offline connection exists, discard old one due to new one's clean_start and renew";
                if (cp.response_topic_requested) {
                    // set_response_topic never modify key part
                    set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
                }
                send_connack(
                    ep,
                    false, // session present
                    true,  // authenticated
                    force_move(connack_props)
                );
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.clean();
                        e.renew(spep, clean_start);
                        e.update_will(timer_ioc_, force_move(will), cp.will_expiry_interval);
                        e.set_username(*username);
                        // renew_session_expiry updates index
                        e.renew_session_expiry(force_move(cp.session_expiry_interval));
                    },
                    [](auto&) { BOOST_ASSERT(false); }
                );
            }
            else {
                // inherit offline session
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "cid:" << client_id
                    << "offline connection exists, inherit old one and renew";
                if (cp.response_topic_requested) {
                    // set_response_topic never modify key part
                    set_response_topic(const_cast<session_state&>(*it), connack_props, *username);
                }
                send_connack(
                    ep,
                    true, // session present
                    true,  // authenticated
                    force_move(connack_props),
                    [
                        this,
                        &idx,
                        it,
                        will = force_move(will),
                        clean_start,
                        spep,
                        will_expiry_interval = cp.will_expiry_interval,
                        session_expiry_interval = cp.session_expiry_interval,
                        username
                    ]
                    (error_code ec) mutable {
                        if (ec) {
                            MQTT_LOG("mqtt_broker", trace)
                                << MQTT_ADD_VALUE(address, this)
                                << ec.message();
                            return;
                        }
                        idx.modify(
                            it,
                            [&](auto& e) {
                                e.renew(spep, clean_start);
                                e.set_username(*username);
                                e.update_will(timer_ioc_, force_move(will), will_expiry_interval);
                                // renew_session_expiry updates index
                                e.renew_session_expiry(force_move(session_expiry_interval));
                                e.send_inflight_messages();
                                e.send_all_offline_messages();
                            },
                            [](auto&) { BOOST_ASSERT(false); }
                        );
                    }
                );
            }
        }

        return true;
    }

    struct connect_param {
        optional<std::chrono::steady_clock::duration> session_expiry_interval;
        optional<std::chrono::steady_clock::duration> will_expiry_interval;
        bool response_topic_requested = false;
    };

    connect_param handle_connect_props(
        endpoint_t& ep,
        v5::properties const& props,
        optional<will> const& will
    ) {
        connect_param cp;
        if (ep.get_protocol_version() == protocol_version::v5) {
            {
                auto v = get_property<v5::property::session_expiry_interval>(props);
                if (v && v.value().val() != 0) {
                    cp.session_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
                }
            }
            {
                auto v = get_property<v5::property::request_response_information>(props);
                if (v && v.value().val() == 1) {
                    cp.response_topic_requested = true;
                }
            }

            if (will) {
                auto v = get_property<v5::property::message_expiry_interval>(will.value().props());
                if (v) {
                    cp.will_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
                }
            }

            if (h_connect_props_) {
                h_connect_props_(props);
            }
        }
        return cp;
    }

    void send_connack(
        endpoint_t& ep,
        bool session_present,
        bool authenticated,
        v5::properties props,
        std::function<void(error_code)> finish = [](error_code){}
    ) {
        // Reply to the connect message.
        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            if (connack_) ep.async_connack(
                session_present,
                authenticated ? connect_return_code::accepted
                              : connect_return_code::not_authorized,
                [finish = force_move(finish)]
                (error_code ec) {
                    finish(ec);
                }
            );
            break;
        case protocol_version::v5:
            // connack_props_ member varible is for testing
            if (connack_props_.empty()) {
                // props local variable is is for real case
                props.emplace_back(v5::property::topic_alias_maximum{topic_alias_max});
                props.emplace_back(v5::property::receive_maximum{receive_maximum_max});
                if (connack_) ep.async_connack(
                    session_present,
                    authenticated ? v5::connect_reason_code::success
                                  : v5::connect_reason_code::not_authorized,
                    force_move(props),
                    [finish = force_move(finish)]
                    (error_code ec) {
                        finish(ec);
                    }
                );
            }
            else {
                // use connack_props_ for testing
                if (connack_) ep.async_connack(
                    session_present,
                    authenticated ? v5::connect_reason_code::success
                                  : v5::connect_reason_code::not_authorized,
                    connack_props_,
                    [finish = force_move(finish)]
                    (error_code ec) {
                        finish(ec);
                    }
                );
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void remove_rule(std::size_t rule_nr) {
        security.remove_auth(rule_nr);
    }

    void set_response_topic(session_state& s, v5::properties& connack_props, std::string const &username) {
        auto response_topic =
            [&] {
                if (auto rt_opt = s.get_response_topic()) {
                    return rt_opt.value();
                }
                auto rt = create_uuid_string();
                s.set_response_topic(rt);
                return rt;
            } ();

        auto rule_nr = security.add_auth(
            response_topic,
            { "@any" }, MQTT_NS::broker::security::authorization::type::allow,
            { username }, MQTT_NS::broker::security::authorization::type::allow
        );

        s.set_clean_handler(
            [this, response_topic, rule_nr]() {
                std::lock_guard<mutex> g(mtx_retains_);
                retains_.erase(response_topic);
                remove_rule(rule_nr);
            }
        );

        connack_props.emplace_back(
            v5::property::response_topic(
                allocate_buffer(response_topic)
            )
        );
    }

    bool handle_empty_client_id(
        con_sp_t spep,
        buffer const& client_id,
        bool clean_start,
        v5::properties& connack_props
    ) {
        auto& ep = *spep;
        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            if (client_id.empty()) {
                if (clean_start) {
                    ep.set_client_id(create_uuid_string());
                }
                else {
                    // https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349242
                    // If the Client supplies a zero-byte ClientId,
                    // the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
                    // If it's a not a clean session, but no client id is provided,
                    // we would have no way to map this connection's session to a new connection later.
                    // So the connection must be rejected.
                    if (connack_) {
                        ep.async_connack(
                            false,
                            connect_return_code::identifier_rejected,
                            [&ep, spep = force_move(spep)]
                            (error_code ec) mutable {
                                if (ec) {
                                    MQTT_LOG("mqtt_broker", info)
                                        << MQTT_ADD_VALUE(address, spep.get())
                                        << ec.message();
                                }
                                ep.async_force_disconnect(
                                    [spep = force_move(spep)]
                                    (error_code ec) {
                                        if (ec) {
                                            MQTT_LOG("mqtt_broker", info)
                                                << MQTT_ADD_VALUE(address, spep.get())
                                                << ec.message();
                                        }
                                    }
                                );
                            }
                        );
                    }
                    return false;
                }
            }
            break;
        case protocol_version::v5:
            if (client_id.empty()) {
                // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059
                //  A Server MAY allow a Client to supply a ClientID that has a length of zero bytes,
                // however if it does so the Server MUST treat this as a special case and assign a
                // unique ClientID to that Client [MQTT-3.1.3-6]. It MUST then process the
                // CONNECT packet as if the Client had provided that unique ClientID,
                // and MUST return the Assigned Client Identifier in the CONNACK packet [MQTT-3.1.3-7].
                // If the Server rejects the ClientID it MAY respond to the CONNECT packet with a CONNACK
                // using Reason Code 0x85 (Client Identifier not valid) as described in section 4.13
                // Handling errors, and then it MUST close the Network Connection [MQTT-3.1.3-8].
                //
                // mqtt_cpp author's note: On v5.0, no Clean Start restriction is described.
                ep.set_client_id(create_uuid_string());
                connack_props.emplace_back(
                    v5::property::assigned_client_identifier(buffer(string_view(ep.get_client_id())))
                );
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    void disconnect_handler(
        con_sp_t spep
    ) {
        if (delay_disconnect_) {
            tim_disconnect_.expires_after(delay_disconnect_.value());
            tim_disconnect_.wait();
        }
        close_proc(force_move(spep), false);
    }

    /**
     * @brief close_proc_no_lock - clean up a connection that has been closed.
     *
     * @param ep - The underlying server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     * @return true if offline session is remained, otherwise false
     */
    // TODO: Maybe change the name of this function.
    bool close_proc_no_lock(
        con_sp_t spep,
        bool send_will,
        optional<v5::disconnect_reason_code> rc) {
        endpoint_t& ep = *spep;

        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // act_sess_it == act_sess_idx.end() could happen if broker accepts
        // the session from client but the client closes the session  before sending
        // MQTT `CONNECT` message.
        // In this case, do nothing is correct behavior.
        if (it == idx.end()) return false;

        bool session_clear =
            [&] {
                if (ep.get_protocol_version() == protocol_version::v3_1_1) {
                    return ep.clean_session();
                }
                else {
                    BOOST_ASSERT(ep.get_protocol_version() == protocol_version::v5);
                    auto const& sei_opt = it->session_expiry_interval();
                    return !sei_opt || sei_opt.value() == std::chrono::steady_clock::duration::zero();
                }
            } ();

        auto do_send_will =
            [&](session_state& ss) {
                if (send_will) {
                    ss.send_will();
                }
                else {
                    ss.clear_will();
                }
            };

        if (session_clear) {
            // const_cast is appropriate here
            // See https://github.com/boostorg/multi_index/issues/50
            auto& ss = const_cast<session_state&>(*it);
            do_send_will(ss);
            if (rc) {
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, spep.get())
                    << "disconnect_and_force_disconnect(async) cid:" << ss.client_id();
                disconnect_and_force_disconnect(spep, rc.value());
            }
            else {
                MQTT_LOG("mqtt_broker", trace)
                    << MQTT_ADD_VALUE(address, spep.get())
                    << "force_disconnect(async) cid:" << ss.client_id();
                force_disconnect(spep);
            }
            idx.erase(it);
            BOOST_ASSERT(sessions_.get<tag_con>().find(spep) == sessions_.get<tag_con>().end());
            return false;
        }
        else {
            idx.modify(
                it,
                [&](session_state& ss) {
                    do_send_will(ss);
                    if (rc) {
                        MQTT_LOG("mqtt_broker", trace)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << "disconnect_and_force_disconnect(async) cid:" << ss.client_id();
                        disconnect_and_force_disconnect(spep, rc.value());
                    }
                    else {
                        MQTT_LOG("mqtt_broker", trace)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << "force_disconnect(async) cid:" << ss.client_id();
                        force_disconnect(spep);
                    }
                    // become_offline updates index
                    ss.become_offline(
                        [this]
                        (std::shared_ptr<as::steady_timer> const& sp_tim) {
                            sessions_.get<tag_tim>().erase(sp_tim);
                        }
                    );
                },
                [](auto&) { BOOST_ASSERT(false); }
            );
            return true;
        }

    }

    /**
     * @brief close_proc - clean up a connection that has been closed.
     *
     * @param ep - The underlying server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     * @param rc - Reason Code for send pack DISCONNECT
     * @return true if offline session is remained, otherwise false
     */
    // TODO: Maybe change the name of this function.
    bool close_proc(
        con_sp_t spep,
        bool send_will,
        optional<v5::disconnect_reason_code> rc = nullopt
    ) {
        std::lock_guard<mutex> g(mtx_sessions_);
        return close_proc_no_lock(force_move(spep), send_will, rc);
    }

    bool publish_handler(
        con_sp_t spep,
        optional<packet_id_t> packet_id,
        publish_options pubopts,
        buffer topic_name,
        buffer contents,
        v5::properties props) {

        auto& ep = *spep;

        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        auto send_pubres =
            [&] (bool authorized = true) {
                switch (pubopts.get_qos()) {
                case qos::at_least_once:
                    ep.async_puback(
                        packet_id.value(),
                        authorized ? v5::puback_reason_code::success
                                   : v5::puback_reason_code::not_authorized,
                        puback_props_,
                        [spep = force_move(spep)]
                        (error_code ec) {
                            if (ec) {
                                MQTT_LOG("mqtt_broker", info)
                                    << MQTT_ADD_VALUE(address, spep.get())
                                    << ec.message();
                            }
                        }
                    );
                    break;
                case qos::exactly_once: {
                    ep.async_pubrec(
                        packet_id.value(),
                        authorized ? v5::pubrec_reason_code::success
                                   : v5::pubrec_reason_code::not_authorized,
                        pubrec_props_,
                        [spep = force_move(spep)]
                        (error_code ec) {
                            if (ec) {
                                MQTT_LOG("mqtt_broker", info)
                                    << MQTT_ADD_VALUE(address, spep.get())
                                    << ec.message();
                            }
                        }
                    );
                } break;
                default:
                    break;
                }
            };

        // See if this session is authorized to publish this topic
        if (security.auth_pub(topic_name, it->get_username()) != security::authorization::type::allow) {

            // Publish not authorized
            send_pubres(false);
            return true;
        }

        v5::properties forward_props;

        for (auto&& p : props) {
            MQTT_NS::visit(
                make_lambda_visitor(
                    [](v5::property::topic_alias&&) {
                        // TopicAlias is not forwarded
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901113
                        // A receiver MUST NOT carry forward any Topic Alias mappings from
                        // one Network Connection to another [MQTT-3.3.2-7].
                    },
                    [&ep](v5::property::subscription_identifier&& p) {
                        MQTT_LOG("mqtt_broker", warning)
                            << MQTT_ADD_VALUE(address, &ep)
                            << "Subscription Identifier from client not forwarded sid:" << p.val();
                    },
                    [&forward_props](auto&& p) {
                        forward_props.push_back(force_move(p));
                    }
                ),
                force_move(p)
            );
        }

        do_publish(
            *it,
            force_move(topic_name),
            force_move(contents),
            pubopts.get_qos() | pubopts.get_retain(), // remove dup flag
            force_move(forward_props)
        );

        send_pubres();
        return true;
    }

    bool puback_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::puback_reason_code /*reason_code*/,
        v5::properties /*props*/) {
        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& ss = const_cast<session_state&>(*it);
        ss.erase_inflight_message_by_packet_id(packet_id);
        ss.send_offline_messages_by_packet_id_release();

        return true;
    }

    bool pubrec_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubrec_reason_code reason_code,
        v5::properties /*props*/) {
        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& ss = const_cast<session_state&>(*it);
        ss.erase_inflight_message_by_packet_id(packet_id);

        if (is_error(reason_code)) return true;

        auto& ep = *spep;

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.async_pubrel(
                packet_id,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
            break;
        case protocol_version::v5:
            ep.async_pubrel(
                packet_id,
                v5::pubrel_reason_code::success,
                pubrel_props_,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        return true;
    }

    bool pubrel_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubrel_reason_code reason_code,
        v5::properties /*props*/) {
        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        auto& ep = *spep;

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.async_pubcomp(
                packet_id,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
            break;
        case protocol_version::v5:
            ep.async_pubcomp(
                packet_id,
                // pubcomp reason code is the same as pubrel one
                static_cast<v5::pubcomp_reason_code>(reason_code),
                pubcomp_props_,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        return true;
    }

    bool pubcomp_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubcomp_reason_code /*reason_code*/,
        v5::properties /*props*/){
        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& ss = const_cast<session_state&>(*it);
        ss.erase_inflight_message_by_packet_id(packet_id);
        ss.send_offline_messages_by_packet_id_release();

        return true;
    }

    bool subscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<subscribe_entry> entries,
        v5::properties props) {

        auto& ep = *spep;

        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        // The element of sessions_ must have longer lifetime
        // than corresponding subscription.
        // Because the subscription store the reference of the element.
        optional<session_state_ref> ssr_opt;

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& ss = const_cast<session_state&>(*it);
        ssr_opt.emplace(ss);

        BOOST_ASSERT(ssr_opt);
        session_state_ref ssr {ssr_opt.value()};

        auto publish_proc =
            [this, &ssr](retain_t const& r, qos qos_value, optional<std::size_t> sid) {
                auto props = r.props;
                if (sid) {
                    props.push_back(v5::property::subscription_identifier(*sid));
                }
                if (r.tim_message_expiry) {
                    auto d =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            r.tim_message_expiry->expiry() - std::chrono::steady_clock::now()
                        ).count();
                    set_property<v5::property::message_expiry_interval>(
                        props,
                        v5::property::message_expiry_interval(
                            static_cast<uint32_t>(d)
                        )
                    );
                }
                ssr.get().publish(
                    timer_ioc_,
                    r.topic,
                    r.contents,
                    std::min(r.qos_value, qos_value) | MQTT_NS::retain::yes,
                    props
                );
            };

        std::vector<std::function<void()>> retain_deliver;
        retain_deliver.reserve(entries.size());

        // subscription identifier
        optional<std::size_t> sid;

        // An in-order list of qos settings, used to send the reply.
        // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
        // allows the server to grant a lower QOS than requested
        // So we reply with the QOS setting that was granted
        // not the one requested.
        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1: {
            std::vector<suback_return_code> res;
            res.reserve(entries.size());
            for (auto& e : entries) {
                if (security.is_subscribe_authorized(ss.get_username(), e.topic_filter)) {
                    res.emplace_back(qos_to_suback_return_code(e.subopts.get_qos())); // converts to granted_qos_x
                    ssr.get().subscribe(
                        force_move(e.share_name),
                        e.topic_filter,
                        e.subopts,
                        [&] {
                            std::shared_lock<mutex> g(mtx_retains_);
                            retains_.find(
                                e.topic_filter,
                                [&](retain_t const& r) {
                                    retain_deliver.emplace_back(
                                        [&publish_proc, &r, qos_value = e.subopts.get_qos(), sid] {
                                            publish_proc(r, qos_value, sid);
                                        }
                                    );
                                }
                            );
                        }
                    );
                }
                else {
                    // User not authorized to subscribe to topic filter
                    res.emplace_back(suback_return_code::failure);
                }
            }
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.async_suback(
                packet_id,
                force_move(res),
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
        } break;
        case protocol_version::v5: {
            // Get subscription identifier
            auto v = get_property<v5::property::subscription_identifier>(props);
            if (v && v.value().val() != 0) {
                sid.emplace(v.value().val());
            }

            std::vector<v5::suback_reason_code> res;
            res.reserve(entries.size());
            for (auto& e : entries) {
                if (security.is_subscribe_authorized(ss.get_username(), e.topic_filter)) {
                    res.emplace_back(v5::qos_to_suback_reason_code(e.subopts.get_qos())); // converts to granted_qos_x
                    ssr.get().subscribe(
                        force_move(e.share_name),
                        e.topic_filter,
                        e.subopts,
                        [&] {
                            std::shared_lock<mutex> g(mtx_retains_);
                            retains_.find(
                                e.topic_filter,
                                [&](retain_t const& r) {
                                    retain_deliver.emplace_back(
                                        [&publish_proc, &r, qos_value = e.subopts.get_qos(), sid] {
                                            publish_proc(r, qos_value, sid);
                                        }
                                    );
                                }
                            );
                        },
                        sid
                    );
                }
                else {
                    // User not authorized to subscribe to topic filter
                    res.emplace_back(v5::suback_reason_code::not_authorized);
                }
            }
            if (h_subscribe_props_) h_subscribe_props_(props);
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.async_suback(
                packet_id,
                force_move(res),
                suback_props_,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
        } break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        for (auto const& f : retain_deliver) {
            f();
        }
        return true;
    }

    bool unsubscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<unsubscribe_entry> entries,
        v5::properties props) {

        auto& ep = *spep;

        std::shared_lock<mutex> g(mtx_sessions_);
        auto& idx = sessions_.get<tag_con>();
        auto it  = idx.find(spep);

        // broker uses async_* APIs
        // If broker erase a connection, then async_force_disconnect()
        // and/or async_force_disconnect () is called.
        // During async operation, spep is valid but it has already been
        // erased from sessions_
        if (it == idx.end()) return true;

        // The element of sessions_ must have longer lifetime
        // than corresponding subscription.
        // Because the subscription store the reference of the element.
        optional<session_state_ref> ssr_opt;

        // const_cast is appropriate here
        // See https://github.com/boostorg/multi_index/issues/50
        auto& ss = const_cast<session_state&>(*it);
        ssr_opt.emplace(ss);

        BOOST_ASSERT(ssr_opt);
        session_state_ref ssr {ssr_opt.value()};

        // For each subscription that this connection has
        // Compare against the list of topic filters, and remove
        // the subscription if the topic filter is in the list.
        for (auto const& e : entries) {
            ssr.get().unsubscribe(e.share_name, e.topic_filter);
        }

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.async_unsuback(
                packet_id,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
            break;
        case protocol_version::v5:
            if (h_unsubscribe_props_) h_unsubscribe_props_(props);
            ep.async_unsuback(
                packet_id,
                std::vector<v5::unsuback_reason_code>(
                    entries.size(),
                    v5::unsuback_reason_code::success
                ),
                unsuback_props_,
                [spep = force_move(spep)]
                (error_code ec) {
                    if (ec) {
                        MQTT_LOG("mqtt_broker", info)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << ec.message();
                    }
                }
            );
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
     * @param source_ss - soource session_state.
     * @param topic - The topic to publish the message on.
     * @param contents - The contents of the message.
     * @param pubopts - publish options
     * @param props - properties
     */
    void do_publish(
        session_state const& source_ss,
        buffer topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props
    ) {
        // Get auth rights for this topic
        // auth_users prepared once here, and then referred multiple times in subs_map_.modify() for efficiency
        auto auth_users = security.auth_sub(topic);

        // publish the message to subscribers.
        // retain is delivered as the original only if rap_value is rap::retain.
        // On MQTT v3.1.1, rap_value is always rap::dont.
        auto deliver =
            [&] (session_state& ss, subscription& sub, auto const& auth_users) {

                // See if this session is authorized to subscribe this topic
                auto access = security.auth_sub_user(auth_users, ss.get_username());
                if (access != security::authorization::type::allow) return;

                publish_options new_pubopts = std::min(pubopts.get_qos(), sub.subopts.get_qos());
                if (sub.subopts.get_rap() == rap::retain && pubopts.get_retain() == MQTT_NS::retain::yes) {
                    new_pubopts |= MQTT_NS::retain::yes;
                }

                if (sub.sid) {
                    props.push_back(v5::property::subscription_identifier(sub.sid.value()));
                    ss.deliver(
                        timer_ioc_,
                        topic,
                        contents,
                        new_pubopts,
                        props
                    );
                    props.pop_back();
                }
                else {
                    ss.deliver(
                        timer_ioc_,
                        topic,
                        contents,
                        new_pubopts,
                        props
                    );
                }
            };

        //                  share_name   topic_filter
        std::set<std::tuple<string_view, string_view>> sent;

        {
            std::shared_lock<mutex> g{mtx_subs_map_};
            subs_map_.modify(
                topic,
                [&](buffer const& /*key*/, subscription& sub) {
                    if (sub.share_name.empty()) {
                        // Non shared subscriptions

                        // If NL (no local) subscription option is set and
                        // publisher is the same as subscriber, then skip it.
                        if (sub.subopts.get_nl() == nl::yes &&
                            sub.ss.get().client_id() ==  source_ss.client_id()) return;
                        deliver(sub.ss.get(), sub, auth_users);
                    }
                    else {
                        // Shared subscriptions
                        bool inserted;
                        std::tie(std::ignore, inserted) = sent.emplace(sub.share_name, sub.topic_filter);
                        if (inserted) {
                            if (auto ssr_opt = shared_targets_.get_target(sub.share_name, sub.topic_filter)) {
                                deliver(ssr_opt.value().get(), sub, auth_users);
                            }
                        }
                    }
                }
            );
        }

        optional<std::chrono::steady_clock::duration> message_expiry_interval;
        if (source_ss.get_protocol_version() == protocol_version::v5) {
            auto v = get_property<v5::property::message_expiry_interval>(props);
            if (v) {
                message_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
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
                std::lock_guard<mutex> g(mtx_retains_);
                retains_.erase(topic);
            }
            else {
                std::shared_ptr<as::steady_timer> tim_message_expiry;
                if (message_expiry_interval) {
                    tim_message_expiry = std::make_shared<as::steady_timer>(timer_ioc_, message_expiry_interval.value());
                    tim_message_expiry->async_wait(
                        [this, topic = topic, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)]
                        (boost::system::error_code const& ec) {
                            if (auto sp = wp.lock()) {
                                if (!ec) {
                                    retains_.erase(topic);
                                }
                            }
                        }
                    );
                }

                std::lock_guard<mutex> g(mtx_retains_);
                retains_.insert_or_assign(
                    topic,
                    retain_t {
                        force_move(topic),
                        force_move(contents),
                        force_move(props),
                        pubopts.get_qos(),
                        tim_message_expiry
                    }
                );
            }
        }
    }

private:
    as::io_context& timer_ioc_; ///< The boost asio context to run this broker on.
    as::steady_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    optional<std::chrono::steady_clock::duration> delay_disconnect_; ///< Used to delay disconnect handling for testing

    // Authorization and authentication settings
    broker::security security;

    mutable mutex mtx_subs_map_;
    sub_con_map subs_map_;   /// subscription information
    shared_target shared_targets_; /// shared subscription targets

    ///< Map of active client id and connections
    /// session_state has references of subs_map_ and shared_targets_.
    /// because session_state (member of sessions_) has references of subs_map_ and shared_targets_.
    mutable mutex mtx_sessions_;
    session_states sessions_;

    mutable mutex mtx_retains_;
    retained_messages retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.

    // MQTTv5 members
    v5::properties connack_props_;
    v5::properties suback_props_;
    v5::properties unsuback_props_;
    v5::properties puback_props_;
    v5::properties pubrec_props_;
    v5::properties pubrel_props_;
    v5::properties pubcomp_props_;
    std::function<void(v5::properties const&)> h_connect_props_;
    std::function<void(v5::properties const&)> h_disconnect_props_;
    std::function<void(v5::properties const&)> h_publish_props_;
    std::function<void(v5::properties const&)> h_puback_props_;
    std::function<void(v5::properties const&)> h_pubrec_props_;
    std::function<void(v5::properties const&)> h_pubrel_props_;
    std::function<void(v5::properties const&)> h_pubcomp_props_;
    std::function<void(v5::properties const&)> h_subscribe_props_;
    std::function<void(v5::properties const&)> h_unsubscribe_props_;
    std::function<void(v5::properties const&)> h_auth_props_;
    bool pingresp_ = true;
    bool connack_ = true;
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_BROKER_HPP

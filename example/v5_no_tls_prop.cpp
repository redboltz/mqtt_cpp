// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// no_tls client and server

#include <iostream>
#include <iomanip>
#include <map>

#include <mqtt_client_cpp.hpp>

#include <boost/lexical_cast.hpp>

#include "locked_cout.hpp"

template <typename Client>
void client_proc(Client& c) {

    using namespace MQTT_NS; // for ""_mb

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_start(true);

    // Setup handlers
    c->set_v5_connack_handler( // use v5 handler
        [&c]
        (bool sp, v5::connect_reason_code reason_code, MQTT_NS::v5::properties props){
            locked_cout() << "[client] Connack handler called" << std::endl;
            locked_cout() << "[client] Session Present: " << std::boolalpha << sp << std::endl;
            locked_cout() << "[client] Connect Reason Code: " << reason_code << std::endl;

            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                            locked_cout() << "[client] prop: session_expiry_interval: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::receive_maximum const& t) {
                            locked_cout() << "[client] prop: receive_maximum: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::maximum_qos const& t) {
                            locked_cout() << "[client] prop: maximum_qos: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::retain_available const& t) {
                            locked_cout() << "[client] prop: retain_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                            locked_cout() << "[client] prop: maximum_packet_size: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::assigned_client_identifier const& t) {
                            locked_cout() << "[client] prop: assigned_client_identifier_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                            locked_cout() << "[client] prop: topic_alias_maximum: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::reason_string const& t) {
                            locked_cout() << "[client] prop: reason_string_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::user_property const& t) {
                            locked_cout() << "[client] prop: user_property_ref: " << t.key() << ":" << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::wildcard_subscription_available const& t) {
                            locked_cout() << "[client] prop: wildcard_subscription_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::subscription_identifier_available const& t) {
                            locked_cout() << "[client] prop: subscription_identifier_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::shared_subscription_available const& t) {
                            locked_cout() << "[client] prop: shared_subscription_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::server_keep_alive const& t) {
                            locked_cout() << "[client] prop: server_keep_alive: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::response_information const& t) {
                            locked_cout() << "[client] prop: response_information_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::server_reference const& t) {
                            locked_cout() << "[client] prop: server_reference_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::authentication_method const& t) {
                            locked_cout() << "[client] prop: authentication_method_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::authentication_data const& t) {
                            locked_cout() << "[client] prop: authentication_data_ref: " << t.val() << std::endl;
                        },
                        [&](auto&& ...) {
                            BOOST_ASSERT(false);
                        }
                    ),
                    p
                );
            }


            c->disconnect();
            return true;
        });
    c->set_close_handler( // this handler doesn't depend on MQTT protocol version
        []
        (){
            locked_cout() << "[client] closed." << std::endl;
        });
    c->set_error_handler( // this handler doesn't depend on MQTT protocol version
        []
        (MQTT_NS::error_code ec){
            locked_cout() << "[client] error: " << ec.message() << std::endl;
        });

    // prepare connect properties
    MQTT_NS::v5::properties con_ps {
        MQTT_NS::v5::property::session_expiry_interval(0x12345678UL),
        MQTT_NS::v5::property::receive_maximum(0x1234U),
        MQTT_NS::v5::property::maximum_packet_size(0x12345678UL),
        MQTT_NS::v5::property::topic_alias_maximum(0x1234U),
        MQTT_NS::v5::property::request_response_information(true),
        MQTT_NS::v5::property::request_problem_information(false),
        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        MQTT_NS::v5::property::authentication_method("test authentication method"_mb),
        MQTT_NS::v5::property::authentication_data("test authentication data"_mb)
    };

    // Connect with properties
    c->connect(std::move(con_ps));
}

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <mqtt_server_cpp.hpp>

namespace mi = boost::multi_index;

using con_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;


template <typename Server>
void server_proc(Server& s, std::set<con_sp_t>& connections) {

    using namespace MQTT_NS::literals; // for ""_mb

    s.set_error_handler( // this handler doesn't depend on MQTT protocol version
        [](MQTT_NS::error_code ec) {
            locked_cout() << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler( // this handler doesn't depend on MQTT protocol version
        [&s, &connections](con_sp_t spep) {
            auto& ep = *spep;
            std::weak_ptr<con_t> wp(spep);

            locked_cout() << "[server] accept" << std::endl;
            // For server close if ep is closed.
            auto g = MQTT_NS::shared_scope_guard(
                [&s] {
                    locked_cout() << "[server] session end" << std::endl;
                    s.close();
                }
            );
            // Pass spep to keep lifetime.
            // It makes sure wp.lock() never return nullptr in the handlers below
            // including close_handler and error_handler.
            ep.start_session(std::make_tuple(std::move(spep), std::move(g)));

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler( // this handler doesn't depend on MQTT protocol version
                [&connections, wp]
                (){
                    locked_cout() << "[server] closed." << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.erase(sp);
                });
            ep.set_error_handler( // this handler doesn't depend on MQTT protocol version
                [&connections, wp]
                (MQTT_NS::error_code ec){
                    locked_cout() << "[server] error: " << ec.message() << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.erase(sp);
                });

            // set MQTT level handlers
            ep.set_v5_connect_handler( // use v5 handler
                [&connections, wp]
                (MQTT_NS::buffer client_id,
                 MQTT_NS::optional<MQTT_NS::buffer> const& username,
                 MQTT_NS::optional<MQTT_NS::buffer> const& password,
                 MQTT_NS::optional<MQTT_NS::will>,
                 bool clean_start,
                 std::uint16_t keep_alive,
                 MQTT_NS::v5::properties props){
                    locked_cout() << "[server] client_id    : " << client_id << std::endl;
                    locked_cout() << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] clean_start  : " << std::boolalpha << clean_start << std::endl;
                    locked_cout() << "[server] keep_alive   : " << keep_alive << std::endl;

                    // check properties
                    for (auto const& p : props) {
                        MQTT_NS::visit(
                            MQTT_NS::make_lambda_visitor(
                                [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                                    locked_cout() << "[server] prop: session_expiry_interval: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::receive_maximum const& t) {
                                    locked_cout() << "[server] prop: receive_maximum: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                                    locked_cout() << "[server] prop: maximum_packet_size: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                                    locked_cout() << "[server] prop: topic_alias_maximum: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::request_response_information const& t) {
                                    locked_cout() << "[server] prop: request_response_information: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::request_problem_information const& t) {
                                    locked_cout() << "[server] prop: request_problem_information: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::user_property const& t) {
                                    locked_cout() << "[server] prop: user_property_ref: " << t.key() << ":" << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::authentication_method const& t) {
                                    locked_cout() << "[server] prop: authentication_method_ref: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::authentication_data const& t) {
                                    locked_cout() << "[server] prop: authentication_data_ref: " << t.val() << std::endl;
                                },
                                [&](auto&& ...) {
                                    BOOST_ASSERT(false);
                                }
                            ),
                            p
                        );
                    }

                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.insert(sp);

                    MQTT_NS::v5::properties connack_ps {
                        MQTT_NS::v5::property::session_expiry_interval(0),
                        MQTT_NS::v5::property::receive_maximum(0),
                        MQTT_NS::v5::property::maximum_qos(MQTT_NS::qos::exactly_once),
                        MQTT_NS::v5::property::retain_available(true),
                        MQTT_NS::v5::property::maximum_packet_size(0),
                        MQTT_NS::v5::property::assigned_client_identifier("test cid"_mb),
                        MQTT_NS::v5::property::topic_alias_maximum(0),
                        MQTT_NS::v5::property::reason_string("test connect success"_mb),
                        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
                        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
                        MQTT_NS::v5::property::wildcard_subscription_available(false),
                        MQTT_NS::v5::property::subscription_identifier_available(false),
                        MQTT_NS::v5::property::shared_subscription_available(false),
                        MQTT_NS::v5::property::server_keep_alive(0),
                        MQTT_NS::v5::property::response_information("test response information"_mb),
                        MQTT_NS::v5::property::server_reference("test server reference"_mb),
                        MQTT_NS::v5::property::authentication_method("test authentication method"_mb),
                        MQTT_NS::v5::property::authentication_data("test authentication data"_mb)
                    };
                    sp->connack(false, MQTT_NS::v5::connect_reason_code::success, std::move(connack_ps));
                    return true;
                }
            );
            ep.set_v5_disconnect_handler( // use v5 handler
                [&connections, wp]
                (MQTT_NS::v5::disconnect_reason_code reason_code, MQTT_NS::v5::properties /*props*/) {
                    locked_cout() <<
                        "[server] disconnect received." <<
                        " reason_code: " << reason_code << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.erase(sp);
                });
        }
    );

    s.listen();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        locked_cout() << argv[0] << " port" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);

    // server
    boost::asio::io_context iocs;
    auto s = MQTT_NS::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        iocs
    );

    // You can set a specific protocol_version if you want to limit accepting version.
    // Otherwise, all protocols are accepted.
    s.set_protocol_version(MQTT_NS::protocol_version::v5);

    std::set<con_sp_t> connections;
    std::thread th(
        [&] {
            server_proc(s, connections);
            iocs.run();
        }
    );

    // client
    // You can set the protocol_version to connect. If you don't set it, v3_1_1 is used.
    auto c = MQTT_NS::make_sync_client(ioc, "localhost", port, MQTT_NS::protocol_version::v5);

    client_proc(c);

    ioc.run();
    th.join();
}

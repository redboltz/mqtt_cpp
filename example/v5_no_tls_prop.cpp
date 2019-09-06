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

template <typename Client>
void client_proc(Client& c) {

    using namespace MQTT_NS; // for ""_mb

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);

    // Setup handlers
    c->set_v5_connack_handler( // use v5 handler
        [&c]
        (bool sp, v5::connect_reason_code reason_code, std::vector<MQTT_NS::v5::property_variant> props){
            std::cout << "[client] Connack handler called" << std::endl;
            std::cout << "[client] Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "[client] Connect Reason Code: "
                      << static_cast<int>(reason_code) << std::endl;

            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor<void>(
                        [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                            std::cout << "[client] prop: session_expiry_interval: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::receive_maximum const& t) {
                            std::cout << "[client] prop: receive_maximum: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::maximum_qos const& t) {
                            std::cout << "[client] prop: maximum_qos: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::retain_available const& t) {
                            std::cout << "[client] prop: retain_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                            std::cout << "[client] prop: maximum_packet_size: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::assigned_client_identifier const& t) {
                            std::cout << "[client] prop: assigned_client_identifier_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                            std::cout << "[client] prop: topic_alias_maximum: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::reason_string const& t) {
                            std::cout << "[client] prop: reason_string_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::user_property const& t) {
                            std::cout << "[client] prop: user_property_ref: " << t.key() << ":" << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::wildcard_subscription_available const& t) {
                            std::cout << "[client] prop: wildcard_subscription_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::subscription_identifier_available const& t) {
                            std::cout << "[client] prop: subscription_identifier_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::shared_subscription_available const& t) {
                            std::cout << "[client] prop: shared_subscription_available: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::server_keep_alive const& t) {
                            std::cout << "[client] prop: server_keep_alive: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::response_information const& t) {
                            std::cout << "[client] prop: response_information_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::server_reference const& t) {
                            std::cout << "[client] prop: server_reference_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::authentication_method const& t) {
                            std::cout << "[client] prop: authentication_method_ref: " << t.val() << std::endl;
                        },
                        [&](MQTT_NS::v5::property::authentication_data const& t) {
                            std::cout << "[client] prop: authentication_data_ref: " << t.val() << std::endl;
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
            std::cout << "[client] closed." << std::endl;
        });
    c->set_error_handler( // this handler doesn't depend on MQTT protocol version
        []
        (boost::system::error_code const& ec){
            std::cout << "[client] error: " << ec.message() << std::endl;
        });

    // prepare connect properties
    std::vector<MQTT_NS::v5::property_variant> con_ps {
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
        [](boost::system::error_code const& ec) {
            std::cout << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler( // this handler doesn't depend on MQTT protocol version
        [&](con_t& ep) {
            std::cout << "[server] accept" << std::endl;
            auto sp = ep.shared_from_this();
            ep.start_session(
                [&, sp] // keeping ep's lifetime as sp until session finished
                (boost::system::error_code const& ec) {
                    std::cout << "[server] session end: " << ec.message() << std::endl;
                    s.close();
                }
            );

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler( // this handler doesn't depend on MQTT protocol version
                [&]
                (){
                    std::cout << "[server] closed." << std::endl;
                    connections.erase(ep.shared_from_this());
                });
            ep.set_error_handler( // this handler doesn't depend on MQTT protocol version
                [&]
                (boost::system::error_code const& ec){
                    std::cout << "[server] error: " << ec.message() << std::endl;
                    connections.erase(ep.shared_from_this());
                });

            // set MQTT level handlers
            ep.set_v5_connect_handler( // use v5 handler
                [&]
                (MQTT_NS::buffer client_id,
                 MQTT_NS::optional<MQTT_NS::buffer> const& username,
                 MQTT_NS::optional<MQTT_NS::buffer> const& password,
                 MQTT_NS::optional<MQTT_NS::will>,
                 bool clean_session,
                 std::uint16_t keep_alive,
                 std::vector<MQTT_NS::v5::property_variant> props){
                    std::cout << "[server] client_id    : " << client_id << std::endl;
                    std::cout << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    std::cout << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    std::cout << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
                    std::cout << "[server] keep_alive   : " << keep_alive << std::endl;

                    // check properties
                    for (auto const& p : props) {
                        MQTT_NS::visit(
                            MQTT_NS::make_lambda_visitor<void>(
                                [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                                    std::cout << "[server] prop: session_expiry_interval: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::receive_maximum const& t) {
                                    std::cout << "[server] prop: receive_maximum: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                                    std::cout << "[server] prop: maximum_packet_size: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                                    std::cout << "[server] prop: topic_alias_maximum: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::request_response_information const& t) {
                                    std::cout << "[server] prop: request_response_information: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::request_problem_information const& t) {
                                    std::cout << "[server] prop: request_problem_information: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::user_property const& t) {
                                    std::cout << "[server] prop: user_property_ref: " << t.key() << ":" << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::authentication_method const& t) {
                                    std::cout << "[server] prop: authentication_method_ref: " << t.val() << std::endl;
                                },
                                [&](MQTT_NS::v5::property::authentication_data const& t) {
                                    std::cout << "[server] prop: authentication_data_ref: " << t.val() << std::endl;
                                },
                                [&](auto&& ...) {
                                    BOOST_ASSERT(false);
                                }
                            ),
                            p
                        );
                    }

                    connections.insert(ep.shared_from_this());

                    std::vector<MQTT_NS::v5::property_variant> connack_ps {
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
                    ep.connack(false, MQTT_NS::connect_return_code::accepted, std::move(connack_ps));
                    return true;
                }
            );
            ep.set_v5_disconnect_handler( // use v5 handler
                [&]
                (std::uint8_t reason_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    std::cout <<
                        "[server] disconnect received." <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    connections.erase(ep.shared_from_this());
                });
        }
    );

    s.listen();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << " port" << std::endl;
        return -1;
    }

    boost::asio::io_service ios;
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);

    // server
    auto s = MQTT_NS::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        ios
    );

    // You can set a specific protocol_version if you want to limit accepting version.
    // Otherwise, all protocols are accepted.
    s.set_protocol_version(MQTT_NS::protocol_version::v5);

    std::set<con_sp_t> connections;
    server_proc(s, connections);


    // client
    // You can set the protocol_version to connect. If you don't set it, v3_1_1 is used.
    auto c = MQTT_NS::make_sync_client(ios, "localhost", port, MQTT_NS::protocol_version::v5);

    client_proc(c);

    ios.run();
}

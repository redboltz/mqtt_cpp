// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_resend_serialize)

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
restore_serialized_publish_message(Client const& c, Packet const& packet) {
    c->restore_serialized_message(
        mqtt::publish_message(packet.begin(), packet.end()),
        []{}
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
restore_serialized_publish_message(Client const& c, Packet const& packet) {
    c->restore_serialized_message(
        mqtt::publish_32_message(packet.begin(), packet.end()),
        []{}
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
restore_serialized_pubrel_message(Client const& c, Packet const& packet) {
    c->restore_serialized_message(
        mqtt::pubrel_message(packet.begin(), packet.end())
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
restore_serialized_pubrel_message(Client const& c, Packet const& packet) {
    c->restore_serialized_message(
        mqtt::pubrel_32_message(packet.begin(), packet.end())
    );
}


template <typename Client, typename Serialized>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
set_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_serialize_handlers(
        [&serialized](mqtt::publish_message msg) {
            serialized.emplace(msg.packet_id(), std::make_tuple(true, msg.continuous_buffer()));
        },
        [&serialized](mqtt::pubrel_message msg) {
            BOOST_CHECK(serialized.find(msg.packet_id()) != serialized.end());
            serialized[msg.packet_id()] = std::make_tuple(false, msg.continuous_buffer());
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}

template <typename Client, typename Serialized>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
set_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_serialize_handlers(
        [&serialized](mqtt::publish_32_message msg) {
            serialized.emplace(msg.packet_id(), std::make_tuple(true, msg.continuous_buffer()));
        },
        [&serialized](mqtt::pubrel_32_message msg) {
            BOOST_CHECK(serialized.find(msg.packet_id()) != serialized.end());
            serialized[msg.packet_id()] = std::make_tuple(false, msg.continuous_buffer());
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}


BOOST_AUTO_TEST_CASE( publish_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_puback"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_at_least_once("topic1", "topic1_contents");
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_puback_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
            return true;
        });

    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( publish_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
            cont("h_pubrec"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_exactly_once("topic1", "topic1_contents");
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_pubrec_handler(
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c2->set_pubcomp_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pubrel_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        cont("h_pubrec"),
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_exactly_once("topic1", "topic1_contents");
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });
    c1->set_pubrec_handler(
        [&chk, &c1, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            c1->force_disconnect();
            return true;
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_pubcomp_handler(
        [&chk, &c2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == 1);
            c2->disconnect();
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub1;
    std::uint16_t pid_pub2;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error1"),
        // connect
        cont("h_connack3"),
        cont("h_puback1"),
        cont("h_puback2"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub1, &pid_pub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub1 = c1->publish_at_least_once("topic1", "topic1_contents1");
                    pid_pub2 = c1->publish_at_least_once("topic1", "topic1_contents2");
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error1");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });
    c2->set_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_puback_handler(
        [&chk, &c2, &pid_pub1, &pid_pub2]
        (packet_id_t packet_id) {
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_puback1");
                    BOOST_TEST(packet_id == pid_pub1);
                },
                "h_puback1",
                [&] {
                    MQTT_CHK("h_puback2");
                    BOOST_TEST(packet_id == pid_pub2);
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
restore_v5_serialized_publish_message(Client const& c, Packet const& packet) {
    c->restore_v5_serialized_message(
        mqtt::v5::publish_message(packet.begin(), packet.end()),
        boost::any()
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
restore_v5_serialized_publish_message(Client const& c, Packet const& packet) {
    c->restore_v5_serialized_message(
        mqtt::v5::publish_32_message(packet.begin(), packet.end()),
        boost::any()
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
restore_v5_serialized_pubrel_message(Client const& c, Packet const& packet) {
    c->restore_v5_serialized_message(
        mqtt::v5::pubrel_message(packet.begin(), packet.end()),
        boost::any()
    );
}

template <typename Client, typename Packet>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
restore_v5_serialized_pubrel_message(Client const& c, Packet const& packet) {
    c->restore_v5_serialized_message(
        mqtt::v5::pubrel_32_message(packet.begin(), packet.end()),
        boost::any()
    );
}


template <typename Client, typename Serialized>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 2
>::type
set_v5_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_v5_serialize_handlers(
        [&serialized](mqtt::v5::publish_message msg) {
            serialized.emplace(msg.packet_id(), std::make_tuple(true, msg.continuous_buffer()));
        },
        [&serialized](mqtt::v5::pubrel_message msg) {
            BOOST_CHECK(serialized.find(msg.packet_id()) != serialized.end());
            serialized[msg.packet_id()] = std::make_tuple(false, msg.continuous_buffer());
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}

template <typename Client, typename Serialized>
inline
typename std::enable_if<
    sizeof(typename Client::element_type::packet_id_t) == 4
>::type
set_v5_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_v5_serialize_handlers(
        [&serialized](mqtt::v5::publish_32_message msg) {
            serialized.emplace(msg.packet_id(), std::make_tuple(true, msg.continuous_buffer()));
        },
        [&serialized](mqtt::v5::pubrel_32_message msg) {
            BOOST_CHECK(serialized.find(msg.packet_id()) != serialized.end());
            serialized[msg.packet_id()] = std::make_tuple(false, msg.continuous_buffer());
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}

BOOST_AUTO_TEST_CASE( publish_qos1_v5 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_puback"),
        // disconnect
        cont("h_close2"),
    };

    std::vector<mqtt::v5::property_variant> ps {
        mqtt::v5::property::payload_format_indicator(mqtt::v5::property::payload_format_indicator::string),
        mqtt::v5::property::message_expiry_interval(0x12345678UL),
        mqtt::v5::property::topic_alias(0x1234U),
        mqtt::v5::property::response_topic("response topic"),
        mqtt::v5::property::correlation_data("correlation data"),
        mqtt::v5::property::user_property("key1", "val1"),
        mqtt::v5::property::user_property("key2", "val2"),
        mqtt::v5::property::subscription_identifier(123),
    };

    std::size_t user_prop_count = 0;
    b.set_publish_props_handler(
        [&user_prop_count, size = ps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
            BOOST_TEST(props.size() == size);

            for (auto const& p : props) {
                mqtt::visit(
                    mqtt::make_lambda_visitor<void>(
                        [&](mqtt::v5::property::payload_format_indicator::recv const& t) {
                            BOOST_TEST(t.val() == mqtt::v5::property::payload_format_indicator::string);
                        },
                        [&](mqtt::v5::property::message_expiry_interval::recv const& t) {
                            BOOST_TEST(t.val() == 0x12345678UL);
                        },
                        [&](mqtt::v5::property::topic_alias::recv const& t) {
                            BOOST_TEST(t.val() == 0x1234U);
                        },
                        [&](mqtt::v5::property::response_topic::recv const& t) {
                            BOOST_TEST(t.val() == "response topic");
                        },
                        [&](mqtt::v5::property::correlation_data::recv const& t) {
                            BOOST_TEST(t.val() == "correlation data");
                        },
                        [&](mqtt::v5::property::user_property::recv const& t) {
                            switch (user_prop_count++) {
                            case 0:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 1:
                                BOOST_TEST(t.key() == "key2");
                                BOOST_TEST(t.val() == "val2");
                                break;
                            case 2:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 3:
                                BOOST_TEST(t.key() == "key2");
                                BOOST_TEST(t.val() == "val2");
                                break;
                            default:
                                BOOST_TEST(false);
                                break;
                            }
                        },
                        [&](mqtt::v5::property::subscription_identifier const& t) {
                            BOOST_TEST(t.val() == 123U);
                        },
                        [&](auto&& ...) {
                            BOOST_TEST(false);
                        }
                    ),
                    p
                );
            }
        }
    );

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub, ps = std::move(ps)]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&, ps = std::move(ps)] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_at_least_once("topic1", "topic1_contents", false, std::move(ps));
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_v5_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_v5_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_v5_puback_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
            return true;
        });

    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( publish_qos2_v5 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
            cont("h_pubrec"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_exactly_once("topic1", "topic1_contents");
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_v5_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_v5_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_v5_pubrec_handler(
        [&chk, &pid_pub]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c2->set_v5_pubcomp_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pubrel_qos2_v5 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_auto_pub_response(false);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        cont("h_pubrec"),
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    std::vector<mqtt::v5::property_variant> ps {
        mqtt::v5::property::reason_string("test success"),
        mqtt::v5::property::user_property("key1", "val1"),
        mqtt::v5::property::user_property("key2", "val2"),
    };
    std::size_t user_prop_count = 0;

    b.set_pubrel_props_handler(
        [&user_prop_count, size = ps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
            BOOST_TEST(props.size() == size);
            for (auto const& p : props) {
                mqtt::visit(
                    mqtt::make_lambda_visitor<void>(
                        [&](mqtt::v5::property::reason_string::recv const& t) {
                            BOOST_TEST(t.val() == "test success");
                        },
                        [&](mqtt::v5::property::user_property::recv const& t) {
                            switch (user_prop_count++) {
                            case 0:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 1:
                                BOOST_TEST(t.key() == "key2");
                                BOOST_TEST(t.val() == "val2");
                                break;
                            case 2:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 3:
                                BOOST_TEST(t.key() == "key2");
                                BOOST_TEST(t.val() == "val2");
                                break;
                            default:
                                BOOST_TEST(false);
                                break;
                            }
                        },
                        [&](auto&& ...) {
                            BOOST_TEST(false);
                        }
                    ),
                    p
                );
            }
        }
    );

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish_exactly_once("topic1", "topic1_contents");
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_v5_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_v5_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });
    c1->set_v5_pubrec_handler(
        [&chk, &c1, &pid_pub, ps = std::move(ps)]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            c1->pubrel(packet_id, mqtt::v5::reason_code::success, std::move(ps));
            c1->force_disconnect();
            return true;
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_v5_pubcomp_handler(
        [&chk, &c2]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == 1);
            c2->disconnect();
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1_v5 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c2->set_client_id("cid1");
    c2->set_clean_session(false);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<
        packet_id_t,
        std::tuple<
            bool,       // is publish
            std::string // whole packet bytes
        >
    > serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub1;
    std::uint16_t pid_pub2;


    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error1"),
        // connect
        cont("h_connack3"),
        cont("h_puback1"),
        cont("h_puback2"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub1, &pid_pub2]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                "h_close1",
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub1 = c1->publish_at_least_once("topic1", "topic1_contents1");
                    pid_pub2 = c1->publish_at_least_once("topic1", "topic1_contents2");
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            c1->set_clean_session(false);
            c1->connect();
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error1");
            for (auto const& e : serialized) {
                auto const& packet = std::get<1>(e.second);
                if (std::get<0>(e.second)) {
                    // is publish
                    restore_v5_serialized_publish_message(c2, packet);
                }
                else {
                    // pubrel
                    restore_v5_serialized_pubrel_message(c2, packet);
                }
            }
            c2->connect();
        });
    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close2");
            s.close();
        });
    c2->set_v5_puback_handler(
        [&chk, &c2, &pid_pub1, &pid_pub2]
        (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
            auto ret = chk.match(
                "start",
                [&] {
                    MQTT_CHK("h_puback1");
                    BOOST_TEST(packet_id == pid_pub1);
                },
                "h_puback1",
                [&] {
                    MQTT_CHK("h_puback2");
                    BOOST_TEST(packet_id == pid_pub2);
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    MQTT_CHK("start");
    c1->connect();
    ios.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_SUITE_END()

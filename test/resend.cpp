// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_resend)

BOOST_AUTO_TEST_CASE( publish_qos1 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_at_least_once("topic1", "topic1_contents");
                            c->force_disconnect();
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub, ps = std::move(ps)]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&, ps = std::move(ps)] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_at_least_once("topic1", "topic1_contents", false, std::move(ps));
                            c->force_disconnect();
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        c->connect();
                    },
                    "h_puback",
                    [&] {
                        MQTT_CHK("h_close2");
                        s.close();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                c->connect();
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                            c->force_disconnect();
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_pubrec_handler(
                [&chk, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                            c->force_disconnect();
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        c->connect();
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        s.close();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                c->connect();
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pubrel_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_pubrec_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->force_disconnect();
                    return true;
                });
            c->set_pubcomp_handler(
                [&chk, &c]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == 1);
                    c->disconnect();
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_auto_pub_response(false);
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &c, &pid_pub, ps = std::move(ps)]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->pubrel(packet_id, mqtt::v5::reason_code::success, std::move(ps));
                    c->force_disconnect();
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == 1);
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        c->connect();
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        s.close();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                c->connect();
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_pubrel_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

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
            cont("h_error1"),
            // connect
            cont("h_connack3"),
            cont("h_pubrec"),
            // force_disconnect
            cont("h_error2"),
            // connect
            cont("h_connack4"),
            cont("h_pubcomp"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, & pid_pub]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                            c->force_disconnect();
                        },
                        "h_error1",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        },
                        "h_error2",
                        [&] {
                            MQTT_CHK("h_connack4");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_pubrec_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->force_disconnect();
                    return true;
                });
            c->set_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, & pid_pub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
                            c->force_disconnect();
                        },
                        "h_error1",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        },
                        "h_error2",
                        [&] {
                            MQTT_CHK("h_connack4");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->force_disconnect();
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == pid_pub);
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        c->connect();
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        s.close();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c]
            (boost::system::error_code const&) {
                auto ret = chk.match(
                    "h_connack2",
                    [&] {
                        MQTT_CHK("h_error1");
                        c->connect();
                    },
                    "h_pubrec",
                    [&] {
                        MQTT_CHK("h_error2");
                        c->connect();
                    }
                );
                BOOST_TEST(ret);
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub1 = c->publish_at_least_once("topic1", "topic1_contents1");
                            pid_pub2 = c->publish_at_least_once("topic1", "topic1_contents2");
                            c->force_disconnect();
                        },
                        "h_error1",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (packet_id_t packet_id) {
                    auto ret = chk.match(
                        "h_connack3",
                        [&] {
                            MQTT_CHK("h_puback1");
                            BOOST_TEST(packet_id == pid_pub1);
                        },
                        "h_puback1",
                        [&] {
                            MQTT_CHK("h_puback2");
                            BOOST_TEST(packet_id == pid_pub2);
                            c->disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            pid_pub1 = c->publish_at_least_once("topic1", "topic1_contents1");
                            pid_pub2 = c->publish_at_least_once("topic1", "topic1_contents2");
                            c->force_disconnect();
                        },
                        "h_error1",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    auto ret = chk.match(
                        "h_connack3",
                        [&] {
                            MQTT_CHK("h_puback1");
                            BOOST_TEST(packet_id == pid_pub1);
                        },
                        "h_puback1",
                        [&] {
                            MQTT_CHK("h_puback2");
                            BOOST_TEST(packet_id == pid_pub2);
                            c->disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        c->connect();
                    },
                    "h_puback2",
                    [&] {
                        MQTT_CHK("h_close2");
                        s.close();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error1");
                c->connect();
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

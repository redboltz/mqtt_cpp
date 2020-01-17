// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "test_util.hpp"

BOOST_AUTO_TEST_SUITE(test_resend)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( publish_qos1 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& b) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_pub;

        boost::asio::deadline_timer tim(ioc);

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

        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
            MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
            MQTT_NS::v5::property::topic_alias(0x1234U),
            MQTT_NS::v5::property::response_topic("response topic"_mb),
            MQTT_NS::v5::property::correlation_data("correlation data"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
            MQTT_NS::v5::property::subscription_identifier(123),
        };

        std::size_t user_prop_count = 0;
        b.set_publish_props_handler(
            [&user_prop_count, size = ps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == size);

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::payload_format_indicator const& t) {
                                BOOST_TEST(t.val() == MQTT_NS::v5::property::payload_format_indicator::string);
                            },
                            [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::topic_alias const& t) {
                                BOOST_TEST(t.val() == 0x1234U);
                            },
                            [&](MQTT_NS::v5::property::response_topic const& t) {
                                BOOST_TEST(t.val() == "response topic");
                            },
                            [&](MQTT_NS::v5::property::correlation_data const& t) {
                                BOOST_TEST(t.val() == "correlation data");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
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
                            [&](MQTT_NS::v5::property::subscription_identifier const& t) {
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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub, ps = std::move(ps)]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once | MQTT_NS::retain::no, std::move(ps));
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
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_puback",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                // TCP level disconnection detecting timing is unpredictable.
                // Sometimes broker first, sometimes the client (this test) first.
                // This test assume that the broker detects first, so I set timer.
                // If client detect the disconnection first, then reconnect with
                // existing client id. And it is overwritten at broker.
                // Then error handler in the broker called, assertion failed due to
                // no corresponding connection exists
                tim.expires_from_now(boost::posix_time::milliseconds(100));
                tim.async_wait(
                    [&c] (MQTT_NS::error_code ec) {
                        BOOST_ASSERT( ! ec);
                        connect_no_clean(c);
                    }
                );
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_qos2 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_pub;

        boost::asio::deadline_timer tim(ioc);

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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                // TCP level disconnection detecting timing is unpredictable.
                // Sometimes broker first, sometimes the client (this test) first.
                // This test assume that the broker detects first, so I set timer.
                // If client detect the disconnection first, then reconnect with
                // existing client id. And it is overwritten at broker.
                // Then error handler in the broker called, assertion failed due to
                // no corresponding connection exists
                tim.expires_from_now(boost::posix_time::milliseconds(100));
                tim.async_wait(
                    [&c] (MQTT_NS::error_code ec) {
                        BOOST_ASSERT( ! ec);
                        connect_no_clean(c);
                    }
                );
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pubrel_qos2 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& b) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_pub;

        boost::asio::deadline_timer tim(ioc);

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

        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        std::size_t user_prop_count = 0;

        b.set_pubrel_props_handler(
            [&user_prop_count, size = ps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        case MQTT_NS::protocol_version::v5:
            c->set_auto_pub_response(false);
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->pubrel(packet_id, MQTT_NS::v5::pubrel_reason_code::success, std::move(ps));
                    c->force_disconnect();
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c]
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                // TCP level disconnection detecting timing is unpredictable.
                // Sometimes broker first, sometimes the client (this test) first.
                // This test assume that the broker detects first, so I set timer.
                // If client detect the disconnection first, then reconnect with
                // existing client id. And it is overwritten at broker.
                // Then error handler in the broker called, assertion failed due to
                // no corresponding connection exists
                tim.expires_from_now(boost::posix_time::milliseconds(100));
                tim.async_wait(
                    [&c] (MQTT_NS::error_code ec) {
                        BOOST_ASSERT( ! ec);
                        connect_no_clean(c);
                    }
                );
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_pubrel_qos2 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_pub;

        boost::asio::deadline_timer tim(ioc);

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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, & pid_pub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, & pid_pub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    c->force_disconnect();
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_pubcomp",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                auto ret = chk.match(
                    "h_connack2",
                    [&] {
                        MQTT_CHK("h_error1");
                        // TCP level disconnection detecting timing is unpredictable.
                        // Sometimes broker first, sometimes the client (this test) first.
                        // This test assume that the broker detects first, so I set timer.
                        // If client detect the disconnection first, then reconnect with
                        // existing client id. And it is overwritten at broker.
                        // Then error handler in the broker called, assertion failed due to
                        // no corresponding connection exists
                        tim.expires_from_now(boost::posix_time::milliseconds(100));
                        tim.async_wait(
                            [&c] (MQTT_NS::error_code ec) {
                                BOOST_ASSERT( ! ec);
                                connect_no_clean(c);
                            }
                        );
                    },
                    "h_pubrec",
                    [&] {
                        MQTT_CHK("h_error2");
                        // TCP level disconnection detecting timing is unpredictable.
                        // Sometimes broker first, sometimes the client (this test) first.
                        // This test assume that the broker detects first, so I set timer.
                        // If client detect the disconnection first, then reconnect with
                        // existing client id. And it is overwritten at broker.
                        // Then error handler in the broker called, assertion failed due to
                        // no corresponding connection exists
                        tim.expires_from_now(boost::posix_time::milliseconds(100));
                        tim.async_wait(
                            [&c] (MQTT_NS::error_code ec) {
                                BOOST_ASSERT( ! ec);
                                connect_no_clean(c);
                            }
                        );
                    }
                );
                BOOST_TEST(ret);
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_pub1;
        std::uint16_t pid_pub2;

        boost::asio::deadline_timer tim(ioc);

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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
                            pid_pub1 = c->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_least_once);
                            pid_pub2 = c->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
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
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                            pid_pub1 = c->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_least_once);
                            pid_pub2 = c->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
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
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_puback2",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error1");
                // TCP level disconnection detecting timing is unpredictable.
                // Sometimes broker first, sometimes the client (this test) first.
                // This test assume that the broker detects first, so I set timer.
                // If client detect the disconnection first, then reconnect with
                // existing client id. And it is overwritten at broker.
                // Then error handler in the broker called, assertion failed due to
                // no corresponding connection exists
                tim.expires_from_now(boost::posix_time::milliseconds(100));
                tim.async_wait(
                    [&c] (MQTT_NS::error_code ec) {
                        BOOST_ASSERT( ! ec);
                        connect_no_clean(c);
                    }
                );
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

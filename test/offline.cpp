// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_offline)

template <typename Client>
inline void connect_no_clean(Client& c) {
    c->set_clean_session(false);
    if (c->get_protocol_version() == MQTT_NS::protocol_version::v5) {
        // set session_expiry_interval as infinity.
        c->connect(MQTT_NS::v5::properties{MQTT_NS::v5::property::session_expiry_interval(0xFFFFFFFFUL)});
    }
    else {
        c->connect();
    }
}

template <typename Client>
inline void async_connect_no_clean(Client& c) {
    c->set_clean_session(false);
    if (c->get_protocol_version() == MQTT_NS::protocol_version::v5) {
        // set session_expiry_interval as infinity.
        c->async_connect(std::vector<MQTT_NS::v5::property_variant>{MQTT_NS::v5::property::session_expiry_interval(0xFFFFFFFFUL)});
    }
    else {
        c->async_connect();
    }
}

BOOST_AUTO_TEST_CASE( publish_qos1 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
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
            // publish topic1 QoS1
            // connect
            cont("h_connack2"),
            cont("h_puback"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code /*reason*/, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &pid_pub, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
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


        checker chk = {
            cont("start"),
            // connect
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),
            // publish topic1 QoS2
            // connect
            cont("h_connack2"),
            cont("h_pubrec"),
            cont("h_pubcomp"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code /*reason*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code/*reason*/, MQTT_NS::v5::properties /*props*/) {
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
            [&chk, &c, &pid_pub, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
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


        checker chk = {
            cont("start"),
            // connect
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),
            // publish topic1 QoS1
            // publish topic1 QoS1
            // connect
            cont("h_connack2"),
            cont("h_puback1"),
            cont("h_puback2"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (packet_id_t packet_id) {
                    auto ret = chk.match(
                        "h_connack2",
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
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code /*reason*/, MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_connack2",
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
            [&chk, &c, &pid_pub1, &pid_pub2, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub1 = c->publish(/*topic_base()*/ + "987/topic1", "topic1_contents1", MQTT_NS::qos::at_least_once);
                        pid_pub2 = c->publish(/*topic_base()*/ + "987/topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
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
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( async_publish_qos1 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
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
            // publish topic1 QoS1
            cont("h_pub_finish"),
            // connect
            cont("h_connack2"),
            cont("h_puback"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->async_disconnect();
                        },
                        "h_pub_finish",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
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
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->async_disconnect();
                        },
                        "h_pub_finish",
                        [&] {
                            MQTT_CHK("h_connack2");
                            // The previous connection is not set Session Expiry Interval.
                            // That means session state is cleared on close.
                            BOOST_TEST(sp == false);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code /*reason*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &c, &pid_pub, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->acquire_unique_packet_id();
                        c->async_publish(
                            pid_pub,
                            "topic1",
                            "topic1_contents",
                            MQTT_NS::qos::at_least_once  | MQTT_NS::retain::no,
                            [&chk](MQTT_NS::error_code ec){
                                BOOST_TEST( ! ec);
                                MQTT_CHK("h_pub_finish");
                            }
                        );
                        async_connect_no_clean(c);
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
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        MQTT_CHK("start");
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_SUITE_END()

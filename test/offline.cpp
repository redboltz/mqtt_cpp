// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_offline)

BOOST_AUTO_TEST_CASE( publish_qos1 ) {
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
            // publish topic1 QoS1
            // connect
            cont("h_connack2"),
            cont("h_puback"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t /*reason*/, std::vector<mqtt::v5::property_variant> /*props*/) {
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
            [&chk, &c, &pid_pub, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->publish_at_least_once("topic1", "topic1_contents");
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
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
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
            // publish topic1 QoS2
            // connect
            cont("h_connack2"),
            cont("h_pubrec"),
            cont("h_pubcomp"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &pid_pub]
                (packet_id_t packet_id, std::uint8_t /*reason*/, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t /*reason*/, std::vector<mqtt::v5::property_variant> /*props*/) {
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
            [&chk, &c, &pid_pub, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
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
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
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
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
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
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
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
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub1, &pid_pub2]
                (packet_id_t packet_id, std::uint8_t /*reason*/, std::vector<mqtt::v5::property_variant> /*props*/) {
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
            [&chk, &c, &pid_pub1, &pid_pub2, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub1 = c->publish_at_least_once(/*topic_base()*/ + "987/topic1", "topic1_contents1");
                        pid_pub2 = c->publish_at_least_once(/*topic_base()*/ + "987/topic1", "topic1_contents2");
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
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( async_publish_qos1 ) {
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
            // publish topic1 QoS1
            cont("h_pub_finish"),
            // connect
            cont("h_connack2"),
            cont("h_puback"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub]
                (packet_id_t packet_id, std::uint8_t /*reason*/, std::vector<mqtt::v5::property_variant> /*props*/) {
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
            [&chk, &c, &pid_pub, &s]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        // offline publish
                        pid_pub = c->async_publish_at_least_once(
                            "topic1",
                            "topic1_contents",
                            false, // retain
                            [&chk](boost::system::error_code const& ec){
                                BOOST_TEST(ec == boost::system::errc::success);
                                MQTT_CHK("h_pub_finish");
                            }
                        );
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
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_SUITE_END()

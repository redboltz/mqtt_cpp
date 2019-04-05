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
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
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
        c->set_puback_handler(
            [&chk, &c, &pid_pub]
            (packet_id_t packet_id) {
                MQTT_CHK("h_puback");
                BOOST_TEST(packet_id == pid_pub);
                c->disconnect();
                return true;
            });
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
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
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pubrel_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
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
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( publish_pubrel_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
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
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
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
        MQTT_CHK("start");
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

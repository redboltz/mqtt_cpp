// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_multi_sub)

BOOST_AUTO_TEST_CASE( multi_channel ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1 QoS0 topic2 QoS0
            "h_suback",
            // publish topic1 QoS0
            "h_publish_topic1",
            // publish topic2 QoS0
            "h_publish_topic2",
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                pid_sub = c->subscribe(
                    "topic1", mqtt::qos::at_most_once,
                    "topic2", mqtt::qos::at_most_once);

                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_puback_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_pubrec_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_pubcomp_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_suback_handler(
            [&order, &current, &c, &pid_sub]
            (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 2U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                BOOST_TEST(*results[1] == mqtt::qos::at_most_once);
                c->publish_at_most_once("topic1", "topic1_contents");
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c, &pid_unsub]
            (std::uint16_t packet_id) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
                return true;
            });
        c->set_publish_handler(
            [&order, &current, &c, &pid_unsub]
            (std::uint8_t header,
             boost::optional<std::uint16_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                switch (order) {
                case 2:
                    BOOST_TEST(current() == "h_publish_topic1");
                    ++order;
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    c->publish_at_most_once("topic2", "topic2_contents");
                    break;
                case 3:
                    BOOST_TEST(current() == "h_publish_topic2");
                    ++order;
                    BOOST_TEST(topic == "topic2");
                    BOOST_TEST(contents == "topic2_contents");
                    pid_unsub = c->unsubscribe(
                        "topic1",
                        "topic2");
                    break;
                default:
                    BOOST_CHECK(false);
                    break;
                }
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( multi_client_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    int sub_count = 0;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_clean_session(true);
    c1->set_client_id("cid1");

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 2) s.close();
    };

    int order1 = 0;

    std::vector<std::string> const expected1 = {
        // connect
        "h_connack",
        // subscribe topic1 QoS0
        "h_suback",
        // publish topic1 QoS0
        "h_publish",
        "h_unsuback",
        // disconnect
        "h_close",
        "finish",
    };

    auto current1 =
        [&order1, &expected1]() -> std::string {
            try {
                return expected1.at(order1);
            }
            catch (std::out_of_range const& e) {
                return e.what();
            }
        };

    c1->set_connack_handler(
        [&order1, &current1, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(current1() == "h_connack");
            ++order1;
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c1->set_close_handler(
        [&order1, &current1, &server_close]
        () {
            BOOST_TEST(current1() == "h_close");
            ++order1;
            server_close();
        });
    c1->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_suback_handler(
        [&order1, &current1, &c1, &sub_count, &pid_sub1]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(current1() == "h_suback");
            ++order1;
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c1->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c1->set_unsuback_handler(
        [&order1, &current1, &c1, &pid_unsub1]
        (std::uint16_t packet_id) {
            BOOST_TEST(current1() == "h_unsuback");
            ++order1;
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
            return true;
        });
    c1->set_publish_handler(
        [&order1, &current1, &c1, &pid_unsub1]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(current1() == "h_publish");
            ++order1;
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
            return true;
        });

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_clean_session(true);
    c2->set_client_id("cid2");

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;

    std::vector<std::string> const expected2 = {
        // connect
        "h_connack",
        // subscribe topic1 QoS0
        "h_suback",
        // publish topic1 QoS0
        "h_publish",
        "h_unsuback",
        // disconnect
        "h_close",
        "finish",
    };

    auto current2 =
        [&order2, &expected2]() -> std::string {
            try {
                return expected2.at(order2);
            }
            catch (std::out_of_range const& e) {
                return e.what();
            }
        };

    c2->set_connack_handler(
        [&order2, &current2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(current2() == "h_connack");
            ++order2;
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &current2, &server_close]
        () {
            BOOST_TEST(current2() == "h_close");
            ++order2;
            server_close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_suback_handler(
        [&order2, &current2, &c2, &sub_count, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(current2() == "h_suback");
            ++order2;
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c2->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &current2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(current2() == "h_unsuback");
            ++order2;
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
        [&order2, &current2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(current2() == "h_publish");
            ++order2;
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(current1() == "finish");
    BOOST_TEST(current2() == "finish");
}

BOOST_AUTO_TEST_CASE( multi_client_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    // c3 --publish--> topic1 ----> c1, c2

    bool c1ready = false;
    bool c2ready = false;
    bool c3ready = false;
    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c3 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_clean_session(true);
    c2->set_clean_session(true);
    c3->set_clean_session(true);
    c1->set_client_id("cid1");
    c2->set_client_id("cid2");
    c3->set_client_id("cid3");
    int order1 = 0;
    int order2 = 0;
    int order3 = 0;

    std::vector<std::string> const expected1 = {
        // connect
        "h_connack",
        // subscribe topic1 QoS1
        "h_suback",
        "h_publish",
        "h_unsuback",
        // disconnect
        "h_close",
        "finish",
    };

    auto current1 =
        [&order1, &expected1]() -> std::string {
            try {
                return expected1.at(order1);
            }
            catch (std::out_of_range const& e) {
                return e.what();
            }
        };

    std::vector<std::string> const expected2 = {
        // connect
        "h_connack",
        // subscribe topic1 QoS1
        "h_suback",
        "h_publish",
        "h_unsuback",
        // disconnect
        "h_close",
        "finish",
    };

    auto current2 =
        [&order2, &expected2]() -> std::string {
            try {
                return expected2.at(order2);
            }
            catch (std::out_of_range const& e) {
                return e.what();
            }
        };

    std::vector<std::string> const expected3 = {
        // connect
        "h_connack",
        // publish topic1 QoS1
        "h_puback",
        // disconnect
        "h_close",
        "finish",
    };

    auto current3 =
        [&order3, &expected3]() -> std::string {
            try {
                return expected3.at(order3);
            }
            catch (std::out_of_range const& e) {
                return e.what();
            }
        };

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 3) s.close();
    };

    std::uint16_t pid_pub3;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    c1->set_connack_handler(
        [&order1, &current1, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(current1() == "h_connack");
            ++order1;
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c1->set_close_handler(
        [&order1, &current1, &server_close]
        () {
            BOOST_TEST(current1() == "h_close");
            ++order1;
            server_close();
        });
    c1->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1->set_suback_handler(
        [&order1, &current1, &c1ready, &c2ready, &c3ready, &c3, &pid_sub1, &pid_pub3]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(current1() == "h_suback");
            ++order1;
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c1ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c1->set_unsuback_handler(
        [&order1, &current1, &c1, &pid_unsub1]
        (std::uint16_t packet_id) {
            BOOST_TEST(current1() == "h_unsuback");
            ++order1;
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
            return true;
        });
    c1->set_publish_handler(
        [&order1, &current1, &c1, &pid_unsub1]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(current1() == "h_publish");
            ++order1;
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
            return true;
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2->set_connack_handler(
        [&order2, &current2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(current2() == "h_connack");
            ++order2;
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &current2, &server_close]
        () {
            BOOST_TEST(current2() == "h_close");
            ++order2;
            server_close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&order2, &current2, &c1ready, &c2ready, &c3ready, &c3, &pid_sub2, &pid_pub3]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(current2() == "h_suback");
            ++order2;
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c2ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &current2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(current2() == "h_unsuback");
            ++order2;
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
        [&order2, &current2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(current2() == "h_publish");
            ++order2;
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c3->set_connack_handler(
        [&order3, &current3, &c3, &c1ready, &c2ready, &c3ready, &pid_pub3]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(current3() == "h_connack");
            ++order3;
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c3ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c3->set_close_handler(
        [&order3, &current3, &server_close]
        () {
            BOOST_TEST(current3() == "h_close");
            ++order3;
            server_close();
        });
    c3->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c3->set_puback_handler(
        [&order3, &current3, &c3, &pid_pub3]
        (std::uint16_t packet_id) {
            BOOST_TEST(current3() == "h_puback");
            ++order3;
            BOOST_TEST(packet_id == pid_pub3);
            c3->disconnect();
            return true;
        });
    c3->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c3->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });

    c1->connect();
    c2->connect();
    c3->connect();

    ios.run();
    BOOST_TEST(current1() == "finish");
    BOOST_TEST(current2() == "finish");
    BOOST_TEST(current3() == "finish");
}

BOOST_AUTO_TEST_SUITE_END()

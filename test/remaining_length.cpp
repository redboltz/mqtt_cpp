// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_remaining_length)

BOOST_AUTO_TEST_CASE( pub_sub_over_127 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 128; ++i) {
            test_contents.push_back(static_cast<char>(i));
        }

        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
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
                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
            [&order, &current, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                c->publish_at_most_once("topic1", test_contents);
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c, &pid_unsub]
            (packet_id_t packet_id) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
                return true;
            });
        c->set_publish_handler(
            [&order, &current, &c, &pid_unsub, &test_contents]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(current() == "h_publish");
                ++order;
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_sub_over_16384 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 16384; ++i) {
            test_contents.push_back(static_cast<char>(i & 0xff));
        }

        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
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
                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
            [&order, &current, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                c->publish_at_most_once("topic1", test_contents);
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c, &pid_unsub]
            (packet_id_t packet_id) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
                return true;
            });
        c->set_publish_handler(
            [&order, &current, &c, &pid_unsub, &test_contents]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(current() == "h_publish");
                ++order;
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

# if 0 // It would make network load too much.

BOOST_AUTO_TEST_CASE( pub_sub_over_2097152 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 2097152; ++i) {
            test_contents.push_back(static_cast<char>(i % 0xff));
        }

        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
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
                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
            [&order, &current, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                c->publish_at_most_once("topic1", test_contents);
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c, &pid_unsub]
            (packet_id_t packet_id) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
                return true;
            });
        c->set_publish_handler(
            [&order, &current, &c, &pid_unsub, &test_contents]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(current() == "h_publish");
                ++order;
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

#endif

BOOST_AUTO_TEST_SUITE_END()

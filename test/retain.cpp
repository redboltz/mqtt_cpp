// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_retain)

BOOST_AUTO_TEST_CASE( simple ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // publish topic1 QoS0 retain
            // subscribe topic1 QoS0
            "h_suback",
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

                c->publish_at_most_once("topic1", "retained_contents", true);

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
            [&order, &current, &pid_sub]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
            [&order, &current, &c, &pid_unsub]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(current() == "h_publish");
                ++order;
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents");
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( overwrite ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // publish topic1 QoS0 retain
            // publish topic1 QoS0 retain
            // publish topic1 QoS0
            // subscribe topic1 QoS0
            "h_suback",
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

                c->publish_at_most_once("topic1", "retained_contents1", true);
                c->publish_at_most_once("topic1", "retained_contents2", true);
                c->publish_at_most_once("topic1", "retained_contents3", false);

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
            [&order, &current, &pid_sub]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
            [&order, &current, &c, &pid_unsub]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(current() == "h_publish");
                ++order;
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents2");
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( retain_and_publish ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1 QoS0
            "h_suback1",
            // publish topic1 QoS0 retain
            "h_publish1",
            // unsubscribe topic1
            "h_unsuback1",
            // subscribe topic1 QoS0
            "h_suback2",
            "h_publish2",
            // unsubscribe topic1
            "h_unsuback2",
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
            [&order, &current, &c, &pid_sub]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                switch (order) {
                case 1:
                    BOOST_TEST(current() == "h_suback1");
                    ++order;
                    c->publish_at_most_once("topic1", "topic1_contents", true);
                    break;
                case 4:
                    BOOST_TEST(current() == "h_suback2");
                    ++order;
                    break;
                default:
                    BOOST_CHECK(false);
                    break;
                }
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c, &pid_sub, &pid_unsub]
            (packet_id_t packet_id) {
                BOOST_TEST(packet_id == pid_unsub);
                switch (order) {
                case 3:
                    BOOST_TEST(current() == "h_unsuback1");
                    ++order;
                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                    break;
                case 6:
                    BOOST_TEST(current() == "h_unsuback2");
                    ++order;
                    c->disconnect();
                    break;
                default:
                    BOOST_CHECK(false);
                    break;
                }
                return true;
            });
        c->set_publish_handler(
            [&order, &current, &c, &pid_unsub]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                pid_unsub = c->unsubscribe("topic1");
                switch (order) {
                case 2:
                    BOOST_TEST(current() == "h_publish1");
                    ++order;
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    break;
                case 5:
                    BOOST_TEST(current() == "h_publish2");
                    ++order;
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
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


BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_manual_publish)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1 QoS0
            "h_suback",
            // publish topic1 QoS1
            "h_publish",
            "h_puback",
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
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                BOOST_TEST(
                    c->subscribe(
                        0,
                        "topic1",
                        mqtt::qos::at_most_once) == false);
                BOOST_TEST(
                    c->subscribe(
                        1,
                        "topic1",
                        mqtt::qos::at_most_once) == true);
                BOOST_TEST(
                    c->subscribe(
                        1,
                        "topic1",
                        mqtt::qos::at_most_once) == false);
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
            [&order, &current, &c]
            (packet_id_t packet_id) {
                BOOST_TEST(packet_id == 1);
                BOOST_TEST(current() == "h_puback");
                ++order;
                {
                    packet_id_t packet_id = 0;
                    BOOST_TEST(
                        c->unsubscribe(packet_id, "topic1") == false);
                }
                BOOST_TEST(
                    c->unsubscribe(1, "topic1") == true);
                BOOST_TEST(
                    c->unsubscribe(1, "topic1") == false);
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
            [&order, &current, &c]
            (packet_id_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                BOOST_TEST(packet_id == 1);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                BOOST_TEST(c->publish(
                               0,
                               "topic1",
                               "topic1_contents",
                               mqtt::qos::at_least_once) == false);
                BOOST_TEST(c->publish(
                               1,
                               "topic1",
                               "topic1_contents",
                               mqtt::qos::at_least_once) == true);
                BOOST_TEST(c->publish(
                               1,
                               "topic1",
                               "topic1_contents",
                               mqtt::qos::at_least_once) == false);
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (packet_id_t packet_id) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                BOOST_TEST(packet_id == 1);
                c->disconnect();
                return true;
            });
        c->set_publish_handler(
            [&order, &current]
            (std::uint8_t header,
             boost::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                BOOST_TEST(current() == "h_publish");
                ++order;
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_SUITE_END()

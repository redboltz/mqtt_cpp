// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_manual_publish)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c.set_clean_session(true);

    int order = 0;
    bool pub_seq_finished = false;
    c.set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);

            BOOST_TEST(
                c.subscribe(
                    0,
                    "mqtt_cpp_client/test/topic1",
                    mqtt::qos::at_most_once) == false);
            BOOST_TEST(
                c.subscribe(
                    1,
                    "mqtt_cpp_client/test/topic1",
                    mqtt::qos::at_most_once) == true);
            BOOST_TEST(
                c.subscribe(
                    1,
                    "mqtt_cpp_client/test/topic1",
                    mqtt::qos::at_most_once) == false);
        });
    c.set_close_handler(
        [&order]
        () {
            BOOST_TEST(order++ == 5);
        });
    c.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c.set_puback_handler(
        [&order, &c, &pub_seq_finished]
        (std::uint16_t packet_id) {
            BOOST_TEST(packet_id == 1);
            switch (order++) {
            case 2:
                break;
            case 3:
                pub_seq_finished = true;
                {
                    std::uint16_t packet_id = 0;
                    BOOST_TEST(
                        c.unsubscribe(packet_id, "mqtt_cpp_client/test/topic1") == false);
                }
                BOOST_TEST(
                    c.unsubscribe(1, "mqtt_cpp_client/test/topic1") == true);
                BOOST_TEST(
                    c.unsubscribe(1, "mqtt_cpp_client/test/topic1") == false);
                break;
            default:
                BOOST_CHECK(false);
                break;
            }
        });
    c.set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c.set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c.set_suback_handler(
        [&order, &c]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order++ == 1);
            BOOST_TEST(packet_id == 1);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            BOOST_TEST(c.publish(
                           0,
                           "mqtt_cpp_client/test/topic1",
                           "topic1_contents",
                           mqtt::qos::at_least_once) == false);
            BOOST_TEST(c.publish(
                           1,
                           "mqtt_cpp_client/test/topic1",
                           "topic1_contents",
                           mqtt::qos::at_least_once) == true);
            BOOST_TEST(c.publish(
                           1,
                           "mqtt_cpp_client/test/topic1",
                           "topic1_contents",
                           mqtt::qos::at_least_once) == false);
        });
    c.set_unsuback_handler(
        [&order, &c, &pub_seq_finished]
        (std::uint16_t packet_id) {
            BOOST_TEST(order++ == 4);
            if (pub_seq_finished) BOOST_TEST(packet_id == 1);
            else BOOST_TEST(packet_id == 2);
            c.disconnect();
        });
    c.set_publish_handler(
        [&order, &c]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "mqtt_cpp_client/test/topic1");
            BOOST_TEST(contents == "topic1_contents");
            switch (order++) {
            case 2:
                break;
            case 3:
                c.unsubscribe("mqtt_cpp_client/test/topic1");
                break;
            default:
                BOOST_CHECK(false);
                break;
            }
        });
    c.connect();
    ios.run();
    BOOST_TEST(order++ == 6);
}

BOOST_AUTO_TEST_SUITE_END()

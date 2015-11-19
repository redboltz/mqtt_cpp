// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_will)

BOOST_AUTO_TEST_CASE( will_qos0 ) {
    boost::asio::io_service ios;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_client_id("cid1");
    c1.set_clean_session(true);
    c1.set_will(
        mqtt::will("mqtt_cpp_client/test/topic1", "will_contents"));

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2.set_client_id("cid2");
    c2.set_clean_session(true);

    int order1 = 0;
    c1.set_connack_handler(
        [&order1, &c1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);
        });
    c1.set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1.set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2.set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c2.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);

            pid_sub2 = c2.subscribe("mqtt_cpp_client/test/topic1", mqtt::qos::at_most_once);
        });
    c2.set_close_handler(
        [&order2]
        () {
            BOOST_TEST(order2++ == 4);
        });
    c2.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2.set_suback_handler(
        [&order2, &c2, &c1, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c1.force_disconnect();
        });
    c2.set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2.disconnect();
        });
    c2.set_publish_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order2++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "mqtt_cpp_client/test/topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2.unsubscribe("mqtt_cpp_client/test/topic1");
        });

    c1.connect();
    c2.connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_qos1 ) {
    boost::asio::io_service ios;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_client_id("cid1");
    c1.set_clean_session(true);
    c1.set_will(
        mqtt::will("mqtt_cpp_client/test/topic1", "will_contents", mqtt::qos::at_least_once));

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2.set_client_id("cid2");
    c2.set_clean_session(true);

    int order1 = 0;
    c1.set_connack_handler(
        [&order1, &c1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);
        });
    c1.set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1.set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2.set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c2.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);

            pid_sub2 = c2.subscribe("mqtt_cpp_client/test/topic1", mqtt::qos::at_least_once);
        });
    c2.set_close_handler(
        [&order2]
        () {
            BOOST_TEST(order2++ == 4);
        });
    c2.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2.set_suback_handler(
        [&order2, &c2, &c1, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            c1.force_disconnect();
        });
    c2.set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2.disconnect();
        });
    c2.set_publish_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order2++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(*packet_id != 0);
            BOOST_TEST(topic == "mqtt_cpp_client/test/topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2.unsubscribe("mqtt_cpp_client/test/topic1");
        });

    c1.connect();
    c2.connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_qos2 ) {
    boost::asio::io_service ios;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_client_id("cid1");
    c1.set_clean_session(true);
    c1.set_will(
        mqtt::will("mqtt_cpp_client/test/topic1", "will_contents", mqtt::qos::exactly_once));

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2.set_client_id("cid2");
    c2.set_clean_session(true);

    int order1 = 0;
    c1.set_connack_handler(
        [&order1, &c1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);
        });
    c1.set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1.set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2.set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c2.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);

            pid_sub2 = c2.subscribe("mqtt_cpp_client/test/topic1", mqtt::qos::exactly_once);
        });
    c2.set_close_handler(
        [&order2]
        () {
            BOOST_TEST(order2++ == 4);
        });
    c2.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2.set_suback_handler(
        [&order2, &c2, &c1, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            c1.force_disconnect();
        });
    c2.set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2.disconnect();
        });
    c2.set_publish_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order2++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::exactly_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(*packet_id != 0);
            BOOST_TEST(topic == "mqtt_cpp_client/test/topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2.unsubscribe("mqtt_cpp_client/test/topic1");
        });

    c1.connect();
    c2.connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_retain ) {
    boost::asio::io_service ios;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_client_id("cid1");
    c1.set_clean_session(true);
    c1.set_will(
        mqtt::will("mqtt_cpp_client/test/topic1", "will_contents", true));

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2.set_client_id("cid2");
    c2.set_clean_session(true);

    int order1 = 0;
    c1.set_connack_handler(
        [&order1, &c1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);
        });
    c1.set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1.set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2.set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c2.publish_at_most_once("mqtt_cpp_client/test/topic1", "", true);

            pid_sub2 = c2.subscribe("mqtt_cpp_client/test/topic1", mqtt::qos::at_most_once);
        });
    c2.set_close_handler(
        [&order2]
        () {
            BOOST_TEST(order2++ == 7);
        });
    c2.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2.set_suback_handler(
        [&order2, &c2, &c1, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            switch (order2++) {
            case 1:
                c1.force_disconnect();
                break;
            case 4:
                break;
            default:
                break;
            }
        });
    c2.set_unsuback_handler(
        [&order2, &c2, &pid_unsub2, &pid_sub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(packet_id == pid_unsub2);
            switch (order2++) {
            case 3:
                pid_sub2 = c2.subscribe("mqtt_cpp_client/test/topic1", mqtt::qos::at_most_once);
                break;
            case 6:
                c2.disconnect();
                break;
            default:
                BOOST_CHECK(false);
                break;
            }
        });
    c2.set_publish_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "mqtt_cpp_client/test/topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2.unsubscribe("mqtt_cpp_client/test/topic1");
            switch (order2++) {
            case 2:
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                break;
            case 5:
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                break;
            default:
                BOOST_CHECK(false);
                break;
            }
        });

    c1.connect();
    c2.connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 8);
}


BOOST_AUTO_TEST_SUITE_END()

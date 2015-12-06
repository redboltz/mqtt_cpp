// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_multi_sub)

BOOST_AUTO_TEST_CASE( multi_channel ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c.set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;

    int order = 0;
    c.set_connack_handler(
        [&order, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c.publish_at_most_once(topic_base() + "/topic1", "", true);
            c.publish_at_most_once(topic_base() + "/topic2", "", true);

            pid_sub = c.subscribe(
                topic_base() + "/topic1", mqtt::qos::at_most_once,
                topic_base() + "/topic2", mqtt::qos::at_most_once);
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
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
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
        [&order, &c, &pid_sub]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order++ == 1);
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 2);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            BOOST_TEST(*results[1] == mqtt::qos::at_most_once);
            c.publish_at_most_once(topic_base() + "/topic1", "topic1_contents");
        });
    c.set_unsuback_handler(
        [&order, &c, &pid_unsub]
        (std::uint16_t packet_id) {
            BOOST_TEST(order++ == 4);
            BOOST_TEST(packet_id == pid_unsub);
            c.disconnect();
        });
    c.set_publish_handler(
        [&order, &c, &pid_unsub]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            switch (order++) {
            case 2:
                BOOST_TEST(topic == topic_base() + "/topic1");
                BOOST_TEST(contents == "topic1_contents");
                c.publish_at_most_once(topic_base() + "/topic2", "topic2_contents");
                break;
            case 3:
                BOOST_TEST(topic == topic_base() + "/topic2");
                BOOST_TEST(contents == "topic2_contents");
                pid_unsub = c.unsubscribe(
                    topic_base() + "/topic1",
                    topic_base() + "/topic2");
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

BOOST_AUTO_TEST_CASE( multi_client_qos0 ) {
    boost::asio::io_service ios;
    int sub_count = 0;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_clean_session(true);

    int order1 = 0;
    c1.set_connack_handler(
        [&order1, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once(topic_base() + "/topic1", "", true);

            pid_sub1 = c1.subscribe(topic_base() + "/topic1", mqtt::qos::at_most_once);
        });
    c1.set_close_handler(
        [&order1]
        () {
            BOOST_TEST(order1++ == 4);
        });
    c1.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1.set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1.set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1.set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1.set_suback_handler(
        [&order1, &c1, &sub_count, &pid_sub1]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order1++ == 1);
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c1.publish_at_most_once(topic_base() + "/topic1", "topic1_contents");
        });
    c1.set_unsuback_handler(
        [&order1, &c1, &pid_unsub1]
        (std::uint16_t packet_id) {
            BOOST_TEST(order1++ == 3);
            BOOST_TEST(packet_id == pid_unsub1);
            c1.disconnect();
        });
    c1.set_publish_handler(
        [&order1, &c1, &pid_unsub1]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(order1++ == 2);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1.unsubscribe(topic_base() + "/topic1");
        });

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2.set_clean_session(true);

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
            c2.publish_at_most_once(topic_base() + "/topic1", "", true);

            pid_sub2 = c2.subscribe(topic_base() + "/topic1", mqtt::qos::at_most_once);
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
    c2.set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2.set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2.set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2.set_suback_handler(
        [&order2, &c2, &sub_count, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c2.publish_at_most_once(topic_base() + "/topic1", "topic1_contents");
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
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(order2++ == 2);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2.unsubscribe(topic_base() + "/topic1");
        });

    c1.connect();
    c2.connect();

    ios.run();
    BOOST_TEST(order1++ == 5);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( multi_client_qos1 ) {
    // c3 --publish--> topic1 ----> c1, c2
    boost::asio::io_service ios;
    int sub_count = 0;

    bool c1ready = false;
    bool c2ready = false;
    bool c3ready = false;
    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c3 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1.set_clean_session(true);
    c2.set_clean_session(true);
    c3.set_clean_session(true);
    int order1 = 0;
    int order2 = 0;
    int order3 = 0;

    std::uint16_t pid_pub3;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    c1.set_connack_handler(
        [&order1, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c1.publish_at_most_once(topic_base() + "/topic1", "", true);

            pid_sub1 = c1.subscribe(topic_base() + "/topic1", mqtt::qos::at_least_once);
        });
    c1.set_close_handler(
        [&order1]
        () {
            BOOST_TEST(order1++ == 4);
        });
    c1.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1.set_suback_handler(
        [&order1, &c1, &sub_count, &c1ready, &c2ready, &c3ready, &c3, &pid_sub1, &pid_pub3]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order1++ == 1);
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c1ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3.publish_at_least_once(topic_base() + "/topic1", "topic1_contents");
            }
        });
    c1.set_unsuback_handler(
        [&order1, &c1, &pid_unsub1]
        (std::uint16_t packet_id) {
            BOOST_TEST(order1++ == 3);
            BOOST_TEST(packet_id == pid_unsub1);
            c1.disconnect();
        });
    c1.set_publish_handler(
        [&order1, &c1, &pid_unsub1]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(order1++ == 2);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1.unsubscribe(topic_base() + "/topic1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2.set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c2.publish_at_most_once(topic_base() + "/topic1", "", true);

            pid_sub2 = c2.subscribe(topic_base() + "/topic1", mqtt::qos::at_least_once);
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
        [&order2, &c2, &sub_count, &c1ready, &c2ready, &c3ready, &c3, &pid_sub2, &pid_pub3]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c2ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3.publish_at_least_once(topic_base() + "/topic1", "topic1_contents");
            }
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
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(order2++ == 2);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2.unsubscribe(topic_base() + "/topic1");
        });

    c3.set_connack_handler(
        [&order3, &c3, &c1ready, &c2ready, &c3ready, &pid_pub3]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order3++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

            // Clear retaind contents
            c3.publish_at_most_once(topic_base() + "/topic1", "", true);
            c3ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3.publish_at_least_once(topic_base() + "/topic1", "topic1_contents");
            }
        });
    c3.set_close_handler(
        [&order3]
        () {
            BOOST_TEST(order3++ == 2);
        });
    c3.set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c3.set_puback_handler(
        [&order3, &c3, &pid_pub3]
        (std::uint16_t packet_id) {
            BOOST_TEST(order3++ == 1);
            BOOST_TEST(packet_id == pid_pub3);
            c3.disconnect();
        });
    c3.set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c3.set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });

    c1.connect();
    c2.connect();
    c3.connect();

    ios.run();
    BOOST_TEST(order1++ == 5);
    BOOST_TEST(order2++ == 5);
    BOOST_TEST(order3++ == 3);
}

BOOST_AUTO_TEST_SUITE_END()

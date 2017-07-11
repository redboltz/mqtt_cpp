// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_remaining_length)

BOOST_AUTO_TEST_CASE( pub_sub_over_127 ) {
    fixture_clear_retain();
    std::string test_contents;
    for (std::size_t i = 0; i < 128; ++i) {
        test_contents.push_back(static_cast<char>(i));
    }

    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;

    int order = 0;
    c->set_connack_handler(
        [&order, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe(topic_base() + "/topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&order]
        () {
            BOOST_TEST(order++ == 4);
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
        [&order, &c, &pid_sub, &test_contents]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order++ == 1);
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c->publish_at_most_once(topic_base() + "/topic1", test_contents);
            return true;
        });
    c->set_unsuback_handler(
        [&order, &c, &pid_unsub]
        (std::uint16_t packet_id) {
            BOOST_TEST(order++ == 3);
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&order, &c, &pid_unsub, &test_contents]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == test_contents);
            pid_unsub = c->unsubscribe(topic_base() + "/topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 5);
}

BOOST_AUTO_TEST_CASE( pub_sub_over_16384 ) {
    fixture_clear_retain();
    std::string test_contents;
    for (std::size_t i = 0; i < 16384; ++i) {
        test_contents.push_back(static_cast<char>(i & 0xff));
    }

    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;

    int order = 0;
    c->set_connack_handler(
        [&order, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe(topic_base() + "/topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&order]
        () {
            BOOST_TEST(order++ == 4);
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
        [&order, &c, &pid_sub, &test_contents]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order++ == 1);
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c->publish_at_most_once(topic_base() + "/topic1", test_contents);
            return true;
        });
    c->set_unsuback_handler(
        [&order, &c, &pid_unsub]
        (std::uint16_t packet_id) {
            BOOST_TEST(order++ == 3);
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&order, &c, &pid_unsub, &test_contents]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == test_contents);
            pid_unsub = c->unsubscribe(topic_base() + "/topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 5);
}

# if 0 // It would make network load too much.

BOOST_AUTO_TEST_CASE( pub_sub_over_2097152 ) {
    fixture_clear_retain();
    std::string test_contents;
    for (std::size_t i = 0; i < 2097152; ++i) {
        test_contents.push_back(i);
    }

    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;

    int order = 0;
    c->set_connack_handler(
        [&order, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe(topic_base() + "/topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&order]
        () {
            BOOST_TEST(order++ == 4);
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
        [&order, &c, &pid_sub, &test_contents]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order++ == 1);
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c->publish_at_most_once(topic_base() + "/topic1", test_contents);
            return true;
        });
    c->set_unsuback_handler(
        [&order, &c, &pid_unsub]
        (std::uint16_t packet_id) {
            BOOST_TEST(order++ == 3);
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&order, &c, &pid_unsub, &test_contents]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(order++ == 2);
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == topic_base() + "/topic1");
            BOOST_TEST(contents == test_contents);
            pid_unsub = c->unsubscribe(topic_base() + "/topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 5);
}

#endif

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "test_settings.hpp"
#include "test_broker.hpp"
#include "test_server_no_tls.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_will)

BOOST_AUTO_TEST_CASE( will_qos0 ) {
    boost::asio::io_service ios;
    test_broker b;
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents"));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    int order1 = 0;
    c1->set_connack_handler(
        [&order1, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2->set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &s]
        () {
            BOOST_TEST(order2++ == 4);
            s.close();
        });
    c2->set_error_handler(
        [&order2, &s]
        (boost::system::error_code const&) {
            BOOST_TEST(order2++ == 4);
            s.close();
        });
    c2->set_suback_handler(
        [&order2, &c1_force_disconnect, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c1_force_disconnect();
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
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
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_qos1 ) {
    boost::asio::io_service ios;
    test_broker b;
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", mqtt::qos::at_least_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    int order1 = 0;
    c1->set_connack_handler(
        [&order1, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2->set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &s]
        () {
            BOOST_TEST(order2++ == 4);
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&order2, &c1_force_disconnect, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            c1_force_disconnect();
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
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
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_qos2 ) {
    boost::asio::io_service ios;
    test_broker b;
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", mqtt::qos::exactly_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    int order1 = 0;
    c1->set_connack_handler(
        [&order1, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2->set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::exactly_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &s]
        () {
            BOOST_TEST(order2++ == 4);
            s.close();
        });
    c2->set_error_handler(
        [&order2, &s]
        (boost::system::error_code const&) {
            BOOST_TEST(order2++ == 4);
            s.close();
        });
    c2->set_suback_handler(
        [&order2, &c1_force_disconnect, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(order2++ == 1);
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            c1_force_disconnect();
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(order2++ == 3);
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
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
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 5);
}

BOOST_AUTO_TEST_CASE( will_retain ) {
    boost::asio::io_service ios;
    test_broker b;
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", true));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    int order1 = 0;
    c1->set_connack_handler(
        [&order1, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order1++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&order1]
        (boost::system::error_code const&) {
            BOOST_TEST(order1++ == 1);
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    int order2 = 0;
    c2->set_connack_handler(
        [&order2, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order2++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&order2, &s]
        () {
            BOOST_TEST(order2++ == 7);
            s.close();
        });
    c2->set_error_handler(
        [&order2, &s]
        (boost::system::error_code const&) {
            BOOST_TEST(order2++ == 7);
            s.close();
        });
    c2->set_suback_handler(
        [&order2, &c1_force_disconnect, &pid_sub2]
        (std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            switch (order2++) {
            case 1:
                c1_force_disconnect();
                break;
            case 4:
                break;
            default:
                break;
            }
            return true;
        });
    c2->set_unsuback_handler(
        [&order2, &c2, &pid_unsub2, &pid_sub2]
        (std::uint16_t packet_id) {
            BOOST_TEST(packet_id == pid_unsub2);
            switch (order2++) {
            case 3:
                pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
                break;
            case 6:
                c2->disconnect();
                break;
            default:
                BOOST_CHECK(false);
                break;
            }
            return true;
        });
    c2->set_publish_handler(
        [&order2, &c2, &pid_unsub2]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
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
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(order1++ == 2);
    BOOST_TEST(order2++ == 8);
}


BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "test_settings.hpp"
#include "test_broker.hpp"
#include "test_server_no_tls.hpp"
#include "checker.hpp"

#include <mqtt/client.hpp>
#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_pubsub_no_strand)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS0
        cont("h_publish"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
            s.close();
            return true;
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
            return true;
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
        [&chk, &c, &pid_sub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &c, &pid_unsub]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS1
        cont("h_publish"),
        cont("h_puback"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
        [&chk, &c, &pid_pub, &pid_sub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            pid_pub = c->publish_at_least_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS2
        cont("h_publish"),
        cont("h_pubrec"),
        cont("h_pubcomp"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_unsub, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->set_suback_handler(
        [&chk, &c, &pid_pub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == 1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS2
        cont("h_suback"),
        // publish topic1 QoS0
        cont("h_publish"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &c, &pid_sub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            c->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &c, &pid_unsub]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS1
        cont("h_publish"),
        cont("h_pub_res_sent"),
        cont("h_puback"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
    mqtt::optional<std::uint16_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            pid_pub = c->publish_at_least_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &recv_packet_id]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS2
        cont("h_publish"),
        cont("h_pub_res_sent"),
        cont("h_pubrec"),
        cont("h_pubcomp"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    mqtt::optional<std::uint16_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &recv_packet_id]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS2
        cont("h_suback"),
        // publish topic1 QoS0
        cont("h_publish"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::exactly_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &c, &pid_sub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            c->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &c, &pid_unsub]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS1
        cont("h_publish"),
        cont("h_pub_res_sent"),
        cont("h_puback"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::exactly_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
    mqtt::optional<std::uint16_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            pid_pub = c->publish_at_least_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &recv_packet_id]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS2
        cont("h_publish"),
        cont("h_pubrec"),
        cont("h_pub_res_sent"),
        cont("h_pubcomp"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::exactly_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    mqtt::optional<std::uint16_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            pid_pub = c->publish_exactly_once("topic1", "topic1_contents");
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &recv_packet_id]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::exactly_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( publish_function ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS0
        cont("h_publish"),
        cont("h_unsuback"),
        // disconnect
        cont("h_close"),
    };

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close");
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
        [&chk, &c, &pid_sub]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c->publish("topic1", "topic1_contents", mqtt::qos::at_most_once);
            return true;
        });
    c->set_unsuback_handler(
        [&chk, &c, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback");
            BOOST_TEST(packet_id == pid_unsub);
            c->disconnect();
            return true;
        });
    c->set_publish_handler(
        [&chk, &c, &pid_unsub]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ios.run();
    BOOST_TEST(chk.all());
}


BOOST_AUTO_TEST_SUITE_END()

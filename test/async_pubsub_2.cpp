// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

#include <vector>
#include <string>

BOOST_AUTO_TEST_SUITE(test_async_pubsub_2)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
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
                    BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
                    c->async_publish_at_most_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
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
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_2);
                    c->async_publish_at_most_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c, &pid_unsub]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->set_pub_res_sent_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
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
            deps("h_puback", "h_publish"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        mqtt::optional<std::uint16_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
            });

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_pub, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    pid_unsub = c->async_unsubscribe("topic1");
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
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
                    pid_pub = c->async_publish_at_least_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
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
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub, &pid_unsub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_2);
                    pid_pub = c->async_publish_at_least_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &recv_packet_id]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
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
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos2 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
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
            deps("h_pubcomp", "h_pubrec"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        mqtt::optional<std::uint16_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
            });

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                    return true;
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
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
                    pid_pub = c->async_publish_exactly_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
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
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubrec_handler(
                [&chk, &pid_pub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubrec");
                    BOOST_TEST(packet_id == pid_pub);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                [&chk, &c, &pid_pub, &pid_unsub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_pubcomp");
                    BOOST_TEST(packet_id == pid_pub);
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_2);
                    pid_pub = c->async_publish_exactly_once("topic1", "topic1_contents");
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &recv_packet_id]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
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
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( publish_function ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::at_most_once);
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
                    c->async_publish("topic1", "topic1_contents", mqtt::qos::at_most_once);
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
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
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::at_most_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                    c->async_publish("topic1", "topic1_contents", mqtt::qos::at_most_once);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c, &pid_unsub]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( publish_dup_function ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS1
            cont("h_suback"),
            // publish topic1 QoS1
            cont("h_publish"),
            cont("h_puback"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::at_least_once);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->async_unsubscribe("topic1");
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
                    auto ret =
                        c->async_publish_dup(1, "topic1", "topic1_contents", mqtt::qos::at_least_once);
                    BOOST_TEST(ret == true);
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->async_disconnect();
                    return true;
                });
            c->set_publish_handler(
                [&chk]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(mqtt::publish::is_dup(header) == false); // not propagated
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                    pid_sub = c->async_subscribe("topic1", mqtt::qos::at_least_once);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->async_unsubscribe("topic1");
                    return true;
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_1);
                    auto ret =
                        c->async_publish_dup(1, "topic1", "topic1_contents", mqtt::qos::at_least_once);
                    BOOST_TEST(ret == true);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(mqtt::publish::is_dup(header) == false); // not propagated
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_sub_prop ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v5) return;

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

        std::vector<mqtt::v5::property_variant> ps {
            mqtt::v5::property::payload_format_indicator(mqtt::v5::property::payload_format_indicator::string),
            mqtt::v5::property::message_expiry_interval(0x12345678UL),
            mqtt::v5::property::topic_alias(0x1234U),
            mqtt::v5::property::response_topic("response topic"),
            mqtt::v5::property::correlation_data("correlation data"),
            mqtt::v5::property::user_property("key1", "val1"),
            mqtt::v5::property::user_property("key2", "val2"),
            mqtt::v5::property::subscription_identifier(123),
        };

        auto prop_size = ps.size();
        std::size_t user_prop_count = 0;

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                pid_sub = c->async_subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, ps = std::move(ps)]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                c->async_publish_at_most_once("topic1", "topic1_contents", false, std::move(ps));
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                c->async_disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c, &pid_unsub, &user_prop_count, prop_size]
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents,
             std::vector<mqtt::v5::property_variant> props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");

                BOOST_TEST(props.size() == prop_size);

                for (auto const& p : props) {
                    mqtt::visit(
                        mqtt::make_lambda_visitor<void>(
                            [&](mqtt::v5::property::payload_format_indicator::recv const& t) {
                                BOOST_TEST(t.val() == mqtt::v5::property::payload_format_indicator::string);
                            },
                            [&](mqtt::v5::property::message_expiry_interval::recv const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](mqtt::v5::property::topic_alias::recv const& t) {
                                BOOST_TEST(t.val() == 0x1234U);
                            },
                            [&](mqtt::v5::property::response_topic::recv const& t) {
                                BOOST_TEST(t.val() == "response topic");
                            },
                            [&](mqtt::v5::property::correlation_data::recv const& t) {
                                BOOST_TEST(t.val() == "correlation data");
                            },
                            [&](mqtt::v5::property::user_property::recv const& t) {
                                switch (user_prop_count++) {
                                case 0:
                                    BOOST_TEST(t.key() == "key1");
                                    BOOST_TEST(t.val() == "val1");
                                    break;
                                case 1:
                                    BOOST_TEST(t.key() == "key2");
                                    BOOST_TEST(t.val() == "val2");
                                    break;
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](mqtt::v5::property::subscription_identifier const& t) {
                                BOOST_TEST(t.val() == 123U);
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                pid_unsub = c->async_unsubscribe("topic1");
                return true;
            });
        c->set_v5_puback_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_v5_pubrec_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_v5_pubcomp_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
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
        c->set_pub_res_sent_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( puback_props ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        if (c->get_protocol_version() != mqtt::protocol_version::v5) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);
        c->set_auto_pub_response(false);

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
            deps("h_puback", "h_publish"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        std::vector<mqtt::v5::property_variant> pubackps {
            mqtt::v5::property::reason_string("test success"),
            mqtt::v5::property::user_property("key1", "val1"),
            mqtt::v5::property::user_property("key2", "val2"),
        };
        std::size_t puback_user_prop_count = 0;

        b.set_puback_props_handler(
            [&puback_user_prop_count, size = pubackps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    mqtt::visit(
                        mqtt::make_lambda_visitor<void>(
                            [&](mqtt::v5::property::reason_string::recv const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](mqtt::v5::property::user_property::recv const& t) {
                                switch (puback_user_prop_count++) {
                                case 0:
                                    BOOST_TEST(t.key() == "key1");
                                    BOOST_TEST(t.val() == "val1");
                                    break;
                                case 1:
                                    BOOST_TEST(t.key() == "key2");
                                    BOOST_TEST(t.val() == "val2");
                                    break;
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        mqtt::optional<std::uint16_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &c, &pid_unsub, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
                pid_unsub = c->async_unsubscribe("topic1");
            });

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                pid_sub = c->async_subscribe("topic1", mqtt::qos::at_least_once);
                return true;
            });
        c->set_v5_puback_handler(
            [&chk, &pid_pub]
            (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_puback");
                BOOST_TEST(packet_id == pid_pub);
                return true;
            });
        c->set_v5_pubrec_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_v5_pubcomp_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, &pid_pub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_1);
                pid_pub = c->async_publish_at_least_once("topic1", "topic1_contents");
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                c->async_disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c, &recv_packet_id, pubackps = std::move(pubackps)]
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents,
             std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_TEST(*packet_id != 0);
                recv_packet_id = packet_id;
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                // pubackps doesn't contain *_ref property, so you don't need to care the lifetime of pubackprops
                c->async_puback(*packet_id, mqtt::v5::reason_code::success, std::move(pubackps));
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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pubrec_rel_comp_prop ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        if (c->get_protocol_version() != mqtt::protocol_version::v5) return;

        // client               broker
        //       publish ------>
        //       <------  pubrec
        //    *  pubrel  ------>
        //       <------ pubcomp
        //
        //       <------ publish
        //    *  pubrec  ------>
        //       <------  pubrel
        //    *  pubcomp ------>
        //
        //    * test target

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);
        c->set_auto_pub_response(false);

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
            deps("h_pubcomp", "h_pubrec"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        std::vector<mqtt::v5::property_variant> pubrecps {
            mqtt::v5::property::reason_string("test success"),
            mqtt::v5::property::user_property("key1", "val1"),
            mqtt::v5::property::user_property("key2", "val2"),
        };
        std::size_t pubrec_user_prop_count = 0;

        std::vector<mqtt::v5::property_variant> pubrelps {
            mqtt::v5::property::reason_string("test success"),
            mqtt::v5::property::user_property("key1", "val1"),
            mqtt::v5::property::user_property("key2", "val2"),
        };
        std::size_t pubrel_user_prop_count = 0;

        std::vector<mqtt::v5::property_variant> pubcompps {
            mqtt::v5::property::reason_string("test success"),
            mqtt::v5::property::user_property("key1", "val1"),
            mqtt::v5::property::user_property("key2", "val2"),
        };
        std::size_t pubcomp_user_prop_count = 0;

        b.set_pubrec_props_handler(
            [&pubrec_user_prop_count, size = pubrecps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    mqtt::visit(
                        mqtt::make_lambda_visitor<void>(
                            [&](mqtt::v5::property::reason_string::recv const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](mqtt::v5::property::user_property::recv const& t) {
                                switch (pubrec_user_prop_count++) {
                                case 0:
                                    BOOST_TEST(t.key() == "key1");
                                    BOOST_TEST(t.val() == "val1");
                                    break;
                                case 1:
                                    BOOST_TEST(t.key() == "key2");
                                    BOOST_TEST(t.val() == "val2");
                                    break;
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        b.set_pubrel_props_handler(
            [&pubrel_user_prop_count, size = pubrelps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    mqtt::visit(
                        mqtt::make_lambda_visitor<void>(
                            [&](mqtt::v5::property::reason_string::recv const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](mqtt::v5::property::user_property::recv const& t) {
                                switch (pubrel_user_prop_count++) {
                                case 0:
                                    BOOST_TEST(t.key() == "key1");
                                    BOOST_TEST(t.val() == "val1");
                                    break;
                                case 1:
                                    BOOST_TEST(t.key() == "key2");
                                    BOOST_TEST(t.val() == "val2");
                                    break;
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        b.set_pubcomp_props_handler(
            [&pubcomp_user_prop_count, size = pubcompps.size()] (std::vector<mqtt::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    mqtt::visit(
                        mqtt::make_lambda_visitor<void>(
                            [&](mqtt::v5::property::reason_string::recv const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](mqtt::v5::property::user_property::recv const& t) {
                                switch (pubcomp_user_prop_count++) {
                                case 0:
                                    BOOST_TEST(t.key() == "key1");
                                    BOOST_TEST(t.val() == "val1");
                                    break;
                                case 1:
                                    BOOST_TEST(t.key() == "key2");
                                    BOOST_TEST(t.val() == "val2");
                                    break;
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        mqtt::optional<std::uint16_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
            });

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                pid_sub = c->async_subscribe("topic1", mqtt::qos::exactly_once);
                return true;
            });
        c->set_v5_puback_handler(
            []
            (packet_id_t, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                BOOST_CHECK(false);
                return true;
            });
        c->set_v5_pubrec_handler(
            [&chk, &c, &pid_pub, pubrelps = std::move(pubrelps)]
            (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_pubrec");
                BOOST_TEST(packet_id == pid_pub);
                c->async_pubrel(packet_id, mqtt::v5::reason_code::success, std::move(pubrelps));
                return true;
            });
        c->set_v5_pubcomp_handler(
            [&chk, &c, &pid_pub, &pid_unsub]
            (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_pubcomp");
                BOOST_TEST(packet_id == pid_pub);
                pid_unsub = c->async_unsubscribe("topic1");
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, &pid_pub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_2);
                pid_pub = c->async_publish_exactly_once("topic1", "topic1_contents");
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                c->async_disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c, &recv_packet_id, pubrecps = std::move(pubrecps)]
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents,
             std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::exactly_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_TEST(*packet_id != 0);
                recv_packet_id = packet_id;
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                c->async_pubrec(*packet_id, mqtt::v5::reason_code::success, std::move(pubrecps));
                return true;
            });
        c->set_v5_pubrel_handler(
            [&c, pubcompps = std::move(pubcompps)]
            (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                c->async_pubcomp(packet_id, mqtt::v5::reason_code::success, std::move(pubcompps));
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
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_SUITE_END()

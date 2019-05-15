// Copyright Takatoshi Kondo 2018
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

BOOST_AUTO_TEST_SUITE(test_as_buffer_async_pubsub_2)

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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    c->async_publish_at_most_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        false,
                        [topic1, contents](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    c->async_publish_at_most_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        false,
                        [topic1, contents](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_pub, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    pid_pub = c->async_publish_at_least_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_pub, &pid_unsub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == pid_pub);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    pid_pub = c->async_publish_at_least_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    pid_pub = c->async_publish_exactly_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::exactly_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub, &pid_pub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_2);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    pid_pub = c->async_publish_exactly_once(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::at_most_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    c->async_publish(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {},
                        mqtt::qos::at_most_once
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::at_most_once,
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    c->async_publish(
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {},
                        mqtt::qos::at_most_once
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::at_least_once,
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    auto ret = c->async_publish_dup(
                        1,
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {},
                        mqtt::qos::at_least_once
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_sub = c->async_subscribe(
                        as::buffer(*topic1),
                        mqtt::qos::at_least_once,
                        [topic1](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    pid_unsub = c->async_unsubscribe(
                        as::buffer(*topic1),
                        [topic1](boost::system::error_code const&) {}
                    );
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
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto contents = std::make_shared<std::string>("topic1_contents");
                    auto ret = c->async_publish_dup(
                        1,
                        as::buffer(*topic1),
                        as::buffer(*contents),
                        [topic1, contents] {},
                        mqtt::qos::at_least_once
                    );
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

BOOST_AUTO_TEST_SUITE_END()

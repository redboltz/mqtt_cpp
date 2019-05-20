// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_retain)

BOOST_AUTO_TEST_CASE( simple ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 QoS0 retain
            // subscribe topic1 QoS0
            cont("h_suback"),
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

                    c->publish_at_most_once("topic1", "retained_contents", true);

                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                [&chk, &pid_sub]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "retained_contents");
                    pid_unsub = c->unsubscribe("topic1");
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

                    c->publish_at_most_once("topic1", "retained_contents", true);

                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                [&chk, &pid_sub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->disconnect();
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
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "retained_contents");
                    pid_unsub = c->unsubscribe("topic1");
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
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( overwrite ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 QoS0 retain
            // publish topic1 QoS0 retain
            // publish topic1 QoS0
            // subscribe topic1 QoS0
            cont("h_suback"),
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

                    c->publish_at_most_once("topic1", "retained_contents1", true);
                    c->publish_at_most_once("topic1", "retained_contents2", true);
                    c->publish_at_most_once("topic1", "retained_contents3", false);

                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                [&chk, &pid_sub]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "retained_contents2");
                    pid_unsub = c->unsubscribe("topic1");
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

                    c->publish_at_most_once("topic1", "retained_contents1", true);
                    c->publish_at_most_once("topic1", "retained_contents2", true);
                    c->publish_at_most_once("topic1", "retained_contents3", false);

                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                [&chk, &pid_sub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->disconnect();
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
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "retained_contents2");
                    pid_unsub = c->unsubscribe("topic1");
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
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( retain_and_publish ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback1"),
            // publish topic1 QoS0 retain
            cont("h_publish1"),
            // unsubscribe topic1
            cont("h_unsuback1"),
            // subscribe topic1 QoS0
            cont("h_suback2"),
            cont("h_publish2"),
            // unsubscribe topic1
            cont("h_unsuback2"),
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
                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                    auto ret = chk.match(
                        "h_connack",
                        [&] {
                            MQTT_CHK("h_suback1");
                            c->publish_at_most_once("topic1", "topic1_contents", true);
                        },
                        "h_unsuback1",
                        [&] {
                            MQTT_CHK("h_suback2");
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_sub, &pid_unsub]
                (packet_id_t packet_id) {
                    BOOST_TEST(packet_id == pid_unsub);
                    auto ret = chk.match(
                        "h_publish1",
                        [&] {
                            MQTT_CHK("h_unsuback1");
                            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                        },
                        "h_publish2",
                        [&] {
                            MQTT_CHK("h_unsuback2");
                            c->disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_publish_handler(
                [&chk, &c, &pid_unsub]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents) {
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1");
                    auto ret = chk.match(
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(mqtt::publish::is_retain(header) == false);
                        },
                        "h_suback2",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(mqtt::publish::is_retain(header) == true);
                        }
                    );
                    BOOST_TEST(ret);
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
                    pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                    auto ret = chk.match(
                        "h_connack",
                        [&] {
                            MQTT_CHK("h_suback1");
                            c->publish_at_most_once("topic1", "topic1_contents", true);
                        },
                        "h_unsuback1",
                        [&] {
                            MQTT_CHK("h_suback2");
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_sub, &pid_unsub]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    auto ret = chk.match(
                        "h_publish1",
                        [&] {
                            MQTT_CHK("h_unsuback1");
                            pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                        },
                        "h_publish2",
                        [&] {
                            MQTT_CHK("h_unsuback2");
                            c->disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c, &pid_unsub]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1");
                    auto ret = chk.match(
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(mqtt::publish::is_retain(header) == false);
                        },
                        "h_suback2",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(mqtt::publish::is_retain(header) == true);
                        }
                    );
                    BOOST_TEST(ret);
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
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( prop ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v5) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 QoS0 retain
            // subscribe topic1 QoS0
            cont("h_suback"),
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
            [&chk, &c, &pid_sub, ps = std::move(ps)]
            (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

                c->publish_at_most_once("topic1", "retained_contents", true, std::move(ps));

                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
            [&chk, &pid_sub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                c->disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c, &pid_unsub, prop_size, &user_prop_count]
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents,
             std::vector<mqtt::v5::property_variant> props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents");

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
                            [&](mqtt::v5::property::subscription_identifier::recv const& t) {
                                BOOST_TEST(t.val() == 123U);
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                pid_unsub = c->unsubscribe("topic1");
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
    do_combi_test_sync(test);
}


BOOST_AUTO_TEST_SUITE_END()

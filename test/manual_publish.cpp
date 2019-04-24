// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_manual_publish)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);


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

        switch (c->get_protocol_version()) {
        case mqtt::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
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
            c->set_puback_handler(
                [&chk, &c]
                (packet_id_t packet_id) {
                    BOOST_TEST(packet_id == 1);
                    MQTT_CHK("h_puback");
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
                [&chk, &c]
                (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                    MQTT_CHK("h_suback");
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
                [&chk, &c]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == 1);
                    c->disconnect();
                    return true;
                });
            c->set_publish_handler(
                [&chk]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents) {
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    MQTT_CHK("h_publish");
                    return true;
                });
            break;
        case mqtt::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
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
            c->set_v5_puback_handler(
                [&chk, &c]
                (packet_id_t packet_id, std::uint8_t, std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(packet_id == 1);
                    MQTT_CHK("h_puback");
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
                [&chk, &c]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == 1);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
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
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == 1);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
                    c->disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    BOOST_TEST(mqtt::publish::is_dup(header) == false);
                    BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    MQTT_CHK("h_publish");
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

BOOST_AUTO_TEST_SUITE_END()

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
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->subscribe(1, "topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_puback_handler(
                [&chk, &c]
                (packet_id_t packet_id) {
                    BOOST_TEST(packet_id == 1);
                    MQTT_CHK("h_puback");
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->unsubscribe(1, "topic1");
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
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == 1);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->publish(1, "topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
                (bool is_dup,
                 MQTT_NS::qos qos_value,
                 bool is_retain,
                 MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    BOOST_TEST(is_dup == false);
                    BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(is_retain == false);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    MQTT_CHK("h_publish");
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->subscribe(1, "topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(packet_id == 1);
                    MQTT_CHK("h_puback");
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->unsubscribe(1, "topic1");
                    return true;
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == 1);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    BOOST_TEST(c->register_packet_id(0) == false);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    BOOST_TEST(c->register_packet_id(1) == false);
                    c->publish(1, "topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == 1);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk]
                (bool is_dup,
                 MQTT_NS::qos qos_value,
                 bool is_retain,
                 MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(is_dup == false);
                    BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(is_retain == false);
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
            [&chk, &finish]
            () {
                MQTT_CHK("h_close");
                finish();
            });
        c->set_error_handler(
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "ordered_caller.hpp"
#include "test_util.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(st_receive_maximum)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( sync ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);
        c->set_maximum_packet_size_recv(50);

        b.set_connack_props(
            MQTT_NS::v5::properties {
                MQTT_NS::v5::property::receive_maximum(2)
            }
        );
        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS2
            cont("h_suback"),
            // publish topic1 100bytes
            cont("publish1"),
            cont("publish2"),
            cont("publish3"),
            cont("h_publish1"),
            cont("h_publish2"),
            cont("h_publish3"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::exactly_once);
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_2);
                MQTT_CHK("publish1");
                c->publish(
                    "topic1",
                    "message1",
                    MQTT_NS::qos::at_least_once
                );
                MQTT_CHK("publish2");
                c->publish(
                    "topic1",
                    "message2",
                    MQTT_NS::qos::at_least_once
                );
                MQTT_CHK("publish3");
                c->publish(
                    "topic1",
                    "message3",
                    MQTT_NS::qos::at_least_once
                );
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_publish1");
                        BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                        BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        BOOST_TEST(packet_id.has_value());
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == "message1");
                    },
                    [&] {
                        MQTT_CHK("h_publish2");
                        BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                        BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        BOOST_TEST(packet_id.has_value());
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == "message2");
                    },
                    [&] {
                        MQTT_CHK("h_publish3");
                        BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                        BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        BOOST_TEST(packet_id.has_value());
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == "message3");
                        c->disconnect();
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_v5_puback_handler(
            []
            (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                return true;
            });

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

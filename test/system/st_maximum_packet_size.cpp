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

BOOST_AUTO_TEST_SUITE(st_maximum_packet_size)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( sync ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        std::size_t header_size =
            1 +      // fixed header
            1 +      // remaining length (0..127)
            2 + 6 +  // topic name length + topic name(topic1)
            1;       // property length

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);
        c->set_maximum_packet_size_recv(50);

        b.set_connack_props(
            MQTT_NS::v5::properties {
                MQTT_NS::v5::property::maximum_packet_size(100)
            }
        );
        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 100bytes
            cont("publish50"),
            cont("publish51"),
            cont("publish100"),
            cont("publish101"),
            cont("publish101_exception"),
            deps("h_publish50", "publish50"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once);
            });
        c->set_v5_suback_handler(
            [&chk, &c, header_size]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                // 50 bytes
                MQTT_CHK("publish50");
                c->publish(
                    "topic1",
                    std::string(50 - header_size, 'a'),
                    MQTT_NS::qos::at_most_once
                );
                // 51 bytes
                MQTT_CHK("publish51");
                c->publish(
                    "topic1",
                    std::string(51 - header_size, 'a'),
                    MQTT_NS::qos::at_most_once
                );
                // 100 bytes
                MQTT_CHK("publish100");
                c->publish(
                    "topic1",
                    std::string(100 - header_size, 'a'),
                    MQTT_NS::qos::at_most_once
                );
                try {
                    // 101 bytes
                    MQTT_CHK("publish101");
                    c->publish(
                        "topic1",
                        std::string(101 - header_size, 'a'),
                        MQTT_NS::qos::at_most_once
                    );
                    BOOST_TEST(false);
                }
                catch (MQTT_NS::packet_size_error const&) {
                    MQTT_CHK("publish101_exception");
                }
            });
        c->set_v5_publish_handler(
            [&chk, &c, header_size]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_publish50");
                        BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                        BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        BOOST_CHECK(!packet_id);
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == std::string(50 - header_size, 'a'));
                        c->disconnect();
                    }
                );
                BOOST_TEST(ret);
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

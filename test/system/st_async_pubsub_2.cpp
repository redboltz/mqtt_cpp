// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

#include <vector>
#include <string>
#include <iostream>

BOOST_AUTO_TEST_SUITE(st_async_pubsub_2)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos2_protocol_error_resend_pubrec ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        std::cout << "cout" << std::endl;
        std::cerr << "cerr" << std::endl;
        std::clog << "clog" << std::endl;
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);
        c->set_auto_pub_response(false);

        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 QoS2
            deps("h_publish", "h_suback"),
            // pubrec send twice
            cont("h_pubrel1"),
            cont("h_pubrel2"),
            deps("h_unsuback", "h_pubcomp", "h_pubrel2"),
            // disconnect
            cont("h_close"),
        };

        auto g = MQTT_NS::shared_scope_guard(
            [&c] {
                auto unsub_pid = c->acquire_unique_packet_id();
                c->async_unsubscribe(unsub_pid, "topic1");
            }
        );

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    auto pid_sub = c->acquire_unique_packet_id();
                    c->async_subscribe(pid_sub, "topic1", MQTT_NS::qos::exactly_once);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
                    auto pid_pub = c->acquire_unique_packet_id();
                    c->async_publish(pid_pub, "topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            c->set_publish_handler(
                [&chk, &c]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::exactly_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(*packet_id != 0);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    // send pubrec twice
                    c->async_pubrec(
                        *packet_id,
                        MQTT_NS::v5::pubrec_reason_code::success
                        [&c]
                        (MQTT_NS::error_code) {
                            c->async_pubrec(*packet_id);
                        }            
                    );
                    return true;
                });
            c->set_pubrel_handler(
                [&chk, &c, g]
                (packet_id_t packet_id) mutable {
                    auto ret = chk.match(
                        "h_publish",
                        [&] {
                            MQTT_CHK("h_pubrel1");
                            c->async_pubcomp(packet_id);
                        },
                        "h_pubrel1",
                        [&] () {
                            MQTT_CHK("h_pubrel2");
                            c->async_pubcomp(packet_id);
                            g.reset();
                        }
                    );
                    BOOST_TEST(ret);
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
                    auto pid_sub = c->acquire_unique_packet_id();
                    c->async_subscribe(pid_sub, "topic1", MQTT_NS::qos::exactly_once);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_2);
                    auto pid_pub = c->acquire_unique_packet_id();
                    c->async_publish(pid_pub, "topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::exactly_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(*packet_id != 0);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    // send pubrec twice
                    c->async_pubrec(*packet_id);
                    c->async_pubrec(*packet_id);
                    return true;
                });
            c->set_v5_pubrel_handler(
                [&chk, &c, g]
                (packet_id_t packet_id, MQTT_NS::v5::pubrel_reason_code, MQTT_NS::v5::properties /*props*/) mutable {
                    auto ret = chk.match(
                        "h_publish",
                        [&] {
                            MQTT_CHK("h_pubrel1");
                            c->async_pubcomp(packet_id);
                        },
                        "h_pubrel1",
                        [&] () {
                            MQTT_CHK("h_pubrel2");
                            c->async_pubcomp(packet_id);
                            g.reset();
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
        g.reset();
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_SUITE_END()

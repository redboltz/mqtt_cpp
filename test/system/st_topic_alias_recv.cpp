// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "test_util.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(st_topic_alias_recv)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( pubsub ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 alias1 QoS0
            // publish alias1 QoS0
            cont("h_publsh1"),
            cont("h_publish2"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
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
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    // register topic alias
                    c->publish(
                        "topic1",
                        "topic1_contents_1",
                        MQTT_NS::qos::at_most_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );
                    // use topic alias
                    c->publish(
                        "",
                        "topic1_contents_2",
                        MQTT_NS::qos::at_most_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_suback",
                        [&] {
                            MQTT_CHK("h_publsh1");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_1");
                        },
                        "h_publsh1",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_2");
                            c->unsubscribe("topic1");
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
        c->set_pub_res_sent_handler(
            []
            (packet_id_t) {
                BOOST_CHECK(false);
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( overwrite ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback1"),
            cont("h_suback2"),
            // publish topic1 alias1 QoS0
            // publish topic2 alias1 QoS0
            // publish alias1 QoS0
            cont("h_publish1"),
            cont("h_publish2"),
            cont("h_publish3"),
            cont("h_unsuback1"),
            cont("h_unsuback2"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    c->subscribe("topic2", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
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
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_connack",
                        [&] {
                            MQTT_CHK("h_suback1");
                        },
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_suback2");
                            BOOST_TEST(reasons.size() == 1U);
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                            // register topic alias
                            c->publish(
                                "topic1",
                                "topic1_contents_1",
                                MQTT_NS::qos::at_most_once,
                                MQTT_NS::v5::properties {
                                    MQTT_NS::v5::property::topic_alias(0x1U)
                                }
                            );
                            // overwrite topic alias
                            c->publish(
                                "topic2",
                                "topic1_contents_2",
                                MQTT_NS::qos::at_most_once,
                                MQTT_NS::v5::properties {
                                    MQTT_NS::v5::property::topic_alias(0x1U)
                                        }
                            );
                            // use topic alias
                            c->publish(
                                "",
                                "topic1_contents_3",
                                MQTT_NS::qos::at_most_once,
                                MQTT_NS::v5::properties {
                                    MQTT_NS::v5::property::topic_alias(0x1U)
                                }
                            );
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_publish3",
                        [&] {
                            MQTT_CHK("h_unsuback1");
                            BOOST_TEST(reasons.size() == 1U);
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                        },
                        "h_unsuback1",
                        [&] {
                            MQTT_CHK("h_unsuback2");
                            BOOST_TEST(reasons.size() == 1U);
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                            c->disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_1");
                        },
                        "h_publish1",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic2");
                            BOOST_TEST(contents == "topic1_contents_2");
                        },
                        "h_publish2",
                        [&] {
                            MQTT_CHK("h_publish3");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic2");
                            BOOST_TEST(contents == "topic1_contents_3");
                            c->unsubscribe("topic1");
                            c->unsubscribe("topic2");
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
        c->set_pub_res_sent_handler(
            []
            (packet_id_t) {
                BOOST_CHECK(false);
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( no_entry ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic_alias1 QoS0
            cont("h_suback"),
            // publish  QoS0
            cont("h_disconnect"),
            // disconnect
            cont("h_error"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
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
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    // use no existing topic alias
                    c->publish(
                        "",
                        "topic1_contents",
                        MQTT_NS::qos::at_most_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );
                    return true;
                });
            c->set_v5_unsuback_handler(
                []
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_publish_handler(
                []
                (MQTT_NS::optional<packet_id_t> /*packet_id*/,
                 MQTT_NS::publish_options /*pubopts*/,
                 MQTT_NS::buffer /*topic*/,
                 MQTT_NS::buffer /*contents*/,
                 MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_v5_disconnect_handler(
                [&chk]
                (MQTT_NS::v5::disconnect_reason_code reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_disconnect");
                    BOOST_TEST(reason_code == MQTT_NS::v5::disconnect_reason_code::protocol_error);
                }
            );
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&chk, &finish]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                finish();
            });
        c->set_pub_res_sent_handler(
            []
            (packet_id_t) {
                BOOST_CHECK(false);
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( resend_publish ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {

        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);
        c->set_auto_pub_response(false);

        boost::asio::steady_timer tim(ioc);

        checker chk = {
            cont("start"),
            // connect
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),
            // connect
            cont("h_connack2"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 alias1 QoS0
            // publish alias1 QoS1
            deps("h_error", "h_suback"),
            cont("h_connack3"),
            cont("h_puback"),
            deps("h_publish_resend_by_broker", "h_connack3"),
            cont("h_publish_resend_by_client"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close2"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "start",
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        "h_close1",
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                            c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                        },
                        "h_error",
                        [&] {
                            MQTT_CHK("h_connack3");
                            BOOST_TEST(sp == true);
                            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            c->set_v5_puback_handler(
                [&chk, &c]
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_puback");
                    c->disconnect();
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
                [&chk, &c, &tim]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_1);
                    // register topic alias
                    c->publish(
                        "topic1",
                        "topic1_contents_1",
                        MQTT_NS::qos::at_most_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );
                    // use topic alias
                    c->publish(
                        "",
                        "topic1_contents_2",
                        MQTT_NS::qos::at_least_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );

                    // TLS and WS combination seems to take a long time.
                    // publish() only calls sync APIs but the message is not
                    // transfered to the socket.
                    // So force_disconnect close the socket before the message has been written.
                    // It's weird but the following wait solve the problem.
                    tim.expires_after(std::chrono::milliseconds(100));
                    tim.wait();
                    // 1st publish is lost because QoS0 but topic_alias is registered
                    // 2nd publish will be resent in the future.
                    // However, it contains empty topic and topic_alias.
                    // mqtt_cpp keeps topic_alias map's life time as the same as session lifetime
                    // so resend would be successfully finished.
                    // See https://lists.oasis-open.org/archives/mqtt-comment/202009/msg00000.html
                    c->force_disconnect();

                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
                    return true;
                });
            c->set_v5_publish_handler(
                [&chk, &c]
                (MQTT_NS::optional<packet_id_t>,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    auto ret = chk.match(
                        "h_connack3",
                        [&] {
                            MQTT_CHK("h_publish_resend_by_broker");
                            // dup is set by broker
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::yes);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_2");
                        },
                        "h_publish_resend_by_broker",
                        [&] {
                            MQTT_CHK("h_publish_resend_by_client");
                            // client --dup--> broker --no dup--> client
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_2");
                            c->unsubscribe("topic1");
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
            [&chk, &c, &finish]
            () {
                auto ret = chk.match(
                    "h_connack1",
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
                    "h_puback",
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&chk, &c, &tim]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                // TCP level disconnection detecting timing is unpredictable.
                // Sometimes broker first, sometimes the client (this test) first.
                // This test assume that the broker detects first, so I set timer.
                // If client detect the disconnection first, then reconnect with
                // existing client id. And it is overwritten at broker.
                // Then error handler in the broker called, assertion failed due to
                // no corresponding connection exists
                tim.expires_after(std::chrono::milliseconds(100));
                tim.async_wait(
                    [&c] (MQTT_NS::error_code ec) {
                        BOOST_ASSERT( ! ec);
                        connect_no_clean(c);
                    }
                );
            });
        MQTT_CHK("start");
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}


BOOST_AUTO_TEST_SUITE_END()

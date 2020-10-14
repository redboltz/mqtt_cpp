// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_retain_2)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( retain_and_publish ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    auto ret = chk.match(
                        "h_connack",
                        [&] {
                            MQTT_CHK("h_suback1");
                            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
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
                            pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1");
                    auto ret = chk.match(
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        },
                        "h_suback2",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    auto ret = chk.match(
                        "h_connack",
                        [&] {
                            MQTT_CHK("h_suback1");
                            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
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
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    auto ret = chk.match(
                        "h_publish1",
                        [&] {
                            MQTT_CHK("h_unsuback1");
                            pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1");
                    auto ret = chk.match(
                        "h_suback1",
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                        },
                        "h_suback2",
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( retain_rap ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
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

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::rap::retain);
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
            [&chk, &c, &pid_sub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback1");
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
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
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                auto ret = chk.match(
                    "h_publish1",
                    [&] {
                        MQTT_CHK("h_unsuback1");
                        pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::rap::retain);
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
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                pid_unsub = c->unsubscribe("topic1");
                auto ret = chk.match(
                    "h_suback1",
                    [&] {
                        MQTT_CHK("h_publish1");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_publish2");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                    }
                );
                BOOST_TEST(ret);
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

BOOST_AUTO_TEST_CASE( retain_rh_send ) {
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
            // publish topic1 QoS0 retain
            cont("h_publish1"),
            // unsubscribe topic1
            cont("h_unsuback1"),
            // subscribe topic1 QoS0 rh:send_only_new_subscription
            cont("h_suback2"),
            cont("h_suback3"),
            deps("h_publish2", "h_suback2"),
            cont("h_publish3"),
            // unsubscribe topic1
            cont("h_unsuback2"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::rap::retain);
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
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback1");
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
                    },
                    "h_unsuback1",
                    [&] {
                        MQTT_CHK("h_suback2");
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_suback3");
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c]
            (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                auto ret = chk.match(
                    "h_publish1",
                    [&] {
                        MQTT_CHK("h_unsuback1");
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::send
                        );
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::send
                        );
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
            [&chk, &c]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                auto ret = chk.match(
                    "h_suback1",
                    [&] {
                        MQTT_CHK("h_publish1");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                        c->unsubscribe("topic1");
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_publish2");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                    },
                    "h_publish2",
                    [&] {
                        MQTT_CHK("h_publish3");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                        c->unsubscribe("topic1");
                    }
                );
                BOOST_TEST(ret);
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

BOOST_AUTO_TEST_CASE( retain_rh_only_newsub ) {
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
            // publish topic1 QoS0 retain
            cont("h_publish1"),
            // unsubscribe topic1
            cont("h_unsuback1"),
            // subscribe topic1 QoS0 rh:send_only_new_subscription
            cont("h_suback2"),
            cont("h_suback3"),
            deps("h_publish2", "h_suback2"),
            // unsubscribe topic1
            cont("h_unsuback2"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::rap::retain);
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
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback1");
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
                    },
                    "h_unsuback1",
                    [&] {
                        MQTT_CHK("h_suback2");
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_suback3");
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c]
            (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                auto ret = chk.match(
                    "h_publish1",
                    [&] {
                        MQTT_CHK("h_unsuback1");
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::send_only_new_subscription
                        );
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::send_only_new_subscription
                        );
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
            [&chk, &c]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                c->unsubscribe("topic1");
                auto ret = chk.match(
                    "h_suback1",
                    [&] {
                        MQTT_CHK("h_publish1");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_publish2");
                        BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                    }
                );
                BOOST_TEST(ret);
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

BOOST_AUTO_TEST_CASE( retain_rh_not_send ) {
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
            // publish topic1 QoS0 retain
            cont("h_publish1"),
            // unsubscribe topic1
            cont("h_unsuback1"),
            // subscribe topic1 QoS0 rh:send_only_new_subscription
            cont("h_suback2"),
            cont("h_suback3"),
            // unsubscribe topic1
            cont("h_unsuback2"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::rap::retain);
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
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback1");
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);
                    },
                    "h_unsuback1",
                    [&] {
                        MQTT_CHK("h_suback2");
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_suback3");
                        c->unsubscribe("topic1");
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c]
            (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                auto ret = chk.match(
                    "h_publish1",
                    [&] {
                        MQTT_CHK("h_unsuback1");
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::not_send
                        );
                        c->subscribe(
                            "topic1",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain_handling::not_send
                        );
                    },
                    "h_unsuback1",
                    [&] {
                        MQTT_CHK("h_unsuback2");
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
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                c->unsubscribe("topic1");
                MQTT_CHK("h_publish1");
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
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

BOOST_AUTO_TEST_CASE( prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
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

        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
            MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
            MQTT_NS::v5::property::topic_alias(0x1234U),
            MQTT_NS::v5::property::response_topic("response topic"_mb),
            MQTT_NS::v5::property::correlation_data("correlation data"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };

        auto prop_size = ps.size();
        std::size_t user_prop_count = 0;

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub, ps = MQTT_NS::force_move(ps)]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) mutable {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);

                c->publish("topic1", "retained_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes, MQTT_NS::force_move(ps));

                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
            [&chk, &pid_sub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                c->disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&chk, &c, &pid_unsub, prop_size, &user_prop_count]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents");

                // -1 means TopicAlias
                // TopicAlias is not forwarded
                // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901113
                // A receiver MUST NOT carry forward any Topic Alias mappings from
                // one Network Connection to another [MQTT-3.3.2-7].
                BOOST_TEST(props.size() == prop_size - 1);

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::payload_format_indicator const& t) {
                                BOOST_TEST(t.val() == MQTT_NS::v5::property::payload_format_indicator::string);
                            },
                            [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::response_topic const& t) {
                                BOOST_TEST(t.val() == "response topic");
                            },
                            [&](MQTT_NS::v5::property::correlation_data const& t) {
                                BOOST_TEST(t.val() == "correlation data");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
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
                            [&](MQTT_NS::v5::property::subscription_identifier const& t) {
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

BOOST_AUTO_TEST_CASE( sid ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
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

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);

                c->publish("topic1", "retained_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes);

                pid_sub = c->subscribe(
                    "topic1", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(123) }
                );
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
            [&chk, &pid_sub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                c->disconnect();
                return true;
            });
        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents");

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::subscription_identifier const& t) {
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

// Copyright Takatoshi Kondo 2020
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

BOOST_AUTO_TEST_SUITE(st_topic_alias)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( pubsub ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();

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
            cont("h_publish1"),
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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_1");
                        },
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
        c->connect(
            MQTT_NS::v5::properties {
                MQTT_NS::v5::property::topic_alias_maximum(10)
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( auto_replace ) {
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
        c->set_auto_replace_topic_alias_send();

        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 alias1 QoS0
            // publish alias1 QoS0
            cont("h_publish1"),
            cont("h_publish2"),
            // disconnect
            cont("h_close"),
        };

        b.set_publish_props_handler(
            [&] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == 1);

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::topic_alias const& t) {
                                MQTT_ORDERED(
                                    [&] {
                                        MQTT_CHK("h_publish1");
                                        BOOST_TEST(t.val() == 0x1U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish2");
                                        BOOST_TEST(t.val() == 0x1U);
                                        c->socket()->post(
                                            [&] {
                                                c->disconnect();
                                            }
                                        );
                                    }
                                );
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

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    // register topic alias
                    c->publish(
                        "topic1",
                        "topic1_contents_1",
                        MQTT_NS::qos::at_most_once,
                        MQTT_NS::v5::properties {
                            MQTT_NS::v5::property::topic_alias(0x1U)
                        }
                    );
                    // use topic alias automatically
                    c->publish(
                        "topic1",
                        "topic1_contents_2",
                        MQTT_NS::qos::at_most_once
                    );
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

BOOST_AUTO_TEST_CASE( auto_map ) {
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
        c->set_auto_map_topic_alias_send();

        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 alias1 QoS0
            // publish alias1 QoS0
            cont("h_publish1"),
            cont("h_publish2"),
            cont("h_publish3"),
            cont("h_publish4"),
            cont("h_publish5"),
            cont("h_publish6"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::topic_alias_maximum(3)
        };

        b.set_connack_props(std::move(ps));

        b.set_publish_props_handler(
            [&] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == 1);

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::topic_alias const& t) {
                                MQTT_ORDERED(
                                    [&] {
                                        MQTT_CHK("h_publish1");
                                        BOOST_TEST(t.val() == 0x1U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish2");
                                        BOOST_TEST(t.val() == 0x2U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish3");
                                        BOOST_TEST(t.val() == 0x3U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish4");
                                        BOOST_TEST(t.val() == 0x1U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish5");
                                        BOOST_TEST(t.val() == 0x3U);
                                    },
                                    [&] {
                                        MQTT_CHK("h_publish6");
                                        BOOST_TEST(t.val() == 0x2U);
                                        c->socket()->post(
                                            [&] {
                                                c->disconnect();
                                            }
                                        );
                                    }
                                );
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


        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    // auto map topic alias
                    c->publish(
                        "topic1",
                        "topic1_contents_1",
                        MQTT_NS::qos::at_most_once
                    );
                    c->publish(
                        "topic2",
                        "topic2_contents_2",
                        MQTT_NS::qos::at_most_once
                    );
                    c->publish(
                        "topic3",
                        "topic3_contents_3",
                        MQTT_NS::qos::at_most_once
                    );
                    c->publish(
                        "topic4",
                        "topic4_contents_4",
                        MQTT_NS::qos::at_most_once
                    );
                    c->publish(
                        "topic3",
                        "topic3_contents_5",
                        MQTT_NS::qos::at_most_once
                    );
                    c->publish(
                        "topic5",
                        "topic5_contents_6",
                        MQTT_NS::qos::at_most_once
                    );
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
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();

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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_suback1");
                        },
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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_unsuback1");
                            BOOST_TEST(reasons.size() == 1U);
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                        },
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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_publish1");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_1");
                        },
                        [&] {
                            MQTT_CHK("h_publish2");
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_CHECK(!packet_id);
                            BOOST_TEST(topic == "topic2");
                            BOOST_TEST(contents == "topic1_contents_2");
                        },
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
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();

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
            cont("h_exception"),
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
                    try {
                        // use no existing topic alias
                        c->publish(
                            "",
                            "topic1_contents",
                            MQTT_NS::qos::at_most_once,
                            MQTT_NS::v5::properties {
                                MQTT_NS::v5::property::topic_alias(0x1U)
                                    }
                        );
                    }
                    catch (MQTT_NS::protocol_error const& e) {
                        MQTT_CHK("h_exception");
                        BOOST_TEST(e.what() == "protocol error");
                        c->disconnect();
                    }
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

BOOST_AUTO_TEST_CASE( resend_publish ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();

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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            c->disconnect();
                        },
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == false);
                            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                            c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                        },
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
                [&chk]
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_puback");
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
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_publish_resend_by_broker");
                            // dup is set by broker
                            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::yes);
                            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                            BOOST_TEST(topic == "topic1");
                            BOOST_TEST(contents == "topic1_contents_2");
                        },
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
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_close1");
                        connect_no_clean(c);
                    },
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

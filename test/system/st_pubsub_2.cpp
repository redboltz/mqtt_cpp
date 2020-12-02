// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(st_pubsub_2)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( pub_sub_wc_plus ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        packet_id_t pid_sub;
        packet_id_t pid_unsub;

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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    pid_sub = c->subscribe("a/+/b", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
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
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "a/topic1/b");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("a/+/b");
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
                    pid_sub = c->subscribe("a/+/b", MQTT_NS::qos::at_most_once);
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
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
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
                [&chk, &c, &pid_unsub]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "a/topic1/b");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("a/+/b");
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

BOOST_AUTO_TEST_CASE( pub_sub_wc_sharp ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        packet_id_t pid_sub;
        packet_id_t pid_unsub;

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
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    pid_sub = c->subscribe("a/#", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
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
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "a/topic1/b");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("a/#");
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
                    pid_sub = c->subscribe("a/#", MQTT_NS::qos::at_most_once);
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
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
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
                [&chk, &c, &pid_unsub]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "a/topic1/b");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("a/#");
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

BOOST_AUTO_TEST_CASE( pub_sub_sid ) {
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
            // subscribe topic1 QoS0 Sid:123
            cont("h_suback"),
            // publish topic1 QoS0
            cont("h_publish"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe(
                    "topic1", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(123) }
                );
                return true;
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
                return true;
            });
        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
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
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&]
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

BOOST_AUTO_TEST_CASE( pub_sub_sid_ow ) {
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
            // subscribe topic1 QoS0 Sid:123
            cont("h_suback_1"),
            // subscribe topic1 QoS0 Sid:456
            cont("h_suback_2"),
            // publish topic1 QoS0
            cont("h_publish"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe(
                    "topic1", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(123) }
                );
                c->subscribe(
                    "topic1", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(456) }
                );
                return true;
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback_1");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    },
                    "h_suback_1",
                    [&] {
                        MQTT_CHK("h_suback_2");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::subscription_identifier const& t) {
                                BOOST_TEST(t.val() == 456U);
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&]
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

BOOST_AUTO_TEST_CASE( pub_sub_sid_multi_match ) {
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
            // subscribe a/b QoS0 Sid:123
            cont("h_suback_1"),
            // subscribe a/# QoS0 Sid:456
            cont("h_suback_2"),
            // publish a/b QoS0
            cont("h_publish_1"),
            cont("h_publish_2"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe(
                    "a/b", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(123) }
                );
                c->subscribe(
                    "a/#", MQTT_NS::qos::at_most_once,
                    MQTT_NS::v5::properties{ MQTT_NS::v5::property::subscription_identifier(456) }
                );
                return true;
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback_1");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    },
                    "h_suback_1",
                    [&] {
                        MQTT_CHK("h_suback_2");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        c->publish("a/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                    }
                );
                BOOST_TEST(ret);
                return true;
            });

        std::set<std::size_t> sids { 123, 456 };

        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                auto ret = chk.match(
                    "h_suback_2",
                    [&] {
                        MQTT_CHK("h_publish_1");
                        BOOST_TEST(topic == "a/b");
                        BOOST_TEST(contents == "topic1_contents");
                        for (auto const& p : props) {
                            MQTT_NS::visit(
                                MQTT_NS::make_lambda_visitor(
                                    [&](MQTT_NS::v5::property::subscription_identifier const& t) {
                                        BOOST_TEST(sids.erase(t.val()) == 1U);
                                    },
                                    [&](auto&& ...) {
                                        BOOST_TEST(false);
                                    }
                                ),
                                p
                            );
                        }
                    },
                    "h_publish_1",
                    [&] {
                        MQTT_CHK("h_publish_2");
                        BOOST_TEST(topic == "a/b");
                        BOOST_TEST(contents == "topic1_contents");
                        for (auto const& p : props) {
                            MQTT_NS::visit(
                                MQTT_NS::make_lambda_visitor(
                                    [&](MQTT_NS::v5::property::subscription_identifier const& t) {
                                        BOOST_TEST(sids.erase(t.val()) == 1U);
                                    },
                                    [&](auto&& ...) {
                                        BOOST_TEST(false);
                                    }
                                ),
                                p
                            );
                        }
                        c->disconnect();
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->set_close_handler(
            [&]
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

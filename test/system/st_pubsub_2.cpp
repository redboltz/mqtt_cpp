// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "ordered_caller.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(st_pubsub_2)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( publish_function_buffer ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    c->publish("topic1"_mb, "topic1_contents"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    c->publish("topic1"_mb, "topic1_contents"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
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

BOOST_AUTO_TEST_CASE( publish_function_buffer_sequence ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    std::vector<MQTT_NS::buffer> bs {
                        "topic"_mb,
                        "1"_mb,
                        "_"_mb,
                        "contents"_mb,
                    };
                    c->publish("topic1"_mb, MQTT_NS::force_move(bs), MQTT_NS::qos::at_most_once);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    std::vector<MQTT_NS::buffer> bs {
                        "topic"_mb,
                        "1"_mb,
                        "_"_mb,
                        "contents"_mb,
                    };
                    c->publish("topic1"_mb, MQTT_NS::force_move(bs), MQTT_NS::qos::at_most_once);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
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

BOOST_AUTO_TEST_CASE( publish_function_const_buffer_sequence ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    auto topic_name = std::make_shared<std::string>("topic1");
                    auto s1 = std::make_shared<std::string>("topic");
                    auto s2 = std::make_shared<std::string>("1");
                    auto s3 = std::make_shared<std::string>("_");
                    auto s4 = std::make_shared<std::string>("contents");
                    std::vector<as::const_buffer> cbs {
                        as::buffer(*s1),
                        as::buffer(*s2),
                        as::buffer(*s3),
                        as::buffer(*s4)
                    };
                    c->publish(
                        as::buffer(*topic_name),
                        cbs,
                        MQTT_NS::qos::at_most_once,
                        std::make_tuple(topic_name, s1, s2, s3, s4)
                    );
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_most_once);
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    auto topic_name = std::make_shared<std::string>("topic1");
                    auto s1 = std::make_shared<std::string>("topic");
                    auto s2 = std::make_shared<std::string>("1");
                    auto s3 = std::make_shared<std::string>("_");
                    auto s4 = std::make_shared<std::string>("contents");
                    std::vector<as::const_buffer> cbs {
                        as::buffer(*s1),
                        as::buffer(*s2),
                        as::buffer(*s3),
                        as::buffer(*s4)
                    };
                    c->publish(
                        as::buffer(*topic_name),
                        cbs,
                        MQTT_NS::qos::at_most_once,
                        std::make_tuple(topic_name, s1, s2, s3, s4)
                    );
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
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
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                    pid_unsub = c->unsubscribe("topic1"_mb);
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

BOOST_AUTO_TEST_CASE( publish_dup_function ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        packet_id_t pid_sub;
        packet_id_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS1
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
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                });
            c->set_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->unsubscribe("topic1");
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    c->publish(1, "topic1", "topic1_contents", MQTT_NS::qos::at_least_once | MQTT_NS::dup::yes);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
                });
            c->set_publish_handler(
                [&chk]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->unsubscribe("topic1");
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_1);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    c->publish(1, "topic1", "topic1_contents", MQTT_NS::qos::at_least_once | MQTT_NS::dup::yes);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
                });
            c->set_v5_publish_handler(
                [&chk]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
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

BOOST_AUTO_TEST_CASE( publish_dup_function_buffer ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        packet_id_t pid_sub;
        packet_id_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS1
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
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_least_once);
                });
            c->set_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->unsubscribe("topic1"_mb);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    c->publish(1, "topic1"_mb, "topic1_contents"_mb, MQTT_NS::qos::at_least_once | MQTT_NS::dup::yes);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
                });
            c->set_publish_handler(
                [&chk]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &pid_sub]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    pid_sub = c->subscribe("topic1"_mb, MQTT_NS::qos::at_least_once);
                });
            c->set_v5_puback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_puback");
                    BOOST_TEST(packet_id == 1);
                    pid_unsub = c->unsubscribe("topic1"_mb);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_1);
                    BOOST_TEST(c->register_packet_id(1) == true);
                    c->publish(1, "topic1"_mb, "topic1_contents"_mb, MQTT_NS::qos::at_least_once | MQTT_NS::dup::yes);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
                });
            c->set_v5_publish_handler(
                [&chk]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_publish");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id.value() == 1);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents");
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

BOOST_AUTO_TEST_CASE( pub_sub_prop ) {
    using namespace std::literals::string_literals;
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
        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
            MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
            MQTT_NS::v5::property::content_type("content type"_mb),
            MQTT_NS::v5::property::topic_alias(0x1234U),
            MQTT_NS::v5::property::response_topic("response topic"_mb),
            MQTT_NS::v5::property::correlation_data("correlation \0data"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };

        auto prop_size = ps.size();
        std::size_t user_prop_count = 0;

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
            });
        c->set_v5_puback_handler(
            []
            (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_pubrec_handler(
            []
            (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_pubcomp_handler(
            []
            (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, ps = MQTT_NS::force_move(ps)]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) mutable {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once | MQTT_NS::retain::no, MQTT_NS::force_move(ps));
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                c->disconnect();
            });
        c->set_v5_publish_handler(
            [&chk, &c, &pid_unsub, &user_prop_count, prop_size]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                MQTT_CHK("h_publish");
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");

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
                            [&](MQTT_NS::v5::property::content_type const& t) {
                                BOOST_TEST(t.val() == "content type");
                            },
                            [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::response_topic const& t) {
                                BOOST_TEST(t.val() == "response topic");
                            },
                            [&](MQTT_NS::v5::property::correlation_data const& t) {
                                BOOST_TEST(t.val() == std::string("correlation \0data"s));
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                pid_unsub = c->unsubscribe("topic1");
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

BOOST_AUTO_TEST_CASE( puback_prop ) {
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

        packet_id_t pid_pub;
        packet_id_t pid_sub;
        packet_id_t pid_unsub;

        bool pub_seq_finished = false;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 QoS1
            cont("h_publish"),
            cont("h_pub_res_sent"),
            cont("h_puback"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties pubackps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto puback_prop_size = pubackps.size();
        b.set_puback_props(MQTT_NS::force_move(pubackps));
        std::size_t puback_user_prop_count = 0;

        MQTT_NS::optional<packet_id_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
            });

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
            });
        c->set_v5_puback_handler(
            [&chk, &c, &pub_seq_finished, &pid_pub, &pid_unsub, puback_prop_size, &puback_user_prop_count]
            (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_puback");
                BOOST_TEST(packet_id == pid_pub);
                pub_seq_finished = true;

                BOOST_TEST(props.size() == puback_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (puback_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                pid_unsub = c->unsubscribe("topic1");
            });
        c->set_v5_pubrec_handler(
            []
            (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_pubcomp_handler(
            []
            (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, &pid_pub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_1);
                pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                c->disconnect();
            });
        c->set_v5_publish_handler(
            [&chk, &recv_packet_id]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_publish");
                BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                BOOST_TEST(*packet_id != 0);
                recv_packet_id = packet_id;
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
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

BOOST_AUTO_TEST_CASE( pubrec_rel_comp_prop ) {
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
        c->set_auto_pub_response(false);

        packet_id_t pid_pub;
        packet_id_t pid_sub;
        packet_id_t pid_unsub;

        bool pub_seq_finished = false;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0
            cont("h_suback"),
            // publish topic1 QoS2
            cont("h_publish"),
            cont("h_pubrec"),
            cont("h_pub_res_sent"),
            cont("h_pubcomp"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties pubrecps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto pubrec_prop_size = pubrecps.size();
        b.set_pubrec_props(MQTT_NS::force_move(pubrecps));
        std::size_t pubrec_user_prop_count = 0;

        MQTT_NS::v5::properties pubrelps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        std::size_t pubrel_user_prop_count = 0;

        MQTT_NS::v5::properties pubcompps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto pubcomp_prop_size = pubcompps.size();
        b.set_pubcomp_props(MQTT_NS::force_move(pubcompps));
        std::size_t pubcomp_user_prop_count = 0;

        b.set_pubrel_props_handler(
            [&pubrel_user_prop_count, size = pubrelps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (pubrel_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        MQTT_NS::optional<packet_id_t> recv_packet_id;
        c->set_pub_res_sent_handler(
            [&chk, &recv_packet_id]
            (packet_id_t packet_id) {
                MQTT_CHK("h_pub_res_sent");
                BOOST_TEST(*recv_packet_id == packet_id);
            });

        c->set_v5_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::exactly_once);
            });
        c->set_v5_puback_handler(
            []
            (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                BOOST_CHECK(false);
            });
        c->set_v5_pubrec_handler(
            [&chk, &c, &pid_pub, pubrec_prop_size, &pubrec_user_prop_count, pubrelps = MQTT_NS::force_move(pubrelps)]
            (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties props) mutable {
                MQTT_CHK("h_pubrec");
                BOOST_TEST(packet_id == pid_pub);

                BOOST_TEST(props.size() == pubrec_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (pubrec_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                c->pubrel(packet_id, MQTT_NS::v5::pubrel_reason_code::success, MQTT_NS::force_move(pubrelps));

            });
        c->set_v5_pubcomp_handler(
            [&chk, &c, &pub_seq_finished, &pid_pub, &pid_unsub, pubcomp_prop_size, &pubcomp_user_prop_count]
            (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_pubcomp");
                BOOST_TEST(packet_id == pid_pub);

                BOOST_TEST(props.size() == pubcomp_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (pubcomp_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                pub_seq_finished = true;
                pid_unsub = c->unsubscribe("topic1");
            });
        c->set_v5_suback_handler(
            [&chk, &c, &pid_sub, &pid_pub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_2);
                pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                c->disconnect();
            });
        c->set_v5_publish_handler(
            [&chk, &c, &recv_packet_id]
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
                recv_packet_id = packet_id;
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                c->pubrec(*packet_id, MQTT_NS::v5::pubrec_reason_code::success, {});
            });
        c->set_v5_pubrel_handler(
            [&c]
            (packet_id_t packet_id, MQTT_NS::v5::pubrel_reason_code, MQTT_NS::v5::properties /*props*/) {
                c->pubcomp(packet_id, MQTT_NS::v5::pubcomp_reason_code::success, {});
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

BOOST_AUTO_TEST_CASE( pub_sub_wc_plus ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
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
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
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
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
                });
            c->set_puback_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                []
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(results.size() == 1U);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                });
            c->set_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    c->disconnect();
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
                });
            c->set_v5_puback_handler(
                []
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                []
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&chk, &c, &pid_sub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(packet_id == pid_sub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    c->publish("a/topic1/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c, &pid_unsub]
                (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(packet_id == pid_unsub);
                    BOOST_TEST(reasons.size() == 1U);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
                    c->disconnect();
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
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_suback_1");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    },
                    [&] {
                        MQTT_CHK("h_suback_2");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
                    }
                );
                BOOST_TEST(ret);
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
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_suback_1");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    },
                    [&] {
                        MQTT_CHK("h_suback_2");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        c->publish("a/b", "topic1_contents", MQTT_NS::qos::at_most_once);
                    }
                );
                BOOST_TEST(ret);
            });

        std::set<std::size_t> sids { 123, 456 };

        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {
                auto ret = MQTT_ORDERED(
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

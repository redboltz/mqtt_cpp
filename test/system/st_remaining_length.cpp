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

BOOST_AUTO_TEST_SUITE(st_remaining_length)

BOOST_AUTO_TEST_CASE( pub_sub_over_127 ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 128; ++i) {
            test_contents.push_back(static_cast<char>(i));
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


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

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
        c->set_puback_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubrec_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubcomp_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&chk, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                c->publish("topic1", test_contents, MQTT_NS::qos::at_most_once);
            });
        c->set_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
            });
        c->set_publish_handler(
            [&chk, &c, &pid_unsub, &test_contents]
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
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_sub_over_16384 ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 16384; ++i) {
            test_contents.push_back(static_cast<char>(i & 0xff));
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


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

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
        c->set_puback_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubrec_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubcomp_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&chk, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                c->publish("topic1", test_contents, MQTT_NS::qos::at_most_once);
            });
        c->set_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
            });
        c->set_publish_handler(
            [&chk, &c, &pid_unsub, &test_contents]
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
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

# if 0 // It would make network load too much.

BOOST_AUTO_TEST_CASE( pub_sub_over_2097152 ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        std::string test_contents;
        for (std::size_t i = 0; i < 2097152; ++i) {
            test_contents.push_back(static_cast<char>(i % 0xff));
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


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

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
        c->set_puback_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubrec_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_pubcomp_handler(
            []
            (std::uint16_t) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&chk, &c, &pid_sub, &test_contents]
            (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                c->publish("topic1", test_contents, MQTT_NS::qos::at_most_once);
            });
        c->set_unsuback_handler(
            [&chk, &c, &pid_unsub]
            (packet_id_t packet_id) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(packet_id == pid_unsub);
                c->disconnect();
            });
        c->set_publish_handler(
            [&chk, &c, &pid_unsub, &test_contents]
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
                BOOST_TEST(contents == test_contents);
                pid_unsub = c->unsubscribe("topic1");
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

#endif

BOOST_AUTO_TEST_SUITE_END()

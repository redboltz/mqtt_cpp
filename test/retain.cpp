// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_retain)

BOOST_AUTO_TEST_CASE( simple ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

                c->publish_at_most_once("topic1", "retained_contents", true);

                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
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
            [&chk, &pid_sub]
            (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents");
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( overwrite ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // publish topic1 QoS0 retain
            // publish topic1 QoS0 retain
            // publish topic1 QoS0
            // subscribe topic1 QoS0
            cont("h_suback"),
            cont("h_publish"),
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);

                c->publish_at_most_once("topic1", "retained_contents1", true);
                c->publish_at_most_once("topic1", "retained_contents2", true);
                c->publish_at_most_once("topic1", "retained_contents3", false);

                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
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
            [&chk, &pid_sub]
            (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
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
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                MQTT_CHK("h_publish");
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == true);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "retained_contents2");
                pid_unsub = c->unsubscribe("topic1");
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( retain_and_publish ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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

        c->set_connack_handler(
            [&chk, &c, &pid_sub]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
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
            (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 1U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        MQTT_CHK("h_suback1");
                        c->publish_at_most_once("topic1", "topic1_contents", true);
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
                        pid_sub = c->subscribe("topic1", mqtt::qos::at_most_once);
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
            (std::uint8_t header,
             mqtt::optional<packet_id_t> packet_id,
             std::string topic,
             std::string contents) {
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_CHECK(!packet_id);
                BOOST_TEST(topic == "topic1");
                BOOST_TEST(contents == "topic1_contents");
                pid_unsub = c->unsubscribe("topic1");
                auto ret = chk.match(
                    "h_suback1",
                    [&] {
                        MQTT_CHK("h_publish1");
                        BOOST_TEST(mqtt::publish::is_retain(header) == false);
                    },
                    "h_suback2",
                    [&] {
                        MQTT_CHK("h_publish2");
                        BOOST_TEST(mqtt::publish::is_retain(header) == true);
                    }
                );
                BOOST_TEST(ret);
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}


BOOST_AUTO_TEST_SUITE_END()

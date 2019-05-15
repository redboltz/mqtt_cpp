// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_multi_sub)

BOOST_AUTO_TEST_CASE( multi_channel ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_clean_session(true);

        std::uint16_t pid_sub;
        std::uint16_t pid_unsub;


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1 QoS0 topic2 QoS0
            cont("h_suback"),
            // publish topic1 QoS0
            cont("h_publish_topic1"),
            // publish topic2 QoS0
            cont("h_publish_topic2"),
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
                pid_sub = c->subscribe(
                    "topic1", mqtt::qos::at_most_once,
                    "topic2", mqtt::qos::at_most_once);

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
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 2U);
                BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
                BOOST_TEST(*results[1] == mqtt::qos::at_most_once);
                c->publish_at_most_once("topic1", "topic1_contents");
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
                BOOST_TEST(mqtt::publish::is_dup(header) == false);
                BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
                BOOST_TEST(mqtt::publish::is_retain(header) == false);
                BOOST_CHECK(!packet_id);
                auto ret = chk.match(
                    "h_suback",
                    [&] {
                        MQTT_CHK("h_publish_topic1");
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == "topic1_contents");
                        c->publish_at_most_once("topic2", "topic2_contents");
                    },
                    "h_publish_topic1",
                    [&] {
                        MQTT_CHK("h_publish_topic2");
                        BOOST_TEST(topic == "topic2");
                        BOOST_TEST(contents == "topic2_contents");
                        pid_unsub = c->unsubscribe(
                            "topic1",
                            "topic2");
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

BOOST_AUTO_TEST_CASE( multi_client_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    int sub_count = 0;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_clean_session(true);
    c1->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 2) s.close();
    };

    checker chk = {
        // connect
        cont("h_connack_1"),
        // subscribe topic1 QoS0
        cont("h_suback_1"),
        // publish topic1 QoS0
        cont("h_publish_1"),
        cont("h_unsuback_1"),
        // disconnect
        cont("h_close_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS0
        cont("h_suback_2"),
        // publish topic1 QoS0
        cont("h_publish_2"),
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c1->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_1");
            server_close();
        });
    c1->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c1->set_suback_handler(
        [&chk, &c1, &sub_count, &pid_sub1]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_1");
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c1->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c1->set_unsuback_handler(
        [&chk, &c1, &pid_unsub1]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_1");
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
            return true;
        });
    c1->set_publish_handler(
        [&chk, &c1, &pid_unsub1]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish_1");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
            return true;
        });

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_clean_session(true);
    c2->set_client_id("cid2");

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_2");
            server_close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c2->set_suback_handler(
        [&chk, &c2, &sub_count, &pid_sub2]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            if (++sub_count == 2)
                c2->publish_at_most_once("topic1", "topic1_contents");
            return true;
        });
    c2->set_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
        [&chk, &c2, &pid_unsub2]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( multi_client_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    // c3 --publish--> topic1 ----> c1, c2

    bool c1ready = false;
    bool c2ready = false;
    bool c3ready = false;
    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    auto c3 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_clean_session(true);
    c2->set_clean_session(true);
    c3->set_clean_session(true);
    c1->set_client_id("cid1");
    c2->set_client_id("cid2");
    c3->set_client_id("cid3");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    checker chk = {
        // connect
        cont("h_connack_1"),
        // subscribe topic1 QoS1
        cont("h_suback_1"),
        cont("h_publish_1"),
        cont("h_unsuback_1"),
        // disconnect
        cont("h_close_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS1
        cont("h_suback_2"),
        cont("h_publish_2"),
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),

        // connect
        deps("h_connack_3"),
        // publish topic1 QoS1
        cont("h_puback_3"),
        // disconnect
        cont("h_close_3"),
    };

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 3) s.close();
    };

    std::uint16_t pid_pub3;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    c1->set_connack_handler(
        [&chk, &c1, &pid_sub1]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c1->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_1");
            server_close();
        });
    c1->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c1->set_suback_handler(
        [&chk, &c1ready, &c2ready, &c3ready, &c3, &pid_sub1, &pid_pub3]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_1");
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c1ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c1->set_unsuback_handler(
        [&chk, &c1, &pid_unsub1]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_1");
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
            return true;
        });
    c1->set_publish_handler(
        [&chk, &c1, &pid_unsub1]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish_1");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
            return true;
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_least_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_2");
            server_close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1ready, &c2ready, &c3ready, &c3, &pid_sub2, &pid_pub3]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);

            c2ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c2->set_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_publish_handler(
        [&chk, &c2, &pid_unsub2]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_least_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c3->set_connack_handler(
        [&chk, &c3, &c1ready, &c2ready, &c3ready, &pid_pub3]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_3");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c3ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish_at_least_once("topic1", "topic1_contents");
            }
            return true;
        });
    c3->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_3");
            server_close();
        });
    c3->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c3->set_puback_handler(
        [&chk, &c3, &pid_pub3]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback_3");
            BOOST_TEST(packet_id == pid_pub3);
            c3->disconnect();
            return true;
        });
    c3->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });
    c3->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
            return true;
        });

    c1->connect();
    c2->connect();
    c3->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_SUITE_END()

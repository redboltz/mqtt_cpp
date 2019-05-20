// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "test_settings.hpp"
#include "test_broker.hpp"
#include "test_server_no_tls.hpp"
#include "checker.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_will)

BOOST_AUTO_TEST_CASE( will_qos0 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents"));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;


    checker chk = {
        // connect
        cont("h_connack_1"),
        // force_disconnect
        cont("h_error_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS0
        cont("h_suback_2"),
        cont("h_publish_2"), // will receive
        // unsubscribe topic1
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),

    };

    c1->set_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&chk]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    std::vector<std::string> const expected2 = {
        "finish",
    };

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
        [&chk, &s]
        () {
            MQTT_CHK("h_close_2");
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            c1_force_disconnect();
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
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( will_qos1 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", mqtt::qos::at_least_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;


    checker chk = {
        // connect
        cont("h_connack_1"),
        // force_disconnect
        cont("h_error_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS1
        cont("h_suback_2"),
        cont("h_publish_2"), // will receive
        // unsubscribe topic1
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),
    };

    c1->set_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&chk]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error_1");
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
        [&chk, &s]
        () {
            MQTT_CHK("h_close_2");
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_least_once);
            c1_force_disconnect();
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
            BOOST_CHECK(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( will_qos2 ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", mqtt::qos::exactly_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;


    checker chk  = {
        // connect
        cont("h_connack_1"),
        // force_disconnect
        cont("h_error_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS2
        cont("h_suback_2"),
        cont("h_publish_2"), // will receive
        cont("h_pub_res_sent_2"),
        // unsubscribe topic1
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),
    };

    c1->set_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&chk]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::exactly_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close_2");
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::exactly_once);
            c1_force_disconnect();
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
        [&chk]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::exactly_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            return true;
        });
    c2->set_pub_res_sent_handler(
        [&chk, &c2, &pid_unsub2]
        (std::uint16_t) {
            MQTT_CHK("h_pub_res_sent_2");
            pid_unsub2 = c2->unsubscribe("topic1");
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( will_retain ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        mqtt::will("topic1", "will_contents", true));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;


    checker chk = {
        // connect
        cont("h_connack_1"),
        // force_disconnect
        cont("h_error_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS0
        cont("h_suback1_2"),
        cont("h_publish1_2"), // will receive
        // unsubscribe topic1
        cont("h_unsuback1_2"),
        // subscribe topic1 QoS0
        cont("h_suback2_2"),
        cont("h_publish2_2"), // retain receive
        // unsubscribe topic1
        cont("h_unsuback2_2"),
        // disconnect
        cont("h_close_2"),
    };

    c1->set_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&chk]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error_1");
        });

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
        [&chk, &s]
        () {
            MQTT_CHK("h_close_2");
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> results) {
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(*results[0] == mqtt::qos::at_most_once);
            auto ret = chk.match(
                "h_connack_2",
                [&] {
                    MQTT_CHK("h_suback1_2");
                    c1_force_disconnect();
                },
                "h_unsuback1_2",
                [&] {
                    MQTT_CHK("h_suback2_2");
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c2->set_unsuback_handler(
        [&chk, &c2, &pid_unsub2, &pid_sub2]
        (packet_id_t packet_id) {
            BOOST_TEST(packet_id == pid_unsub2);
            auto ret = chk.match(
                "h_publish1_2",
                [&] {
                    MQTT_CHK("h_unsuback1_2");
                    pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
                },
                "h_publish2_2",
                [&] {
                    MQTT_CHK("h_unsuback2_2");
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        });
    c2->set_publish_handler(
        [&chk, &c2, &pid_unsub2]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents) {
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            auto ret = chk.match(
                "h_suback1_2",
                [&] {
                    MQTT_CHK("h_publish1_2");
                    BOOST_TEST(mqtt::publish::is_retain(header) == false);
                },
                "h_suback2_2",
                [&] {
                    MQTT_CHK("h_publish2_2");
                    BOOST_TEST(mqtt::publish::is_retain(header) == true);
                }
            );
            BOOST_TEST(ret);
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_CASE( overlength_message ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    std::string wm(0x10000, 'a');
    c1->set_will(mqtt::will("topic1", wm));
    c1->set_clean_session(true);
    c1->connect();
    try {
        ios.run();
        BOOST_CHECK(false);
    }
    catch (mqtt::will_message_length_error const&) {
        BOOST_CHECK(true);
    }
}

BOOST_AUTO_TEST_CASE( will_prop ) {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);

    auto c1 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    std::vector<mqtt::v5::property_variant> ps {
        mqtt::v5::property::payload_format_indicator(mqtt::v5::property::payload_format_indicator::string),
        mqtt::v5::property::message_expiry_interval(0x12345678UL),
        mqtt::v5::property::topic_alias(0x1234U),
        mqtt::v5::property::response_topic("response topic"),
        mqtt::v5::property::correlation_data("correlation data"),
        mqtt::v5::property::user_property("key1", "val1"),
        mqtt::v5::property::user_property("key2", "val2"),
        mqtt::v5::property::subscription_identifier(123),
    };

    auto prop_size = ps.size();
    std::size_t user_prop_count = 0;

    c1->set_will(
        mqtt::will(
            "topic1",
            "will_contents",
            false,
            std::move(ps)
        ));

    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = mqtt::make_client(ios, broker_url, broker_notls_port, mqtt::protocol_version::v5);
    c2->set_client_id("cid2");
    c2->set_clean_session(true);

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;


    checker chk = {
        // connect
        cont("h_connack_1"),
        // force_disconnect
        cont("h_error_1"),

        // connect
        deps("h_connack_2"),
        // subscribe topic1 QoS0
        cont("h_suback_2"),
        cont("h_publish_2"), // will receive
        // unsubscribe topic1
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),

    };

    c1->set_v5_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c1_force_disconnect();
            return true;
        });
    c1->set_close_handler(
        []
        () {
            BOOST_CHECK(false);
        });
    c1->set_error_handler(
        [&chk]
        (boost::system::error_code const&) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    std::vector<std::string> const expected2 = {
        "finish",
    };

    c2->set_v5_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", mqtt::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &s]
        () {
            MQTT_CHK("h_close_2");
            s.close();
        });
    c2->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c2->set_v5_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == mqtt::v5::reason_code::granted_qos_0);
            c1_force_disconnect();
            return true;
        });
    c2->set_v5_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id, std::vector<std::uint8_t> reasons, std::vector<mqtt::v5::property_variant> /*props*/) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == mqtt::v5::reason_code::success);
            c2->disconnect();
            return true;
        });
    c2->set_v5_publish_handler(
        [&chk, &c2, &pid_unsub2, prop_size, &user_prop_count]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic,
         std::string contents,
         std::vector<mqtt::v5::property_variant> props) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(mqtt::publish::is_dup(header) == false);
            BOOST_TEST(mqtt::publish::get_qos(header) == mqtt::qos::at_most_once);
            BOOST_TEST(mqtt::publish::is_retain(header) == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");

            BOOST_TEST(props.size() == prop_size);

            for (auto const& p : props) {
                mqtt::visit(
                    mqtt::make_lambda_visitor<void>(
                        [&](mqtt::v5::property::payload_format_indicator::recv const& t) {
                            BOOST_TEST(t.val() == mqtt::v5::property::payload_format_indicator::string);
                        },
                        [&](mqtt::v5::property::message_expiry_interval::recv const& t) {
                            BOOST_TEST(t.val() == 0x12345678UL);
                        },
                        [&](mqtt::v5::property::topic_alias::recv const& t) {
                            BOOST_TEST(t.val() == 0x1234U);
                        },
                        [&](mqtt::v5::property::response_topic::recv const& t) {
                            BOOST_TEST(t.val() == "response topic");
                        },
                        [&](mqtt::v5::property::correlation_data::recv const& t) {
                            BOOST_TEST(t.val() == "correlation data");
                        },
                        [&](mqtt::v5::property::user_property::recv const& t) {
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
                        [&](mqtt::v5::property::subscription_identifier::recv const& t) {
                            BOOST_TEST(t.val() == 123U);
                        },
                        [&](auto&& ...) {
                            BOOST_TEST(false);
                        }
                    ),
                    p
                );
            }

            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ios.run();
    BOOST_TEST(chk.all());
}

BOOST_AUTO_TEST_SUITE_END()

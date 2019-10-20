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
#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_pubsub_no_strand)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
            return true;
        });
    c->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close");
            finish();
            return true;
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
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
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos0 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS1
        cont("h_publish"),
        cont("h_puback"),
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
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
        [&chk, &c, &pid_pub, &pid_sub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
        [&chk]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_TEST(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos0 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS2
        cont("h_publish"),
        cont("h_pubrec"),
        cont("h_pubcomp"),
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_unsub, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->set_suback_handler(
        [&chk, &c, &pid_pub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == 1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        [&chk]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos1 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS2
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
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
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
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos1 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


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

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
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
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
    MQTT_NS::optional<packet_id_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
        [&chk, &recv_packet_id]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_least_once);
            BOOST_TEST(is_retain == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos1 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS0
        cont("h_suback"),
        // publish topic1 QoS2
        cont("h_publish"),
        cont("h_pub_res_sent"),
        cont("h_pubrec"),
        cont("h_pubcomp"),
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
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::at_least_once);
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    MQTT_NS::optional<packet_id_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        [&chk, &recv_packet_id]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_least_once);
            BOOST_TEST(is_retain == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos2 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


    checker chk = {
        // connect
        cont("h_connack"),
        // subscribe topic1 QoS2
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
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::exactly_once);
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
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos1_sub_qos2 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


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

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::exactly_once);
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
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_puback_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
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
    MQTT_NS::optional<packet_id_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
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
        [&chk, &recv_packet_id]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_least_once);
            BOOST_TEST(is_retain == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pub_qos2_sub_qos2 ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_clean_session(true);

    std::uint16_t pid_pub;
    std::uint16_t pid_sub;
    std::uint16_t pid_unsub;


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

    c->set_connack_handler(
        [&chk, &c, &pid_sub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub = c->subscribe("topic1", MQTT_NS::qos::exactly_once);
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
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            return true;
        });
    c->set_pubcomp_handler(
        [&chk, &c, &pid_pub, &pid_unsub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    MQTT_NS::optional<packet_id_t> recv_packet_id;
    c->set_pub_res_sent_handler(
        [&chk, &recv_packet_id]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pub_res_sent");
            BOOST_TEST(*recv_packet_id == packet_id);
        });
    c->set_suback_handler(
        [&chk, &c, &pid_sub, &pid_pub]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
            pid_pub = c->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
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
        [&chk, &recv_packet_id]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::exactly_once);
            BOOST_TEST(is_retain == false);
            BOOST_TEST(*packet_id != 0);
            recv_packet_id = packet_id;
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( publish_function ) {
    boost::asio::io_context iocb;
    test_broker b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    auto finish =
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                }
            );
        };

    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client_no_strand(ioc, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback");
            BOOST_TEST(packet_id == pid_sub);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish");
            BOOST_TEST(is_dup == false);
            BOOST_TEST(qos_value == MQTT_NS::qos::at_most_once);
            BOOST_TEST(is_retain == false);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub = c->unsubscribe("topic1");
            return true;
        });
    c->connect();
    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}


BOOST_AUTO_TEST_SUITE_END()

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
#include "global_fixture.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_will)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( will_qos0 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        MQTT_NS::will("topic1"_mb, "will_contents"_mb));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
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
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    std::vector<std::string> const expected2 = {
        "finish",
    };

    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
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
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( will_qo0_timeout ) {
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

    constexpr uint32_t will_expiry_interval = 1;
    as::steady_timer timeout(ioc);
    as::steady_timer timeout_2(ioc);

    MQTT_NS::v5::properties ps {
        MQTT_NS::v5::property::message_expiry_interval(will_expiry_interval),
    };

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(MQTT_NS::will("topic1"_mb, "will_contents"_mb, MQTT_NS::retain::yes, MQTT_NS::force_move(ps)));

    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
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
       // cont("h_publish_2"), // will receive
        // unsubscribe topic1
        cont("h_unsuback_2"),
        // disconnect
        cont("h_close_2"),

    };

    c1->set_v5_connack_handler(
        [&chk, &c1_force_disconnect]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2->set_v5_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_v5_suback_handler(
        [&chk, &c2, &c1_force_disconnect, &pid_sub2, &pid_unsub2, &timeout, &timeout_2, &will_expiry_interval]
        (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

            timeout.expires_after(std::chrono::seconds(1 + will_expiry_interval));
            timeout.async_wait(
                [&c1_force_disconnect](MQTT_NS::error_code ec) {
                    if (!ec) {
                         c1_force_disconnect();
                    }
                }
            );

            timeout_2.expires_after(std::chrono::seconds(2 + will_expiry_interval));
            timeout_2.async_wait(
                [&c2, &pid_unsub2](MQTT_NS::error_code ec) {
                    if (!ec) {
                        pid_unsub2 = c2->unsubscribe("topic1");
                    }
                }
            );

            return true;
        });
    c2->set_v5_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
            return true;
        });
    c2->set_v5_publish_handler(
        [&chk, &c2, &pid_unsub2]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) {

            // Will should not be received
            BOOST_TEST(false);
            return true;
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( will_qos1 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        MQTT_NS::will("topic1"_mb, "will_contents"_mb, MQTT_NS::qos::at_least_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
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
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_least_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
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
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_CHECK(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( will_qos2 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        MQTT_NS::will("topic1"_mb, "will_contents"_mb, MQTT_NS::qos::exactly_once));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
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
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::exactly_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
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
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::exactly_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
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

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( will_retain ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_will(
        MQTT_NS::will("topic1"_mb, "will_contents"_mb, MQTT_NS::retain::yes));
    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
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
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
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
                    pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
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
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
            auto ret = chk.match(
                "h_suback1_2",
                [&] {
                    MQTT_CHK("h_publish1_2");
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                },
                "h_suback2_2",
                [&] {
                    MQTT_CHK("h_publish2_2");
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                }
            );
            BOOST_TEST(ret);
            return true;
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( overlength_message ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    std::string wm(0x10000, 'a');
    c1->set_will(MQTT_NS::will("topic1"_mb, MQTT_NS::buffer(MQTT_NS::string_view(wm))));
    c1->set_clean_session(true);
    try {
        c1->connect();
        ioc.run();
        BOOST_CHECK(false);
    }
    catch (MQTT_NS::will_message_length_error const&) {
        BOOST_CHECK(true);
        c1->force_disconnect();
        finish();
    }

    th.join();
}

BOOST_AUTO_TEST_CASE( will_prop ) {
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
                    b.clear_all_retained_topics();
                }
            );
        };

    boost::asio::io_context ioc;

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    MQTT_NS::v5::properties ps {
        MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
        MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
        MQTT_NS::v5::property::will_delay_interval(0x12345678UL),
        MQTT_NS::v5::property::topic_alias(0x1234U),
        MQTT_NS::v5::property::response_topic("response topic"_mb),
        MQTT_NS::v5::property::correlation_data("correlation data"_mb),
        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        MQTT_NS::v5::property::subscription_identifier(123),
    };

    auto prop_size = ps.size();
    std::size_t user_prop_count = 0;

    c1->set_will(
        MQTT_NS::will(
            "topic1"_mb,
            "will_contents"_mb,
            MQTT_NS::retain::yes,
            MQTT_NS::force_move(ps)
        ));

    int c1fd_count = 0;
    auto c1_force_disconnect = [&c1, &c1fd_count] {
        if (++c1fd_count == 2) c1->force_disconnect();
    };

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
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
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
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
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error_1");
        });

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;


    std::vector<std::string> const expected2 = {
        "finish",
    };

    c2->set_v5_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
            return true;
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close_2");
            finish();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_v5_suback_handler(
        [&chk, &c1_force_disconnect, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            c1_force_disconnect();
            return true;
        });
    c2->set_v5_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
            c2->disconnect();
            return true;
        });
    c2->set_v5_publish_handler(
        [&chk, &c2, &pid_unsub2, prop_size, &user_prop_count]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties props) {
            MQTT_CHK("h_publish_2");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "will_contents");

            BOOST_TEST(props.size() == prop_size);

            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&](MQTT_NS::v5::property::payload_format_indicator const& t) {
                            BOOST_TEST(t.val() == MQTT_NS::v5::property::payload_format_indicator::string);
                        },
                        [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
                            BOOST_TEST(t.val() <= 0x12345678UL);
                        },
                        [&](MQTT_NS::v5::property::will_delay_interval const& t) {
                            BOOST_TEST(t.val() == 0x12345678UL);
                        },
                        [&](MQTT_NS::v5::property::topic_alias const& t) {
                            BOOST_TEST(t.val() == 0x1234U);
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

            pid_unsub2 = c2->unsubscribe("topic1");
            return true;
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

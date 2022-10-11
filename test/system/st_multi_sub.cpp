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

BOOST_AUTO_TEST_SUITE(st_multi_sub)

using namespace std::literals::string_literals;

BOOST_AUTO_TEST_CASE( multi_channel ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
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
            (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                pid_sub = c->subscribe(
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                    {
                        {"topic1", MQTT_NS::qos::at_most_once},
                        {"topic2", MQTT_NS::qos::at_most_once}
                    }
                );

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
            [&chk, &c, &pid_sub]
            (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
                MQTT_CHK("h_suback");
                BOOST_TEST(packet_id == pid_sub);
                BOOST_TEST(results.size() == 2U);
                BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                BOOST_TEST(results[1] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
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
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                BOOST_CHECK(!packet_id);
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_publish_topic1");
                        BOOST_TEST(topic == "topic1");
                        BOOST_TEST(contents == "topic1_contents");
                        c->publish("topic2", "topic2_contents", MQTT_NS::qos::at_most_once);
                    },
                    [&] {
                        MQTT_CHK("h_publish_topic2");
                        BOOST_TEST(topic == "topic2");
                        BOOST_TEST(contents == "topic2_contents");
                        pid_unsub = c->unsubscribe(
                            std::vector<MQTT_NS::string_view>
                            {
                                "topic1",
                                "topic2"
                            }
                        );
                    }
                );
                BOOST_TEST(ret);
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( multi_client_qos0 ) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
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
    int sub_count = 0;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_clean_session(true);
    c1->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 2) finish();
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
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", MQTT_NS::qos::at_most_once);
        });
    c1->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_1");
            server_close();
        });
    c1->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c1->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c1->set_suback_handler(
        [&chk, &c1, &sub_count, &pid_sub1]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_1");
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            if (++sub_count == 2)
                c1->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
        });
    c1->set_unsuback_handler(
        [&chk, &c1, &pid_unsub1]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_1");
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
        });
    c1->set_publish_handler(
        [&chk, &c1, &pid_unsub1]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish_1");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
        });

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_clean_session(true);
    c2->set_client_id("cid2");

    std::uint16_t pid_sub2;
    std::uint16_t pid_unsub2;

    c2->set_connack_handler(
        [&chk, &c2, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub2 = c2->subscribe("topic1", MQTT_NS::qos::at_most_once);
        });
    c2->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_2");
            server_close();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c2, &sub_count, &pid_sub2]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
            if (++sub_count == 2)
                c2->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
        });
    c2->set_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
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
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
        });

    c1->connect();
    c2->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( multi_client_qos1 ) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
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
    // c3 --publish--> topic1 ----> c1, c2

    bool c1ready = false;
    bool c2ready = false;
    bool c3ready = false;
    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    auto c3 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
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
        if (++close_count == 3) finish();
    };

    std::uint16_t pid_pub3;

    std::uint16_t pid_sub1;
    std::uint16_t pid_unsub1;

    c1->set_connack_handler(
        [&chk, &c1, &pid_sub1]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            pid_sub1 = c1->subscribe("topic1", MQTT_NS::qos::at_least_once);
        });
    c1->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_1");
            server_close();
        });
    c1->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c1->set_suback_handler(
        [&chk, &c1ready, &c2ready, &c3ready, &c3, &pid_sub1, &pid_pub3]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_1");
            BOOST_TEST(packet_id == pid_sub1);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);

            c1ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
            }
        });
    c1->set_unsuback_handler(
        [&chk, &c1, &pid_unsub1]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_1");
            BOOST_TEST(packet_id == pid_unsub1);
            c1->disconnect();
        });
    c1->set_publish_handler(
        [&chk, &c1, &pid_unsub1]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            MQTT_CHK("h_publish_1");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub1 = c1->unsubscribe("topic1");
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
        });
    c2->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_2");
            server_close();
        });
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c2->set_suback_handler(
        [&chk, &c1ready, &c2ready, &c3ready, &c3, &pid_sub2, &pid_pub3]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(packet_id == pid_sub2);
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);

            c2ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
            }
        });
    c2->set_unsuback_handler(
        [&chk, &c2, &pid_unsub2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_unsuback_2");
            BOOST_TEST(packet_id == pid_unsub2);
            c2->disconnect();
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
            BOOST_TEST(*packet_id != 0);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents");
            pid_unsub2 = c2->unsubscribe("topic1");
        });

    c3->set_connack_handler(
        [&chk, &c3, &c1ready, &c2ready, &c3ready, &pid_pub3]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("h_connack_3");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            c3ready = true;
            if (c1ready && c2ready && c3ready) {
                pid_pub3 = c3->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
            }
        });
    c3->set_close_handler(
        [&chk, &server_close]
        () {
            MQTT_CHK("h_close_3");
            server_close();
        });
    c3->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });
    c3->set_puback_handler(
        [&chk, &c3, &pid_pub3]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback_3");
            BOOST_TEST(packet_id == pid_pub3);
            c3->disconnect();
        });
    c3->set_pubrec_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });
    c3->set_pubcomp_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });

    c1->connect();
    c2->connect();
    c3->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( multi_client_nl ) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
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

    int close_count = 0;
    auto server_close = [&] {
        if (++close_count == 2) finish();
    };

    boost::asio::io_context ioc;

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_clean_start(true);
    c1->set_client_id("cid1");

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_clean_start(true);
    c2->set_client_id("cid2");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    checker chk = {
        // connect
        cont("h_connack_1"),
        cont("h_connack_2"),

        // subscribe topic1 QoS0 NL:yes
        cont("h_suback_1"),
        // subscribe topic1 QoS0 NL:no
        cont("h_suback_2"),

        // c1 publish topic1 QoS0
        cont("h_publish_2_1"),
        // c2 publish topic1 QoS0
        cont("h_publish_1"),
        deps("h_publish_2_2","h_publish_2_1"),

        // disconnect
        deps("h_close_1", "h_publish_1"),
        deps("h_close_2", "h_publish_2_2"),
    };

    c1->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            c2->connect();
        }
    );

    c2->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            c1->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::nl::yes);
        }
    );

    c1->set_v5_suback_handler(
        [&]
        (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_1");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            c2->subscribe("topic1", MQTT_NS::qos::at_most_once | MQTT_NS::nl::no);
        }
    );

    c2->set_v5_suback_handler(
        [&]
        (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_2");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_most_once);
        }
    );

    c1->set_v5_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_publish_1");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_CHECK(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(contents == "topic1_contents2");
            c1->disconnect();
        }
    );

    c2->set_v5_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) {
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_publish_2_1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents1");
                    c2->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_most_once);
                },
                [&] {
                    MQTT_CHK("h_publish_2_2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents2");
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);

        }
    );

    c1->set_close_handler(
        [&]
        () {
            MQTT_CHK("h_close_1");
            server_close();
        });
    c2->set_close_handler(
        [&]
        () {
            MQTT_CHK("h_close_2");
            server_close();
        });

    c1->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        });

    c2->set_puback_handler(
        []
        (std::uint16_t) {
            BOOST_CHECK(false);
        });

    c1->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}


BOOST_AUTO_TEST_SUITE_END()

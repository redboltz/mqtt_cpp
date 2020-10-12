// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "global_fixture.hpp"

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_SUITE(test_shared_sub)

BOOST_AUTO_TEST_CASE( qos0 ) {
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
    // c3 --publish--> topic1 ----> c1, c2

    auto p1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    auto s1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    auto s2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);

    p1->set_clean_start(true);
    s1->set_clean_start(true);
    s2->set_clean_start(true);

    p1->set_client_id("p1");
    s1->set_client_id("s1");
    s2->set_client_id("s2");

    using packet_id_t = typename std::remove_reference_t<decltype(*p1)>::packet_id_t;

    checker chk = {
        // connect
        cont("h_connack_p1"),
        cont("h_connack_s1"),
        cont("h_connack_s2"),

        // shared subscribe
        cont("h_suback_s1"),
        cont("h_suback_s2"),

        // publish
        cont("h_publish_s1"),
        deps("h_publish_s2_1", "h_suback_s2"),

        // shared unsubscribe
        cont("h_unsuback_s2"),

        // publish
        cont("h_publish_s2_2"),

        // shared unsubscribe
        cont("h_unsuback_s1"),

        // close
        cont("h_close_p1"),
        cont("h_close_s1"),
        cont("h_close_s2"),
    };

    p1->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_p1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s1->connect();
            return true;
        });

    s1->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_s1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s2->connect();
            return true;
        });

    s2->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_s2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s1->subscribe("$share/share1/topic1", MQTT_NS::qos::at_most_once);
            return true;
        });

    s1->set_v5_suback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_s1");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

            s2->subscribe("$share/share1/topic1", MQTT_NS::qos::at_most_once);

            return true;
        });

    s2->set_v5_suback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_s2");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

            p1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_most_once);
            p1->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_most_once);

            return true;
        });

    std::set<MQTT_NS::string_view> pubmsg {
        "topic1_contents1",
        "topic1_contents2",
    };

    auto g = MQTT_NS::shared_scope_guard(
        [&] {
            s1->unsubscribe("$share/share1/topic1");
        }
    );

    s1->set_v5_publish_handler(
        [&, g]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_publish_s1");
            BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
            BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
            BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
            BOOST_TEST(!packet_id);
            BOOST_TEST(topic == "topic1");
            BOOST_TEST(pubmsg.erase(contents) == 1);
            return true;
        });

    s2->set_v5_publish_handler(
        [&, g]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) {
            auto ret = chk.match(
                "h_suback_s2",
                [&] {
                    MQTT_CHK("h_publish_s2_1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(pubmsg.erase(contents) == 1);
                    s1->unsubscribe("$share/share1/topic1");
                },
                "h_unsuback_s2",
                [&] {
                    MQTT_CHK("h_publish_s2_2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents3");
                }
            );
            BOOST_TEST(ret);
            return true;
        });

    g.reset();

    s1->set_v5_unsuback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_unsuback_s1");
            p1->publish("topic1", "topic1_contents3", MQTT_NS::qos::at_most_once);
            return true;
        });
    s2->set_v5_unsuback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_unsuback_s2");
            p1->disconnect();
            return true;
        });

    p1->set_close_handler(
        [&]
        () {
            MQTT_CHK("h_close_p1");
            s1->disconnect();
        });
    s1->set_close_handler(
        [&]
        () {
            MQTT_CHK("h_close_s1");
            s2->disconnect();
        });
    s2->set_close_handler(
        [&]
        () {
            MQTT_CHK("h_close_s2");
            finish();
        });

    p1->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

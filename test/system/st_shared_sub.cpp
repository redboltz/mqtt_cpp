// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "../common/global_fixture.hpp"

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_SUITE(st_shared_sub)

BOOST_AUTO_TEST_CASE( qos0 ) {
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

    // p1 --publish--> sn1/t1 sn1/t2 ----> s1, s2, s3
    // https://github.com/redboltz/mqtt_cpp/issues/716#issue-731179540

    auto p1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    auto s1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    auto s2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    auto s3 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);

    p1->set_clean_start(true);
    s1->set_clean_start(true);
    s2->set_clean_start(true);
    s3->set_clean_start(true);

    p1->set_client_id("p1");
    s1->set_client_id("s1");
    s2->set_client_id("s2");
    s3->set_client_id("s3");

    using packet_id_t = typename std::remove_reference_t<decltype(*p1)>::packet_id_t;

    checker chk = {
        // connect
        cont("h_connack_p1"),
        cont("h_connack_s1"),
        cont("h_connack_s2"),
        cont("h_connack_s3"),

        // shared subscribe
        cont("h_suback_s1"),
        cont("h_suback_s2"),
        cont("h_suback_s3"),

        // publish t1,t2,t1,t2,t1,t2,t1,t2  8times

        deps("h_publish_s1_1", "h_suback_s1"),
        cont("h_publish_s1_2"),
        cont("h_publish_s1_3"),
        deps("h_publish_s2_1", "h_suback_s2"),
        cont("h_publish_s2_2"),
        deps("h_publish_s3_1", "h_suback_s3"),
        cont("h_publish_s3_2"),
        cont("h_publish_s3_3"),

        // close
        deps("h_close_p1", "h_suback_s3"),
        deps("h_close_s1", "h_publish_s1_3"),
        deps("h_close_s2", "h_publish_s2_2"),
        deps("h_close_s3", "h_publish_s3_3"),
    };

    p1->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_p1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s1->connect();
            return true;
        }
    );

    s1->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_s1");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s2->connect();
            return true;
        }
    );

    s2->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_s2");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s3->connect();
            return true;
        }
    );

    s3->set_v5_connack_handler(
        [&]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_connack_s3");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            s1->subscribe(
                std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                {
                    {"$share/sn1/t1", MQTT_NS::qos::at_most_once},
                    {"$share/sn1/t2", MQTT_NS::qos::at_most_once}
                }
            );
            return true;
        }
    );

    s1->set_v5_suback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_s1");
            BOOST_TEST(reasons.size() == 2U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            BOOST_TEST(reasons[1] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

            s2->subscribe("$share/sn1/t2", MQTT_NS::qos::at_most_once);

            return true;
        }
    );

    s2->set_v5_suback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_s2");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            s3->subscribe(
                std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                {
                    {"$share/sn1/t1", MQTT_NS::qos::at_most_once},
                    {"$share/sn1/t2", MQTT_NS::qos::at_most_once}
                }
            );
            return true;
        }
    );

    s3->set_v5_suback_handler(
        [&]
        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_suback_s3");
            BOOST_TEST(reasons.size() == 2U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
            BOOST_TEST(reasons[1] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

            p1->publish("t1", "contents1", MQTT_NS::qos::at_most_once);
            p1->publish("t2", "contents2", MQTT_NS::qos::at_most_once);
            p1->publish("t1", "contents3", MQTT_NS::qos::at_most_once);
            p1->publish("t2", "contents4", MQTT_NS::qos::at_most_once);
            p1->publish("t1", "contents5", MQTT_NS::qos::at_most_once);
            p1->publish("t2", "contents6", MQTT_NS::qos::at_most_once);
            p1->publish("t1", "contents7", MQTT_NS::qos::at_most_once);
            p1->publish("t2", "contents8", MQTT_NS::qos::at_most_once);
            p1->disconnect();
            return true;
        }
    );

    s1->set_v5_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) mutable {
            auto ret = chk.match(
                "h_suback_s1",
                [&] {
                    MQTT_CHK("h_publish_s1_1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t1");
                    BOOST_TEST(contents == "contents1");
                },
                "h_publish_s1_1",
                [&]{
                    MQTT_CHK("h_publish_s1_2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t2");
                    BOOST_TEST(contents == "contents4");
                },
                "h_publish_s1_2",
                [&]{
                    MQTT_CHK("h_publish_s1_3");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t1");
                    BOOST_TEST(contents == "contents7");
                    s1->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );

    s2->set_v5_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) mutable {
            auto ret = chk.match(
                "h_suback_s2",
                [&] {
                    MQTT_CHK("h_publish_s2_1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t2");
                    BOOST_TEST(contents == "contents2");
                },
                "h_publish_s2_1",
                [&]{
                    MQTT_CHK("h_publish_s2_2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t2");
                    BOOST_TEST(contents == "contents6");
                    s2->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );

    s3->set_v5_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/) mutable {
            auto ret = chk.match(
                "h_suback_s3",
                [&] {
                    MQTT_CHK("h_publish_s3_1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t1");
                    BOOST_TEST(contents == "contents3");
                },
                "h_publish_s3_1",
                [&]{
                    MQTT_CHK("h_publish_s3_2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t1");
                    BOOST_TEST(contents == "contents5");
                },
                "h_publish_s3_2",
                [&]{
                    MQTT_CHK("h_publish_s3_3");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_TEST(!packet_id);
                    BOOST_TEST(topic == "t2");
                    BOOST_TEST(contents == "contents8");
                    s3->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );

    auto g = MQTT_NS::shared_scope_guard(
        [&] {
            finish();
        }
    );

    p1->set_close_handler(
        [&, g]
        () mutable {
            MQTT_CHK("h_close_p1");
            g.reset();
        }
    );
    s1->set_close_handler(
        [&, g]
        () mutable {
            MQTT_CHK("h_close_s1");
            g.reset();
        }
    );
    s2->set_close_handler(
        [&, g]
        () mutable {
            MQTT_CHK("h_close_s2");
            g.reset();
        }
    );
    s3->set_close_handler(
        [&, g]
        () mutable {
            MQTT_CHK("h_close_s3");
            g.reset();
        }
    );

    g.reset();
    p1->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "test_util.hpp"
#include "global_fixture.hpp"

BOOST_AUTO_TEST_SUITE(test_broker_offline_message)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( offline_pubsub_v3_1_1 ) {

    //
    // c1 ---- broker ----- c2 (CleanSession: false)
    //
    // 1. c2 subscribe t1 QoS2
    // 2. c2 disconnect
    // 3. c1 publish t1 QoS0
    // 4. c1 publish t1 QoS1
    // 5. c1 publish t1 QoS2
    // 6. c2 connect again
    //

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
    c1->set_clean_session(true);
    c1->set_client_id("cid1");

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_clean_session(false);
    c2->set_client_id("cid2");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    checker chk = {
        cont("c1_h_connack"),
        cont("c2_h_connack1"),

        // c2 subscribe t1 qos2
        cont("c2_h_suback"),
        cont("c2_h_close1"),

        // c1 publish t1 qos0
        // c1 publish t1 qos1
        // c1 publish t1 qos2
        cont("c1_h_puback"),
        cont("c1_h_pubrec"),
        cont("c1_h_pubcomp"),

        // c2 connect again
        cont("c2_h_connack2"),
        cont("c2_h_publish1"),
        cont("c2_h_publish2"),
        cont("c2_h_publish3"),

        cont("c1_h_close"),
        cont("c2_h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            MQTT_CHK("c1_h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            c2->connect();
            return true;
        }
    );
    c2->set_connack_handler(
        [&chk, &c2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            auto ret = chk.match(
                "c1_h_connack",
                [&] {
                    MQTT_CHK("c2_h_connack1");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c2->subscribe("topic1", MQTT_NS::qos::exactly_once);
                },
                "c2_h_connack1",
                [&] {
                    MQTT_CHK("c2_h_connack2");
                    BOOST_TEST(sp == true);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );
    c2->set_suback_handler(
        [&chk, &c2]
        (packet_id_t, std::vector<MQTT_NS::suback_return_code> results) {
            MQTT_CHK("c2_h_suback");
            BOOST_TEST(results.size() == 1U);
            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
            c2->disconnect();
            return true;
        }
    );
    c2->set_close_handler(
        [&chk, &c1, &finish]
        () {
            auto ret = chk.match(
                "c2_h_suback",
                [&] {
                    MQTT_CHK("c2_h_close1");
                    c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_most_once);
                    c1->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
                    c1->publish("topic1", "topic1_contents3", MQTT_NS::qos::exactly_once);
                },
                "c2_h_close1",
                [&] {
                    MQTT_CHK("c2_h_close2");
                    finish();
                }
            );
            BOOST_TEST(ret);

        }
    );
    c1->set_puback_handler(
        [&chk]
        (std::uint16_t) {
            MQTT_CHK("c1_h_puback");
            return true;
        }
    );
    c1->set_pubrec_handler(
        [&chk]
        (std::uint16_t) {
            MQTT_CHK("c1_h_pubrec");
            return true;
        }
    );
    c1->set_pubcomp_handler(
        [&chk, &c2]
        (std::uint16_t) {
            MQTT_CHK("c1_h_pubcomp");
            c2->connect();
            return true;
        }
    );
    c2->set_publish_handler(
        [&chk, &c1]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic,
         MQTT_NS::buffer contents) {
            auto ret = chk.match(
                "c2_h_connack2",
                [&] {
                    MQTT_CHK("c2_h_publish1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents1");
                },
                "c2_h_publish1",
                [&] {
                    MQTT_CHK("c2_h_publish2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents2");
                },
                "c2_h_publish2",
                [&] {
                    MQTT_CHK("c2_h_publish3");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::exactly_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents3");

                    c1->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );
    c1->set_close_handler(
        [&chk, &c2]
        () {
            MQTT_CHK("c1_h_close");
            c2->disconnect();
        }
    );

    // error cases
    c1->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        }
    );
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        }
    );

    c1->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( offline_pubsub_v5 ) {

    //
    // c1 ---- broker ----- c2 (CleanSession: false)
    //
    // 1. c2 subscribe t1 QoS2
    // 2. c2 disconnect
    // 3. c1 publish t1 QoS0
    // 4. c1 publish t1 QoS1
    // 5. c1 publish t1 QoS2
    // 6. c2 connect again
    //

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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_clean_session(true);
    c1->set_client_id("cid1");

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_clean_session(false);
    c2->set_client_id("cid2");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    checker chk = {
        cont("c1_h_connack"),
        cont("c2_h_connack1"),

        // c2 subscribe t1 qos2
        cont("c2_h_suback"),
        cont("c2_h_close1"),

        // c1 publish t1 qos0
        // c1 publish t1 qos1
        // c1 publish t1 qos2
        cont("c1_h_puback"),
        cont("c1_h_pubrec"),
        cont("c1_h_pubcomp"),

        // c2 connect again
        cont("c2_h_connack2"),
        cont("c2_h_publish1"),
        cont("c2_h_publish2"),
        cont("c2_h_publish3"),

        cont("c1_h_close"),
        cont("c2_h_close2"),
    };

    MQTT_NS::v5::properties ps {
        MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
        MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
        MQTT_NS::v5::property::content_type("content type"_mb),
        MQTT_NS::v5::property::topic_alias(0x1234U),
        MQTT_NS::v5::property::response_topic("response topic"_mb),
        MQTT_NS::v5::property::correlation_data("correlation data"_mb),
        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
    };

    auto prop_size = ps.size();
    std::size_t user_prop_count = 0;

    c1->set_v5_connack_handler(
        [&chk, &c2]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("c1_h_connack");
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_reason_code == MQTT_NS::v5::connect_reason_code::success);
            c2->connect(
                MQTT_NS::v5::properties{
                    MQTT_NS::v5::property::session_expiry_interval(
                        MQTT_NS::session_never_expire
                    )
                }
            );
            return true;
        }
    );
    c2->set_v5_connack_handler(
        [&chk, &c2]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_reason_code, MQTT_NS::v5::properties /*props*/) {
            auto ret = chk.match(
                "c1_h_connack",
                [&] {
                    MQTT_CHK("c2_h_connack1");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    c2->subscribe("topic1", MQTT_NS::qos::exactly_once);
                },
                "c2_h_connack1",
                [&] {
                    MQTT_CHK("c2_h_connack2");
                    BOOST_TEST(sp == true);
                    BOOST_TEST(connack_reason_code == MQTT_NS::v5::connect_reason_code::success);
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );
    c2->set_v5_suback_handler(
        [&chk, &c2]
        (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) mutable {
            MQTT_CHK("c2_h_suback");
            BOOST_TEST(reasons.size() == 1U);
            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_2);
            c2->disconnect();
            return true;
        }
    );
    c2->set_close_handler(
        [&chk, &c1, &finish, &ps]
        () {
            auto ret = chk.match(
                "c2_h_suback",
                [&] {
                    MQTT_CHK("c2_h_close1");
                    c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_most_once, ps);
                    c1->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once, ps);
                    c1->publish("topic1", "topic1_contents3", MQTT_NS::qos::exactly_once, ps);
                },
                "c2_h_close1",
                [&] {
                    MQTT_CHK("c2_h_close2");
                    finish();
                }
            );
            BOOST_TEST(ret);

        }
    );
    c1->set_v5_puback_handler(
        [&chk]
        (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("c1_h_puback");
            return true;
        }
    );
    c1->set_v5_pubrec_handler(
        [&chk]
        (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("c1_h_pubrec");
            return true;
        }
    );
    c1->set_v5_pubcomp_handler(
        [&chk, &c2]
        (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("c1_h_pubcomp");
            c2->connect(
                MQTT_NS::v5::properties{
                    MQTT_NS::v5::property::session_expiry_interval(
                        MQTT_NS::session_never_expire
                    )
                }
            );
            return true;
        }
    );
    c2->set_v5_publish_handler(
        [&chk, &c1, &prop_size, &user_prop_count]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props) {

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
                            BOOST_TEST(t.val() == "correlation data");
                        },
                        [&](MQTT_NS::v5::property::user_property const& t) {
                            switch (user_prop_count++) {
                            case 0:
                            case 2:
                            case 4:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 1:
                            case 3:
                            case 5:
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

            auto ret = chk.match(
                "c2_h_connack2",
                [&] {
                    MQTT_CHK("c2_h_publish1");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_most_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(!packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents1");
                },
                "c2_h_publish1",
                [&] {
                    MQTT_CHK("c2_h_publish2");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::at_least_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents2");
                },
                "c2_h_publish2",
                [&] {
                    MQTT_CHK("c2_h_publish3");
                    BOOST_TEST(pubopts.get_dup() == MQTT_NS::dup::no);
                    BOOST_TEST(pubopts.get_qos() == MQTT_NS::qos::exactly_once);
                    BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::no);
                    BOOST_CHECK(packet_id);
                    BOOST_TEST(topic == "topic1");
                    BOOST_TEST(contents == "topic1_contents3");

                    c1->disconnect();
                }
            );
            BOOST_TEST(ret);
            return true;
        }
    );
    c1->set_close_handler(
        [&chk, &c2]
        () {
            MQTT_CHK("c1_h_close");
            c2->disconnect();
        }
    );

    // error cases
    c1->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        }
    );
    c2->set_error_handler(
        []
        (MQTT_NS::error_code) {
            BOOST_CHECK(false);
        }
    );

    c1->connect();

    ioc.run();
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "ordered_caller.hpp"
#include "test_util.hpp"
#include "../common/global_fixture.hpp"

BOOST_AUTO_TEST_SUITE(st_reqres)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( pubsub ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");

        std::string response_topic;

        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe
            cont("h_suback"),
            // publish
            cont("h_publish"),
            // disconnect
            cont("h_close"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                std::size_t times = 0;
                MQTT_NS::v5::visit_props(
                    props,
                    [&](MQTT_NS::v5::property::response_topic const& v) {
                        ++times;
                        response_topic = std::string(v.val());
                    },
                    [](auto){}
                );
                BOOST_TEST(times == 1);

                c->subscribe(response_topic, MQTT_NS::qos::at_most_once);
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t /*packet_id*/,
             std::vector<MQTT_NS::v5::suback_reason_code> reasons,
             MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                BOOST_TEST(reasons.size() == 1U);
                BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);

                c->publish(response_topic, "response_contents", MQTT_NS::qos::at_most_once);
            });
        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_publish");
                BOOST_TEST(topic == response_topic);
                BOOST_TEST(contents == "response_contents");
                c->disconnect();
            });
        c->set_close_handler(
            [&]
            () {
                MQTT_CHK("h_close");
                finish();
            });
        c->set_error_handler(
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        c->set_clean_session(true);
        c->connect(
            MQTT_NS::v5::properties{
                MQTT_NS::v5::property::request_response_information(true),
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( session ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");

        std::string response_topic1;
        std::string response_topic2;

        checker chk = {
            // connect
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),

            // connect
            cont("h_connack2"),
            // disconnect
            cont("h_close2"),

            // connect
            cont("h_connack3"),
            // disconnect
            cont("h_close3"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_connack1");
                        BOOST_TEST(sp == false);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const& v) {
                                ++times;
                                response_topic1 = std::string(v.val());
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                    },
                    [&] {
                        MQTT_CHK("h_connack2");
                        BOOST_TEST(sp == true);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const& v) {
                                ++times;
                                response_topic2 = std::string(v.val());
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                        BOOST_TEST(response_topic1 == response_topic2);
                    },
                    [&] {
                        MQTT_CHK("h_connack3");
                        BOOST_TEST(sp == false);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const& v) {
                                ++times;
                                BOOST_TEST(v.val() != response_topic1);
                                BOOST_TEST(v.val() != response_topic2);
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                    }
                );
                BOOST_TEST(ret);
                c->disconnect();
            });
        c->set_close_handler(
            [&]
            () {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        // session will expire on disconnect
                        c->connect(
                            MQTT_NS::v5::properties{
                                MQTT_NS::v5::property::request_response_information(true)
                            }
                        );
                    },
                    [&] {
                        MQTT_CHK("h_close2");
                        // no session exists even if clean false
                        c->set_clean_session(false);
                        c->connect(
                            MQTT_NS::v5::properties{
                                MQTT_NS::v5::property::request_response_information(true)
                            }
                        );
                    },
                    [&] {
                        MQTT_CHK("h_close3");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        c->set_clean_session(true);
        c->connect(
            MQTT_NS::v5::properties{
                MQTT_NS::v5::property::request_response_information(true),
                MQTT_NS::v5::property::session_expiry_interval(
                    MQTT_NS::session_never_expire
                )
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( retain ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");

        std::string response_topic1;
        std::string response_topic2;

        // postfix number is a connection number in this scenario
        checker chk = {
            // connect
            cont("h_connack1"),
            // publish retain
            // disconnect
            cont("h_close1"),

            // connect
            cont("h_connack2"),
            // subscribe
            cont("h_suback2"),
            // receive retained publish
            cont("h_publish2"),
            // disconnect
            cont("h_close2"),

            // connect
            cont("h_connack3"),
            // subscribe
            cont("h_suback3"),
            // disconnect
            cont("h_close3"),
        };

        c->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_connack1");
                        BOOST_TEST(sp == false);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const& v) {
                                ++times;
                                response_topic1 = std::string(v.val());
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                        c->publish(
                            response_topic1,
                            "response_contents",
                            MQTT_NS::qos::at_most_once | MQTT_NS::retain::yes
                        );
                        c->disconnect();
                    },
                    [&] {
                        MQTT_CHK("h_connack2");
                        BOOST_TEST(sp == true);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const& v) {
                                ++times;
                                response_topic2 = std::string(v.val());
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                        BOOST_TEST(response_topic1 == response_topic2);
                        c->subscribe(response_topic2, MQTT_NS::qos::at_most_once);
                    },
                    [&] {
                        MQTT_CHK("h_connack3");
                        BOOST_TEST(sp == false);
                        BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                        std::size_t times = 0;
                        MQTT_NS::v5::visit_props(
                            props,
                            [&](MQTT_NS::v5::property::response_topic const&) {
                                ++times;
                            },
                            [](auto){}
                        );
                        BOOST_TEST(times == 1);
                        // subscribe erased (previous) response topic
                        c->subscribe(response_topic2, MQTT_NS::qos::at_most_once);
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_v5_suback_handler(
            [&]
            (packet_id_t /*packet_id*/,
             std::vector<MQTT_NS::v5::suback_reason_code> reasons,
             MQTT_NS::v5::properties /*props*/) {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_suback2");
                        BOOST_TEST(reasons.size() == 1U);
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    },
                    [&] {
                        MQTT_CHK("h_suback3");
                        BOOST_TEST(reasons.size() == 1U);
                        // When authorization mechanism is introduced it will ought to be failed
                        BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        c->disconnect();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_v5_publish_handler(
            [&]
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_publish2");
                BOOST_TEST(topic == response_topic2);
                BOOST_TEST(pubopts.get_retain() == MQTT_NS::retain::yes);
                BOOST_TEST(contents == "response_contents");
                c->disconnect();
            });
        c->set_close_handler(
            [&]
            () {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_close1");
                        c->set_clean_session(false);
                        // session will expire on disconnect
                        c->connect(
                            MQTT_NS::v5::properties{
                                MQTT_NS::v5::property::request_response_information(true)
                            }
                        );
                    },
                    [&] {
                        MQTT_CHK("h_close2");
                        // no session exists even if clean false
                        c->set_clean_session(false);
                        c->connect(
                            MQTT_NS::v5::properties{
                                MQTT_NS::v5::property::request_response_information(true)
                            }
                        );
                    },
                    [&] {
                        MQTT_CHK("h_close3");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        c->set_clean_session(true);
        c->connect(
            MQTT_NS::v5::properties{
                MQTT_NS::v5::property::request_response_information(true),
                MQTT_NS::v5::property::session_expiry_interval(
                    MQTT_NS::session_never_expire
                )
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

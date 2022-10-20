// Copyright Takatoshi Kondo 20ss
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
#include <mqtt/setup_log.hpp>

BOOST_AUTO_TEST_SUITE(st_comp_token)

using namespace MQTT_NS::literals;


BOOST_AUTO_TEST_CASE( future ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {

        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

        auto wg = boost::asio::make_work_guard(ioc.get_executor());

        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("c_connect"),
            cont("g_connect"),

            cont("c_subscribe"),
            cont("g_subscribe"),

            cont("c_publish"),
            cont("g_publish"),

            cont("c_unsubscribe"),
            cont("g_unsubscribe"),

            cont("c_disconnect"),
            cont("g_disconnect"),
        };

        // multiple times called handlers is still callback
        c->set_close_handler(
            [&] {
            }
        );
        c->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
            }
        );

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&]
                (bool, MQTT_NS::connect_return_code) {
                });
            c->set_puback_handler(
                [&]
                (packet_id_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubrec_handler(
                [&]
                (std::uint16_t) {
                    BOOST_CHECK(false);
                });
            c->set_pubcomp_handler(
                [&]
                (std::uint16_t) {
                    BOOST_CHECK(false);
                });
            c->set_suback_handler(
                [&]
                (packet_id_t, std::vector<MQTT_NS::suback_return_code>) {
                });
            c->set_unsuback_handler(
                [&]
                (packet_id_t) {
                });
            c->set_publish_handler(
                [&]
                (MQTT_NS::optional<packet_id_t>,
                 MQTT_NS::publish_options,
                 MQTT_NS::buffer,
                 MQTT_NS::buffer) {
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&]
                (bool, MQTT_NS::v5::connect_reason_code, MQTT_NS::v5::properties) {
                });
            c->set_v5_puback_handler(
                [&]
                (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubrec_handler(
                [&]
                (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties) {
                    BOOST_CHECK(false);
                });
            c->set_v5_pubcomp_handler(
                [&]
                (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties) {
                    BOOST_CHECK(false);
                });
            c->set_v5_suback_handler(
                [&]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties) {
                });
            c->set_v5_unsuback_handler(
                [&]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties) {
                });
            c->set_v5_publish_handler(
                [&]
                (MQTT_NS::optional<packet_id_t>,
                 MQTT_NS::publish_options,
                 MQTT_NS::buffer,
                 MQTT_NS::buffer,
                 MQTT_NS::v5::properties) {
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        // future based code

        std::thread th_lib {
            [&] {
                try {
                    ioc.run();
                }
                catch (std::exception const& e) {
                    BOOST_TEST_INFO(e.what());
                    BOOST_CHECK(false);
                }
            }
        };

        // one-shot handler can be replaced with boost::asio::use_future
        {
            MQTT_CHK("c_connect");
            auto f = c->async_connect(boost::asio::use_future);
            try {
                f.get();
                MQTT_CHK("g_connect");
            }
            catch (std::exception const& e) {
                BOOST_TEST_INFO(e.what());
                BOOST_CHECK(false);
            }
        }
        {
            MQTT_CHK("c_subscribe");
            auto pid_sub = c->acquire_unique_packet_id();
            auto f = c->async_subscribe(pid_sub, "topic1", MQTT_NS::qos::exactly_once, boost::asio::use_future);
            try {
                f.get();
                MQTT_CHK("g_subscribe");
            }
            catch (std::exception const& e) {
                BOOST_TEST_INFO(e.what());
                BOOST_CHECK(false);
            }
        }
        {
            MQTT_CHK("c_publish");
            auto f = c->async_publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once, boost::asio::use_future);
            try {
                f.get();
                MQTT_CHK("g_publish");
            }
            catch (std::exception const& e) {
                BOOST_TEST_INFO(e.what());
                BOOST_CHECK(false);
            }
        }
        {
            MQTT_CHK("c_unsubscribe");
            auto pid_unsub = c->acquire_unique_packet_id();
            auto f = c->async_unsubscribe(pid_unsub, "topic1", boost::asio::use_future);
            try {
                f.get();
                MQTT_CHK("g_unsubscribe");
            }
            catch (std::exception const& e) {
                BOOST_TEST_INFO(e.what());
                BOOST_CHECK(false);
            }
        }
        {
            MQTT_CHK("c_disconnect");
            auto f = c->async_disconnect(boost::asio::use_future);
            try {
                f.get();
                MQTT_CHK("g_disconnect");
            }
            catch (std::exception const& e) {
                BOOST_TEST_INFO(e.what());
                BOOST_CHECK(false);
            }
        }

        wg.reset();
        finish();
        th_lib.join();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( user_strand ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {

        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

        auto str_user = boost::asio::make_strand(ioc.get_executor());
        auto wg = boost::asio::make_work_guard(ioc.get_executor());

        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            cont("h_suback"),
            cont("h_publish"),
            cont("h_unsuback"),
            cont("h_close"),
        };

        c->set_close_handler(
            [&] {
                MQTT_CHK("h_close");
                finish();
                wg.reset();
            }
        );
        c->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
            }
        );

        switch (c->get_protocol_version()) {

        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (bool, MQTT_NS::connect_return_code) {
                        MQTT_CHK("h_connack");
                        BOOST_TEST(str_user.running_in_this_thread());
                        auto pid_sub = c->acquire_unique_packet_id();
                        c->async_subscribe(
                            pid_sub,
                            "topic1",
                            MQTT_NS::qos::exactly_once,
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_puback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_pubrec_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (std::uint16_t) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_pubcomp_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (std::uint16_t) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_suback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, std::vector<MQTT_NS::suback_return_code>) {
                        MQTT_CHK("h_suback");
                        BOOST_TEST(str_user.running_in_this_thread());
                        c->async_publish(
                            "topic1",
                            "topic1_contents",
                            MQTT_NS::qos::at_most_once,
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_unsuback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t) {
                        MQTT_CHK("h_unsuback");
                        BOOST_TEST(str_user.running_in_this_thread());
                        c->async_disconnect(
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_publish_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (MQTT_NS::optional<packet_id_t>,
                     MQTT_NS::publish_options,
                     MQTT_NS::buffer,
                     MQTT_NS::buffer) {
                        MQTT_CHK("h_publish");
                        BOOST_TEST(str_user.running_in_this_thread());
                        auto pid_unsub = c->acquire_unique_packet_id();
                        c->async_unsubscribe(
                            pid_unsub,
                            "topic1",
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (bool, MQTT_NS::v5::connect_reason_code, MQTT_NS::v5::properties) {
                        MQTT_CHK("h_connack");
                        BOOST_TEST(str_user.running_in_this_thread());
                        auto pid_sub = c->acquire_unique_packet_id();
                        c->async_subscribe(
                            pid_sub,
                            "topic1",
                            MQTT_NS::qos::exactly_once,
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_v5_puback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_v5_pubrec_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_v5_pubcomp_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties) {
                        BOOST_CHECK(false);
                    }
                )
            );
            c->set_v5_suback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties) {
                        MQTT_CHK("h_suback");
                        BOOST_TEST(str_user.running_in_this_thread());
                        c->async_publish(
                            "topic1",
                            "topic1_contents",
                            MQTT_NS::qos::at_most_once,
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_v5_unsuback_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties) {
                        MQTT_CHK("h_unsuback");
                        BOOST_TEST(str_user.running_in_this_thread());
                        c->async_disconnect(
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            c->set_v5_publish_handler(
                boost::asio::bind_executor(
                    str_user,
                    [&]
                    (MQTT_NS::optional<packet_id_t>,
                     MQTT_NS::publish_options,
                     MQTT_NS::buffer,
                     MQTT_NS::buffer,
                     MQTT_NS::v5::properties) {
                        MQTT_CHK("h_publish");
                        BOOST_TEST(str_user.running_in_this_thread());
                        auto pid_unsub = c->acquire_unique_packet_id();
                        c->async_unsubscribe(
                            pid_unsub,
                            "topic1",
                            boost::asio::bind_executor(
                                str_user,
                                [&](MQTT_NS::error_code) {
                                    BOOST_TEST(str_user.running_in_this_thread());
                                }
                            )
                        );
                    }
                )
            );
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->async_connect(
            boost::asio::bind_executor(
                str_user,
                [&](MQTT_NS::error_code) {
                    BOOST_TEST(str_user.running_in_this_thread());
                }
            )
        );

        std::thread th_user {
            [&] {
                try {
                    ioc.run();
                }
                catch (std::exception const& e) {
                    BOOST_TEST_INFO(e.what());
                    BOOST_CHECK(false);
                }
            }
        };
        th_user.join();

    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_SUITE_END()

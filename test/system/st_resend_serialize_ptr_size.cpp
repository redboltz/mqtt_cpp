﻿// Copyright Takatoshi Kondo 2018
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

BOOST_AUTO_TEST_SUITE(st_resend_serialize_ptr_size)

using namespace MQTT_NS::literals;

template <typename Client, typename Elem>
inline
void restore_serialized_message(Client const& c, Elem const& e) {
    c->restore_serialized_message(e.second.begin(), e.second.end());
}

template <typename Client, typename Serialized>
inline
void set_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_serialize_handlers(
        [&serialized](packet_id_t packet_id, char const* data, std::size_t size) {
            serialized.emplace(packet_id, std::string(data, size));
        },
        [&serialized](packet_id_t packet_id, char const* data, std::size_t size) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized[packet_id] = std::string(data, size);
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}

BOOST_AUTO_TEST_CASE( publish_qos1 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_puback"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::qos::at_least_once);
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_puback_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
        });

    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( publish_qos2 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
            cont("h_pubrec"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_pubrec_handler(
        [&chk, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
        });
    c2->set_pubcomp_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pubrel_qos2 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        cont("h_pubrec"),
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });
    c1->set_pubrec_handler(
        [&chk, &c1, &pid_pub]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            c1->force_disconnect();
        });

    c2->set_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_pubcomp_handler(
        [&chk, &c2]
        (packet_id_t packet_id) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == 1);
            c2->disconnect();
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_serialize_handlers(c1, serialized);
    set_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub1;
    std::uint16_t pid_pub2;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error1"),
        // connect
        cont("h_connack3"),
        cont("h_puback1"),
        cont("h_puback2"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_connack_handler(
        [&chk, &c1, &pid_pub1, &pid_pub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == false);
                    pid_pub1 = c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_least_once);
                    pid_pub2 = c1->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error1");
            for (auto const& e : serialized) {
                restore_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });
    c2->set_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::connect_return_code connack_return_code) {
            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_puback_handler(
        [&chk, &c2, &pid_pub1, &pid_pub2]
        (packet_id_t packet_id) {
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_puback1");
                    BOOST_TEST(packet_id == pid_pub1);
                },
                [&] {
                    MQTT_CHK("h_puback2");
                    BOOST_TEST(packet_id == pid_pub2);
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

template <typename Client, typename Elem>
inline
void restore_v5_serialized_message(Client const& c, Elem const& e) {
    c->restore_v5_serialized_message(e.second.begin(), e.second.end());
}

template <typename Client, typename Serialized>
inline
void set_v5_serialize_handlers(Client const& c, Serialized& serialized) {
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    c->set_v5_serialize_handlers(
        [&serialized](packet_id_t packet_id, char const* data, std::size_t size) {
            serialized.emplace(packet_id, std::string(data, size));
        },
        [&serialized](packet_id_t packet_id, char const* data, std::size_t size) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized[packet_id] = std::string(data, size);
        },
        [&serialized](packet_id_t packet_id) {
            BOOST_CHECK(serialized.find(packet_id) != serialized.end());
            serialized.erase(packet_id);
        }
    );
}

BOOST_AUTO_TEST_CASE( publish_qos1_v5 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_puback"),
        // disconnect
        cont("h_close2"),
    };

    MQTT_NS::v5::properties ps {
        MQTT_NS::v5::property::payload_format_indicator(MQTT_NS::v5::property::payload_format_indicator::string),
        MQTT_NS::v5::property::message_expiry_interval(0x12345678UL),
        MQTT_NS::v5::property::topic_alias(0x1234U),
        MQTT_NS::v5::property::response_topic("response topic"_mb),
        MQTT_NS::v5::property::correlation_data("correlation data"_mb),
        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
    };

    std::size_t user_prop_count = 0;
    b.set_publish_props_handler(
        [&user_prop_count, size = ps.size()] (MQTT_NS::v5::properties const& props) {
            BOOST_TEST(props.size() == size);

            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&](MQTT_NS::v5::property::payload_format_indicator const& t) {
                            BOOST_TEST(t.val() == MQTT_NS::v5::property::payload_format_indicator::string);
                        },
                        [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
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
                            case 2:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 3:
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
        }
    );

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub, ps = std::move(ps)]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    // The previous connection is not set Session Expiry Interval.
                    // That means session state is cleared on close.
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::retain::no | MQTT_NS::qos::at_least_once, std::move(ps));
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_v5_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_v5_puback_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_puback");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
        });

    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( publish_qos2_v5 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
            cont("h_pubrec"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    // The previous connection is not set Session Expiry Interval.
                    // That means session state is cleared on close.
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_v5_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_v5_pubrec_handler(
        [&chk, &pid_pub]
        (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
        });
    c2->set_v5_pubcomp_handler(
        [&chk, &c2, &pid_pub]
        (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == pid_pub);
            c2->disconnect();
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( pubrel_qos2_v5 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);
    c1->set_auto_pub_response(false);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS2
        cont("h_pubrec"),
        // force_disconnect
        cont("h_error"),
        // connect
        cont("h_connack3"),
        cont("h_pubcomp"),
        // disconnect
        cont("h_close2"),
    };

    MQTT_NS::v5::properties ps {
        MQTT_NS::v5::property::reason_string("test success"_mb),
        MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
        MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
    };
    std::size_t user_prop_count = 0;

    b.set_pubrel_props_handler(
        [&user_prop_count, size = ps.size()] (MQTT_NS::v5::properties const& props) {
            BOOST_TEST(props.size() == size);
            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&](MQTT_NS::v5::property::reason_string const& t) {
                            BOOST_TEST(t.val() == "test success");
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
                            case 2:
                                BOOST_TEST(t.key() == "key1");
                                BOOST_TEST(t.val() == "val1");
                                break;
                            case 3:
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
        }
    );

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    // The previous connection is not set Session Expiry Interval.
                    // That means session state is cleared on close.
                    BOOST_TEST(sp == false);
                    pid_pub = c1->publish("topic1", "topic1_contents", MQTT_NS::qos::exactly_once);
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error");
            for (auto const& e : serialized) {
                restore_v5_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });
    c1->set_v5_pubrec_handler(
        [&chk, &c1, &pid_pub, ps = std::move(ps)]
        (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_pubrec");
            BOOST_TEST(packet_id == pid_pub);
            c1->pubrel(packet_id, MQTT_NS::v5::pubrel_reason_code::success, std::move(ps));
            c1->force_disconnect();
        });

    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_v5_pubcomp_handler(
        [&chk, &c2]
        (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code, MQTT_NS::v5::properties /*props*/) {
            MQTT_CHK("h_pubcomp");
            BOOST_TEST(packet_id == 1);
            c2->disconnect();
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_CASE( multi_publish_qos1_v5 ) {
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

    auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c1->set_client_id("cid1");
    c1->set_clean_session(true);

    auto c2 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port, MQTT_NS::protocol_version::v5);
    c2->set_client_id("cid1");

    using packet_id_t = typename std::remove_reference_t<decltype(*c1)>::packet_id_t;

    std::map<packet_id_t, std::string> serialized;

    set_v5_serialize_handlers(c1, serialized);
    set_v5_serialize_handlers(c2, serialized);

    std::uint16_t pid_pub1;
    std::uint16_t pid_pub2;

    boost::asio::steady_timer tim(ioc);

    checker chk = {
        cont("start"),
        // connect
        cont("h_connack1"),
        // disconnect
        cont("h_close1"),
        // connect
        cont("h_connack2"),
        // publish topic1 QoS1
        // publish topic1 QoS1
        // force_disconnect
        cont("h_error1"),
        // connect
        cont("h_connack3"),
        cont("h_puback1"),
        cont("h_puback2"),
        // disconnect
        cont("h_close2"),
    };

    c1->set_v5_connack_handler(
        [&chk, &c1, &pid_pub1, &pid_pub2]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    c1->disconnect();
                },
                [&] {
                    MQTT_CHK("h_connack2");
                    // The previous connection is not set Session Expiry Interval.
                    // That means session state is cleared on close.
                    BOOST_TEST(sp == false);
                    pid_pub1 = c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_least_once);
                    pid_pub2 = c1->publish("topic1", "topic1_contents2", MQTT_NS::qos::at_least_once);
                    c1->force_disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    c1->set_close_handler(
        [&chk, &c1]
        () {
            MQTT_CHK("h_close1");
            connect_no_clean(c1);
        });
    c1->set_error_handler(
        [&chk, &c2, &serialized, &tim]
        (MQTT_NS::error_code) {
            MQTT_CHK("h_error1");
            for (auto const& e : serialized) {
                restore_v5_serialized_message(c2, e);
            }
            // TCP level disconnection detecting timing is unpredictable.
            // Sometimes broker first, sometimes the client (this test) first.
            // This test assume that the broker detects first, so I set timer.
            // If client detect the disconnection first, then reconnect with
            // existing client id. And it is overwritten at broker.
            // Then error handler in the broker called, assertion failed due to
            // no corresponding connection exists
            tim.expires_after(std::chrono::milliseconds(100));
            tim.async_wait(
                [&c2] (MQTT_NS::error_code ec) {
                    BOOST_ASSERT( ! ec);
                    connect_no_clean(c2);
                }
            );
        });
    c2->set_v5_connack_handler(
        [&chk]
        (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
            BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
            MQTT_CHK("h_connack3");
            BOOST_TEST(sp == true);
        });
    c2->set_close_handler(
        [&chk, &finish]
        () {
            MQTT_CHK("h_close2");
            finish();
        });
    c2->set_v5_puback_handler(
        [&chk, &c2, &pid_pub1, &pid_pub2]
        (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code, MQTT_NS::v5::properties /*props*/) {
            auto ret = MQTT_ORDERED(
                [&] {
                    MQTT_CHK("h_puback1");
                    BOOST_TEST(packet_id == pid_pub1);
                },
                [&] {
                    MQTT_CHK("h_puback2");
                    BOOST_TEST(packet_id == pid_pub2);
                    c2->disconnect();
                }
            );
            BOOST_TEST(ret);
        });
    MQTT_CHK("start");
    c1->connect();
    ioc.run();
    BOOST_TEST(serialized.empty() == true);
    BOOST_TEST(chk.all());
    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

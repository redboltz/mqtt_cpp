// Copyright Takatoshi Kondo 2018
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

BOOST_AUTO_TEST_SUITE(st_as_buffer_sub)

using namespace std::literals::string_literals;

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe(
                        std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        }
                    );
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe( std::vector<MQTT_NS::string_view>{"topic1", "topic2"} );
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    c->subscribe(
                        std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        }
                    );
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe( std::vector<MQTT_NS::string_view>{"topic1", "topic2"});
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v
                        {
                            { "topic1", MQTT_NS::qos::at_most_once },
                            { "topic2", MQTT_NS::qos::exactly_once }
                        };
                    c->subscribe(std::move(v));
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe(std::vector<MQTT_NS::string_view>{"topic1", "topic2"});
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v
                        {
                            { "topic1", MQTT_NS::qos::at_most_once },
                            { "topic2", MQTT_NS::qos::exactly_once }
                        };
                    c->subscribe(std::move(v));
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe(std::vector<MQTT_NS::string_view>{"topic1", "topic2"});
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single_async ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    auto topic = std::make_shared<std::string>("topic1");
                    c->async_subscribe(
                        as::buffer(*topic),
                        MQTT_NS::qos::at_most_once,
                        [topic](MQTT_NS::error_code) {});
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    auto topic = std::make_shared<std::string>("topic1");
                    c->async_unsubscribe(
                        as::buffer(*topic),
                        [topic](MQTT_NS::error_code) {});
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    auto topic = std::make_shared<std::string>("topic1");
                    c->async_subscribe(
                        as::buffer(*topic),
                        MQTT_NS::qos::at_most_once,
                        [topic](MQTT_NS::error_code) {});
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    auto topic = std::make_shared<std::string>("topic1");
                    c->async_unsubscribe(
                        as::buffer(*topic),
                        [topic](MQTT_NS::error_code) {});
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg_async ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    c->async_subscribe(
                        std::vector<std::tuple<as::const_buffer, MQTT_NS::subscribe_options>>
                        {
                            {as::buffer(*topic1), MQTT_NS::qos::at_most_once},
                            {as::buffer(*topic2), MQTT_NS::qos::exactly_once}
                        },
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    c->async_unsubscribe(
                        std::vector<as::const_buffer>{ as::buffer(*topic1), as::buffer(*topic2) },
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    c->async_subscribe(
                        std::vector<std::tuple<as::const_buffer, MQTT_NS::subscribe_options>>
                        {
                            {as::buffer(*topic1), MQTT_NS::qos::at_most_once},
                            {as::buffer(*topic2), MQTT_NS::qos::exactly_once}
                        },
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    c->async_unsubscribe(
                        std::vector<as::const_buffer>{ as::buffer(*topic1), as::buffer(*topic2) },
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec_async ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe topic1
            cont("h_suback"),
            // unsubscribe topic1
            cont("h_unsuback"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    std::vector<std::tuple<as::const_buffer, MQTT_NS::subscribe_options>> v
                    {
                        { as::buffer(*topic1), MQTT_NS::qos::at_most_once },
                        { as::buffer(*topic2), MQTT_NS::qos::exactly_once }
                    };
                    c->async_subscribe(
                        std::move(v),
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    std::vector<as::const_buffer> v
                        {
                            as::buffer(*topic1),
                            as::buffer(*topic2),
                        };
                    c->async_unsubscribe(
                        std::move(v),
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    std::vector<std::tuple<as::const_buffer, MQTT_NS::subscribe_options>> v
                        {
                            { as::buffer(*topic1), MQTT_NS::qos::at_most_once },
                            { as::buffer(*topic2), MQTT_NS::qos::exactly_once }
                        };
                    c->async_subscribe(
                        std::move(v),
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::suback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    auto topic1 = std::make_shared<std::string>("topic1");
                    auto topic2 = std::make_shared<std::string>("topic2");
                    std::vector<as::const_buffer> v
                        {
                            as::buffer(*topic1),
                            as::buffer(*topic2),
                        };
                    c->async_unsubscribe(
                        std::move(v),
                        [topic1, topic2](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t, std::vector<MQTT_NS::v5::unsuback_reason_code>, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

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
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_SUITE_END()

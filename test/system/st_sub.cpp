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
#include <mqtt/subscribe_options.hpp>

BOOST_AUTO_TEST_SUITE(st_sub)

using namespace MQTT_NS::literals;
using namespace std::literals::string_literals;

BOOST_AUTO_TEST_CASE( qos0_sub_string_single ) {
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
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> results) {
                    MQTT_CHK("h_suback");
                    BOOST_TEST(results.size() == 1);
                    BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
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
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(reasons.size() == 1);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
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

BOOST_AUTO_TEST_CASE( sub_update ) {
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
            cont("h_suback_1"), // QoS0
            cont("h_suback_2"), // QoS1
            cont("h_suback_3"), // QoS2
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
                    c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                    c->subscribe("topic1", MQTT_NS::qos::exactly_once);
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> results) {
                    BOOST_TEST(results.size() == 1);
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_suback_1");
                            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_0);
                        },
                        [&] {
                            MQTT_CHK("h_suback_2");
                            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_1);
                        },
                        [&] {
                            MQTT_CHK("h_suback_3");
                            BOOST_TEST(results[0] == MQTT_NS::suback_return_code::success_maximum_qos_2);
                            c->unsubscribe("topic1");
                        }
                    );
                    BOOST_TEST(ret);
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
                    c->subscribe("topic1", MQTT_NS::qos::at_least_once);
                    c->subscribe("topic1", MQTT_NS::qos::exactly_once);
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    BOOST_TEST(reasons.size() == 1);
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_suback_1");
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_0);
                        },
                        [&] {
                            MQTT_CHK("h_suback_2");
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_1);
                        },
                        [&] {
                            MQTT_CHK("h_suback_3");
                            BOOST_TEST(reasons[0] == MQTT_NS::v5::suback_reason_code::granted_qos_2);
                            c->unsubscribe("topic1");
                        }
                    );
                    BOOST_TEST(ret);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> reasons, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_unsuback");
                    BOOST_TEST(reasons.size() == 1);
                    BOOST_TEST(reasons[0] == MQTT_NS::v5::unsuback_reason_code::success);
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

BOOST_AUTO_TEST_CASE( sub_v5_options ) {
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
                    c->subscribe("topic1",   MQTT_NS::nl::yes
                                           | MQTT_NS::rap::retain
                                           | MQTT_NS::qos::at_most_once
                                           | MQTT_NS::retain_handling::not_send);
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( qos0_sub_string_multi_arg ) {
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
                    c->unsubscribe( std::vector<MQTT_NS::string_view>{ MQTT_NS::string_view{"topic1"}, MQTT_NS::string_view{"topic2"} } );
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
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe( std::vector<MQTT_NS::string_view>{ MQTT_NS::string_view{"topic1"}, MQTT_NS::string_view{"topic2"} });
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( qos0_sub_string_multi_vec ) {
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
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->subscribe(v);
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    std::vector<MQTT_NS::string_view> v
                        {
                         "topic1",
                         "topic2",
                        };
                    c->unsubscribe(v);
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
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->subscribe(v);
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    std::vector<MQTT_NS::string_view> v
                        {
                         "topic1",
                         "topic2",
                        };
                    c->unsubscribe(v);
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( qos0_sub_string_single_async ) {
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
                    c->async_subscribe("topic1", MQTT_NS::qos::at_most_once, [](MQTT_NS::error_code) {});
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe("topic1", [](MQTT_NS::error_code) {});
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
                    c->async_subscribe("topic1", MQTT_NS::qos::at_most_once, [](MQTT_NS::error_code) {});
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe("topic1", [](MQTT_NS::error_code) {});
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( qos0_sub_string_multi_arg_async ) {
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
                    c->async_subscribe(
                        std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>>
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        },
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe(
                        std::vector<std::string> {
                            "topic1",
                            "topic2"
                        },
                        [](MQTT_NS::error_code) {}
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
                    c->async_subscribe(
                        std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>>
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        },
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe(
                        std::vector<std::string> {
                            "topic1",
                            "topic2"
                        },
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( qos0_sub_string_multi_vec_async ) {
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
                    std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->async_subscribe(
                        v,
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                    MQTT_CHK("h_suback");
                    std::vector<std::string> v
                        {
                            "topic1",
                            "topic2",
                        };
                    c->async_unsubscribe(
                        v,
                        [](MQTT_NS::error_code) {}
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
                    std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->async_subscribe(
                        v,
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_suback");
                    std::vector<std::string> v
                        {
                            "topic1",
                            "topic2",
                        };
                    c->async_unsubscribe(
                        v,
                        [](MQTT_NS::error_code) {}
                    );
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
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

BOOST_AUTO_TEST_CASE( sub_unsub_prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

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

        MQTT_NS::v5::properties subps {
            MQTT_NS::v5::property::subscription_identifier(268435455UL),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb)
        };

        MQTT_NS::v5::properties unsubps {
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb)
        };

        std::size_t sub_user_prop_count = 0;
        std::size_t unsub_user_prop_count = 0;

        b.set_subscribe_props_handler(
            [&sub_user_prop_count, size = subps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::subscription_identifier const& t) {
                                BOOST_TEST(t.val() == 268435455UL);
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (sub_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        b.set_unsubscribe_props_handler(
            [&unsub_user_prop_count, size = unsubps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (unsub_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
            }
        );

        c->set_v5_connack_handler(
            [&chk, &c, &subps]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once, subps);
            });
        c->set_v5_suback_handler(
            [&chk, &c, &unsubps]
            (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_suback");
                c->unsubscribe("topic1", unsubps);
            });
        c->set_v5_unsuback_handler(
            [&chk, &c]
            (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_unsuback");
                c->disconnect();
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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( suback_unsuback_prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

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

        MQTT_NS::v5::properties subackps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto suback_prop_size = subackps.size();
        b.set_suback_props(std::move(subackps));

        MQTT_NS::v5::properties unsubackps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto unsuback_prop_size = unsubackps.size();
        b.set_unsuback_props(std::move(unsubackps));

        std::size_t sub_user_prop_count = 0;
        std::size_t unsub_user_prop_count = 0;

        c->set_v5_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once);
            });
        c->set_v5_suback_handler(
            [&chk, &c, &sub_user_prop_count, suback_prop_size]
            (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::suback_reason_code> /*reasons*/, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_suback");
                BOOST_TEST(props.size() == suback_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (sub_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                c->unsubscribe("topic1");
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &unsub_user_prop_count, unsuback_prop_size]
            (packet_id_t /*packet_id*/, std::vector<MQTT_NS::v5::unsuback_reason_code> /*reasons*/, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(props.size() == unsuback_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test success");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (unsub_user_prop_count++) {
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
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }
                c->disconnect();
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
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

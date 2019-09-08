// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>
#include <mqtt/subscribe_options.hpp>

BOOST_AUTO_TEST_SUITE(test_sub)

using namespace MQTT_NS::literals;
using namespace std::literals::string_literals;

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( sub_v5_options ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe("topic1",   MQTT_NS::nl::yes
                                           | MQTT_NS::rap::retain
                                           | MQTT_NS::qos::at_most_once
                                           | MQTT_NS::retain_handling::not_send);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe("topic1");
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe(
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        }
                    );
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe( { MQTT_NS::string_view{"topic1"}, MQTT_NS::string_view{"topic2"} } );
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->subscribe(
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        }
                    );
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    c->unsubscribe( { MQTT_NS::string_view{"topic1"}, MQTT_NS::string_view{"topic2"} });
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->subscribe(v);
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    std::vector<MQTT_NS::string_view> v
                        {
                         "topic1",
                         "topic2",
                        };
                    c->unsubscribe(v);
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->subscribe(v);
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    std::vector<MQTT_NS::string_view> v
                        {
                         "topic1",
                         "topic2",
                        };
                    c->unsubscribe(v);
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_subscribe("topic1", MQTT_NS::qos::at_most_once, [](boost::system::error_code const&) {});
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe("topic1", [](boost::system::error_code const&) {});
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_subscribe("topic1", MQTT_NS::qos::at_most_once, [](boost::system::error_code const&) {});
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe("topic1", [](boost::system::error_code const&) {});
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_subscribe(
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        },
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe(
                        std::vector<std::string> {
                            "topic1",
                            "topic2"
                        },
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_subscribe(
                        {
                            {"topic1", MQTT_NS::qos::at_most_once},
                            {"topic2", MQTT_NS::qos::exactly_once}
                        },
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    c->async_unsubscribe(
                        std::vector<std::string> {
                            "topic1",
                            "topic2"
                        },
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
                (bool sp, std::uint8_t connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->async_subscribe(
                        v,
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<MQTT_NS::optional<std::uint8_t>> /*results*/) {
                    MQTT_CHK("h_suback");
                    std::vector<std::string> v
                        {
                            "topic1",
                            "topic2",
                        };
                    c->async_unsubscribe(
                        v,
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>> v;
                    v.emplace_back("topic1", MQTT_NS::qos::at_most_once);
                    v.emplace_back("topic2", MQTT_NS::qos::exactly_once);
                    c->async_subscribe(
                        v,
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_suback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_suback");
                    std::vector<std::string> v
                        {
                            "topic1",
                            "topic2",
                        };
                    c->async_unsubscribe(
                        v,
                        [](boost::system::error_code const&) {}
                    );
                    return true;
                });
            c->set_v5_unsuback_handler(
                [&chk, &c]
                (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                    MQTT_CHK("h_unsuback");
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( sub_unsub_prop ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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

        std::vector<MQTT_NS::v5::property_variant> subps {
            MQTT_NS::v5::property::subscription_identifier(268435455UL),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb)
        };

        std::vector<MQTT_NS::v5::property_variant> unsubps {
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb)
        };

        std::size_t sub_user_prop_count = 0;
        std::size_t unsub_user_prop_count = 0;

        b.set_subscribe_props_handler(
            [&sub_user_prop_count, size = subps.size()] (std::vector<MQTT_NS::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor<void>(
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
            [&unsub_user_prop_count, size = unsubps.size()] (std::vector<MQTT_NS::v5::property_variant> const& props) {
                BOOST_TEST(props.size() == size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor<void>(
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
            (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once, subps);
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c, &unsubps]
            (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                MQTT_CHK("h_suback");
                c->unsubscribe("topic1", unsubps);
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c]
            (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                MQTT_CHK("h_unsuback");
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( suback_unsuback_prop ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& b) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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

        std::vector<MQTT_NS::v5::property_variant> subackps {
            MQTT_NS::v5::property::reason_string("test success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
        };
        auto suback_prop_size = subackps.size();
        b.set_suback_props(std::move(subackps));

        std::vector<MQTT_NS::v5::property_variant> unsubackps {
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
            (bool sp, std::uint8_t connack_return_code, std::vector<MQTT_NS::v5::property_variant> /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                return true;
            });
        c->set_v5_suback_handler(
            [&chk, &c, &sub_user_prop_count, suback_prop_size]
            (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> props) {
                MQTT_CHK("h_suback");
                BOOST_TEST(props.size() == suback_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor<void>(
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
                return true;
            });
        c->set_v5_unsuback_handler(
            [&chk, &c, &unsub_user_prop_count, unsuback_prop_size]
            (packet_id_t /*packet_id*/, std::vector<std::uint8_t> /*reasons*/, std::vector<MQTT_NS::v5::property_variant> props) {
                MQTT_CHK("h_unsuback");
                BOOST_TEST(props.size() == unsuback_prop_size);
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor<void>(
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
                return true;
            });
        c->set_close_handler(
            [&chk, &s]
            () {
                MQTT_CHK("h_close");
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

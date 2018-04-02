// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_as_buffer_sub)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                std::string topic1("topic1");
                c->subscribe(as::buffer(topic1), mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                std::string topic1("topic1");
                c->unsubscribe(as::buffer(topic1));
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                std::string topic1("topic1");
                std::string topic2("topic2");
                c->subscribe(
                    as::buffer(topic1), mqtt::qos::at_most_once,
                    as::buffer(topic2), mqtt::qos::exactly_once);
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                std::string topic1("topic1");
                std::string topic2("topic2");
                c->unsubscribe(as::buffer(topic1), as::buffer(topic2));
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                std::vector<std::tuple<as::const_buffer, std::uint8_t>> v;
                std::string topic1("topic1");
                std::string topic2("topic2");
                v.emplace_back(as::buffer(topic1), mqtt::qos::at_most_once);
                v.emplace_back(as::buffer(topic2), mqtt::qos::exactly_once);
                c->subscribe(v);
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                std::string topic1("topic1");
                std::string topic2("topic2");
                std::vector<as::const_buffer> v
                    {
                        as::buffer(topic1),
                        as::buffer(topic2),
                    };
                c->unsubscribe(v);
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_single_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                auto topic = std::make_shared<std::string>("topic1");
                c->async_subscribe(
                    as::buffer(*topic),
                    mqtt::qos::at_most_once,
                    [topic](boost::system::error_code const&) {});
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                auto topic = std::make_shared<std::string>("topic1");
                c->async_unsubscribe(
                    as::buffer(*topic),
                    [topic](boost::system::error_code const&) {});
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_arg_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                auto topic1 = std::make_shared<std::string>("topic1");
                auto topic2 = std::make_shared<std::string>("topic2");
                c->async_subscribe(
                    as::buffer(*topic1), mqtt::qos::at_most_once,
                    as::buffer(*topic2), mqtt::qos::exactly_once,
                    [topic1, topic2](boost::system::error_code const&) {}
                );
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                auto topic1 = std::make_shared<std::string>("topic1");
                auto topic2 = std::make_shared<std::string>("topic2");
                c->async_unsubscribe(
                    as::buffer(*topic1),
                    as::buffer(*topic2),
                    [topic1, topic2](boost::system::error_code const&) {}
                );
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( pub_qos0_sub_string_multi_vec_async ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe topic1
            "h_suback",
            // unsubscribe topic1
            "h_unsuback",
            // disconnect
            "h_close",
            "finish",
        };

        auto current =
            [&order, &expected]() -> std::string {
                try {
                    return expected.at(order);
                }
                catch (std::out_of_range const& e) {
                    return e.what();
                }
            };

        c->set_connack_handler(
            [&order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                auto topic1 = std::make_shared<std::string>("topic1");
                auto topic2 = std::make_shared<std::string>("topic2");
                std::vector<std::tuple<as::const_buffer, std::uint8_t>> v;
                v.emplace_back(as::buffer(*topic1), mqtt::qos::at_most_once);
                v.emplace_back(as::buffer(*topic2), mqtt::qos::exactly_once);
                c->async_subscribe(
                    v,
                    [topic1, topic2](boost::system::error_code const&) {}
                );
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s]
            () {
                BOOST_TEST(current() == "h_close");
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                auto topic1 = std::make_shared<std::string>("topic1");
                auto topic2 = std::make_shared<std::string>("topic2");
                std::vector<as::const_buffer> v
                    {
                        as::buffer(*topic1),
                        as::buffer(*topic2),
                    };
                c->async_unsubscribe(
                    v,
                    [topic1, topic2](boost::system::error_code const&) {}
                );
                return true;
            });
        c->set_unsuback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/) {
                BOOST_TEST(current() == "h_unsuback");
                ++order;
                c->disconnect();
                return true;
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <iostream>
#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_length_check)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;
        int lc_order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // subscribe
            "h_lc_suback",
            "h_suback",
            // publish
            "h_lc_publish", // fail
            "h_error",
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
                c->subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&current, &order, &s, &c]
            (boost::system::error_code const& ec) {
                BOOST_TEST(current() == "h_error");
                ++order;
                BOOST_TEST(ec == boost::system::errc::message_size);
                s.close();
                c->force_disconnect();
            });
        c->set_suback_handler(
            [&order, &current, &c]
            (std::uint16_t /*packet_id*/, std::vector<boost::optional<std::uint8_t>> /*results*/) {
                BOOST_TEST(current() == "h_suback");
                ++order;
                c->publish_at_most_once("topic1", "topic1_contents");
                return true;
            });
        c->set_publish_handler(
            []
            (std::uint8_t /*header*/,
             boost::optional<std::uint16_t> /*packet_id*/,
             std::string /*topic*/,
             std::string /*contents*/) {
                BOOST_CHECK(false);
                return false;
            });
        c->set_is_valid_length_handler(
            [&order, &current, &lc_order]
            (std::uint8_t cpt, std::size_t /*len*/) {
                switch (lc_order++) {
                case 0:
                    BOOST_TEST(cpt == mqtt::control_packet_type::suback);
                    BOOST_TEST(current() == "h_lc_suback");
                    ++order;
                    return true;
                case 1:
                    BOOST_TEST(cpt == mqtt::control_packet_type::publish);
                    BOOST_TEST(current() == "h_lc_publish");
                    ++order;
                    return false;
                default:
                    BOOST_CHECK(false);
                    return false;
                }
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_SUITE_END()

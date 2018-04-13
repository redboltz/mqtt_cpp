// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

BOOST_AUTO_TEST_SUITE(test_connect)

BOOST_AUTO_TEST_CASE( connect ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
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
                BOOST_TEST(c->connected() == true);
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                BOOST_TEST(c->connected() == true);
                return true;
            });
        c->set_close_handler(
            [&order, &current, &s, &c]
            () {
                BOOST_TEST(current() == "h_close");
                BOOST_TEST(c->connected() == false);
                ++order;
                s.close();
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        BOOST_TEST(c->connected() == false);
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( connect_no_strand ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
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
                c->disconnect();
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
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( keep_alive ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            "h_pingresp",
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
            [&order, &current]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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
        c->set_pingresp_handler(
            [&order, &current, &c]
            () {
                BOOST_TEST(current() == "h_pingresp");
                ++order;
                c->disconnect();
                return true;
            });
        c->set_keep_alive_sec(3);
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( keep_alive_and_send_control_packet ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            "2sec",
            "h_pingresp",
            "4sec_cancelled",
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

        boost::asio::deadline_timer tim(ios);
        c->set_connack_handler(
            [&order, &current, &c, &tim]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                tim.expires_from_now(boost::posix_time::seconds(2));
                tim.async_wait(
                    [&order, &current, &c, &tim](boost::system::error_code const& ec) {
                        BOOST_TEST(current() == "2sec");
                        ++order;
                        BOOST_CHECK(!ec);
                        c->publish_at_most_once("topic1", "timer_reset");
                        tim.expires_from_now(boost::posix_time::seconds(4));
                        tim.async_wait(
                            [&order, &current](boost::system::error_code const& ec) {
                                BOOST_TEST(current() == "4sec_cancelled");
                                ++order;
                                BOOST_TEST(ec == boost::asio::error::operation_aborted );
                            }
                        );
                    }
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
        c->set_pingresp_handler(
            [&order, &current, &c, &tim]
            () {
                BOOST_TEST(current() == "h_pingresp");
                ++order;
                tim.cancel();
                c->disconnect();
                return true;
            });
        c->set_keep_alive_sec_ping_ms(3, 3 * 1000);
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( connect_again ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        bool first = true;
        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack1",
            // disconnect
            "h_close1",
            // connect
            "h_connack2",
            // disconnect
            "h_close2",
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
            [&first, &order, &current, &c]
            (bool sp, std::uint8_t connack_return_code) {
                if (first) {
                    BOOST_TEST(current() == "h_connack1");
                    ++order;
                }
                else {
                    BOOST_TEST(current() == "h_connack2");
                    ++order;
                }
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&first, &order, &current, &c, &s]
            () {
                if (first) {
                    BOOST_TEST(current() == "h_close1");
                    ++order;
                    first = false;
                    c->connect();
                }
                else {
                    BOOST_TEST(current() == "h_close2");
                    ++order;
                    s.close();
                }
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( nocid ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
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
                c->disconnect();
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
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( nocid_noclean ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // disconnect
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
            [&order, &current]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::identifier_rejected);
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&order, &current, &s]
            (boost::system::error_code const&) {
                BOOST_TEST(current() == "h_error");
                ++order;
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( noclean ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack1",
            // disconnect
            "h_close1",
            // connect
            "h_connack2",
            // disconnect
            "h_close2",
            // connect
            "h_connack3",
            // disconnect
            "h_close3",
            // connect
            "h_connack4",
            // disconnect
            "h_close4",
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

        int connect = 0;
        c->set_connack_handler(
            [&order, &current, &connect, &c]
            (bool sp, std::uint8_t connack_return_code) {
                switch (connect) {
                case 0:
                    BOOST_TEST(current() == "h_connack1");
                    ++order;
                    BOOST_TEST(sp == false);
                    break;
                case 1:
                    BOOST_TEST(current() == "h_connack2");
                    ++order;
                    BOOST_TEST(sp == true);
                    break;
                case 2:
                    BOOST_TEST(current() == "h_connack3");
                    ++order;
                    BOOST_TEST(sp == false);
                    break;
                case 3:
                    BOOST_TEST(current() == "h_connack4");
                    ++order;
                    BOOST_TEST(sp == false);
                    break;
                }
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&order, &current, &connect, &c, &s]
            () {
                switch (connect) {
                case 0:
                    BOOST_TEST(current() == "h_close1");
                    ++order;
                    c->connect();
                    ++connect;
                    break;
                case 1:
                    BOOST_TEST(current() == "h_close2");
                    ++order;
                    c->set_clean_session(true);
                    c->connect();
                    ++connect;
                    break;
                case 2:
                    BOOST_TEST(current() == "h_close3");
                    ++order;
                    c->set_clean_session(false);
                    c->connect();
                    ++connect;
                    break;
                case 3:
                    BOOST_TEST(current() == "h_close4");
                    ++order;
                    s.close();
                    break;
                }
            });
        c->set_error_handler(
            []
            (boost::system::error_code const&) {
                BOOST_CHECK(false);
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( disconnect_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // disconnect
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
            [&order, &current, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(2));
                c->disconnect(boost::posix_time::seconds(1));
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&order, &current, &s]
            (boost::system::error_code const&) {
                BOOST_TEST(current() == "h_error");
                ++order;
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( disconnect_not_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
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
            [&order, &current, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(1));
                c->disconnect(boost::posix_time::seconds(2));
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
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( async_disconnect_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
            // disconnect
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
            [&order, &current, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(2));
                c->async_disconnect(boost::posix_time::seconds(1));
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&order, &current, &s]
            (boost::system::error_code const&) {
                BOOST_TEST(current() == "h_error");
                ++order;
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_CASE( async_disconnect_not_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        int order = 0;

        std::vector<std::string> const expected = {
            // connect
            "h_connack",
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
            [&order, &current, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                BOOST_TEST(current() == "h_connack");
                ++order;
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(1));
                c->async_disconnect(boost::posix_time::seconds(2));
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
        c->connect();
        ios.run();
        BOOST_TEST(current() == "finish");
    };
    do_combi_test(test);
}

BOOST_AUTO_TEST_SUITE_END()

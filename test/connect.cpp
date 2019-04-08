// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_connect)

BOOST_AUTO_TEST_CASE( connect ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(c->connected() == true);
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                BOOST_TEST(c->connected() == true);
                return true;
            });
        c->set_close_handler(
            [&chk, &s, &c]
            () {
                MQTT_CHK("h_close");
                BOOST_TEST(c->connected() == false);
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
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( connect_no_strand ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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

BOOST_AUTO_TEST_CASE( keep_alive ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            cont("h_pingresp"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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
        c->set_pingresp_handler(
            [&chk, &c]
            () {
                MQTT_CHK("h_pingresp");
                c->disconnect();
                return true;
            });
        c->set_keep_alive_sec(3);
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( keep_alive_and_send_control_packet ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            cont("2sec"),
            cont("h_pingresp"),
            cont("4sec_cancelled"),
            // disconnect
            cont("h_close"),
        };

        boost::asio::deadline_timer tim(ios);
        c->set_connack_handler(
            [&chk, &c, &tim]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                tim.expires_from_now(boost::posix_time::seconds(2));
                tim.async_wait(
                    [&chk, &c, &tim](boost::system::error_code const& ec) {
                        MQTT_CHK("2sec");
                        BOOST_CHECK(!ec);
                        c->publish_at_most_once("topic1", "timer_reset");
                        tim.expires_from_now(boost::posix_time::seconds(4));
                        tim.async_wait(
                            [&chk](boost::system::error_code const& ec) {
                                MQTT_CHK("4sec_cancelled");
                                BOOST_TEST(ec == boost::asio::error::operation_aborted );
                            }
                        );
                    }
                );
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
        c->set_pingresp_handler(
            [&chk, &c, &tim]
            () {
                MQTT_CHK("h_pingresp");
                tim.cancel();
                c->disconnect();
                return true;
            });
        c->set_keep_alive_sec_ping_ms(3, 3 * 1000);
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( connect_again ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        bool first = true;

        checker chk = {
            // connect
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),
            // connect
            cont("h_connack2"),
            // disconnect
            cont("h_close2"),
        };

        c->set_connack_handler(
            [&first, &chk, &c]
            (bool sp, std::uint8_t connack_return_code) {
                if (first) {
                    MQTT_CHK("h_connack1");
                }
                else {
                    MQTT_CHK("h_connack2");
                }
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&first, &chk, &c, &s]
            () {
                if (first) {
                    MQTT_CHK("h_close1");
                    first = false;
                    c->connect();
                }
                else {
                    MQTT_CHK("h_close2");
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
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( nocid ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
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

BOOST_AUTO_TEST_CASE( nocid_noclean ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_error"),
        };

        c->set_connack_handler(
            [&chk]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
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
            [&chk, &s]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( noclean ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");

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
            // connect
            cont("h_connack4"),
            // disconnect
            cont("h_close4"),
        };

        int connect = 0;
        c->set_connack_handler(
            [&chk, &connect, &c]
            (bool sp, std::uint8_t connack_return_code) {
                switch (connect) {
                case 0:
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    break;
                case 1:
                    MQTT_CHK("h_connack2");
                    BOOST_TEST(sp == true);
                    break;
                case 2:
                    MQTT_CHK("h_connack3");
                    BOOST_TEST(sp == false);
                    break;
                case 3:
                    MQTT_CHK("h_connack4");
                    BOOST_TEST(sp == false);
                    break;
                }
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->disconnect();
                return true;
            });
        c->set_close_handler(
            [&chk, &connect, &c, &s]
            () {
                switch (connect) {
                case 0:
                    MQTT_CHK("h_close1");
                    c->connect();
                    ++connect;
                    break;
                case 1:
                    MQTT_CHK("h_close2");
                    c->set_clean_session(true);
                    c->connect();
                    ++connect;
                    break;
                case 2:
                    MQTT_CHK("h_close3");
                    c->set_clean_session(false);
                    c->connect();
                    ++connect;
                    break;
                case 3:
                    MQTT_CHK("h_close4");
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
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( disconnect_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_error"),
        };

        c->set_connack_handler(
            [&chk, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
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
            [&chk, &s]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( disconnect_not_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(1));
                c->disconnect(boost::posix_time::seconds(2));
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

BOOST_AUTO_TEST_CASE( async_disconnect_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_error"),
        };

        c->set_connack_handler(
            [&chk, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
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
            [&chk, &s]
            (boost::system::error_code const&) {
                MQTT_CHK("h_error");
                s.close();
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( async_disconnect_not_timeout ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s) {
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        c->set_connack_handler(
            [&chk, &c, &s]
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                s.broker().set_disconnect_delay(boost::posix_time::seconds(1));
                c->async_disconnect(boost::posix_time::seconds(2));
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
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_SUITE_END()

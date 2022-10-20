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

BOOST_AUTO_TEST_SUITE(st_connect1)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( connect ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(c->connected() == true);
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->disconnect();
                    BOOST_TEST(c->connected() == true);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(c->connected() == true);
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                    c->disconnect();
                    BOOST_TEST(c->connected() == true);
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &finish, &c]
            () {
                MQTT_CHK("h_close");
                BOOST_TEST(c->connected() == false);
                finish();
            });
        c->set_error_handler(
            []
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        c->connect();
        BOOST_TEST(c->connected() == false);
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
    do_combi_test(test); // for MQTT_NS::client factory test
}

BOOST_AUTO_TEST_CASE( connect_no_strand ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
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
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
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

BOOST_AUTO_TEST_CASE( keep_alive ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            cont("h_pingresp"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
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
        c->set_pingresp_handler(
            [&chk, &c]
            () {
                MQTT_CHK("h_pingresp");
                c->disconnect();
            });
        c->set_keep_alive_sec(3);
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( keep_alive_and_send_control_packet ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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

        boost::asio::steady_timer tim(ioc);
        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &tim]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    tim.expires_after(std::chrono::seconds(2));
                    tim.async_wait(
                        [&chk, &c, &tim](MQTT_NS::error_code ec) {
                            MQTT_CHK("2sec");
                            BOOST_CHECK(!ec);
                            c->publish("topic1", "timer_reset", MQTT_NS::qos::at_most_once);
                            tim.expires_after(std::chrono::seconds(4));
                            tim.async_wait(
                                [&chk](MQTT_NS::error_code ec) {
                                    MQTT_CHK("4sec_cancelled");
                                    BOOST_TEST(ec == boost::asio::error::operation_aborted);
                                }
                            );
                        }
                    );
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &tim]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    tim.expires_after(std::chrono::seconds(2));
                    tim.async_wait(
                        [&chk, &c, &tim](MQTT_NS::error_code ec) {
                            MQTT_CHK("2sec");
                            BOOST_CHECK(!ec);
                            c->publish("topic1", "timer_reset", MQTT_NS::qos::at_most_once);
                            tim.expires_after(std::chrono::seconds(4));
                            tim.async_wait(
                                [&chk](MQTT_NS::error_code ec) {
                                    MQTT_CHK("4sec_cancelled");
                                    BOOST_TEST(ec == boost::asio::error::operation_aborted);
                                }
                            );
                        }
                    );
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
        c->set_pingresp_handler(
            [&chk, &c, &tim]
            () {
                MQTT_CHK("h_pingresp");
                tim.cancel();
                c->disconnect();
            });
        c->set_keep_alive_sec(3, std::chrono::seconds(3));
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}


BOOST_AUTO_TEST_CASE( pingresp_timeout ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        b.set_pingresp(false);
        c->set_pingresp_timeout(std::chrono::seconds(2));
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // error
            cont("h_error"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&chk, &finish]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                finish();
            });
        c->set_keep_alive_sec(3);
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( connect_again ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&first, &chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    if (first) {
                        MQTT_CHK("h_connack1");
                    }
                    else {
                        MQTT_CHK("h_connack2");
                    }
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&first, &chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    if (first) {
                        MQTT_CHK("h_connack1");
                    }
                    else {
                        MQTT_CHK("h_connack2");
                    }
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    c->disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&first, &chk, &c, &finish]
            () {
                if (first) {
                    MQTT_CHK("h_close1");
                    first = false;
                    c->connect();
                }
                else {
                    MQTT_CHK("h_close2");
                    finish();
                }
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

BOOST_AUTO_TEST_CASE( nocid ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
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
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    std::size_t times = 0;
                    MQTT_NS::v5::visit_props(
                        props,
                        [&](MQTT_NS::v5::property::assigned_client_identifier const& p) {
                            ++times;
                            BOOST_TEST(p.val() == c->get_client_id());
                        },
                        [](auto){}
                    );
                    BOOST_TEST(times == 1);
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

BOOST_AUTO_TEST_CASE( nocid_noclean ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_error"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::identifier_rejected);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            // On v5, a combination of empty client_id and clean_start:false is accepted.
            // Because the client can know the assigned client_id.
            // Even if session_expiry_interval != 0 and store the disconnected session,
            // the client can access the session using assigned client_id
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    std::size_t times = 0;
                    MQTT_NS::v5::visit_props(
                        props,
                        [&](MQTT_NS::v5::property::assigned_client_identifier const& p) {
                            ++times;
                            BOOST_TEST(p.val() == c->get_client_id());
                        },
                        [](auto){}
                    );
                    BOOST_TEST(times == 1);
                    c->force_disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&chk, &finish]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error");
                finish();
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( noclean ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
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
        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &connect, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
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
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->disconnect();
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &connect, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    switch (connect) {
                    case 0:
                        MQTT_CHK("h_connack1");
                        BOOST_TEST(sp == false);
                        break;
                    case 1:
                        MQTT_CHK("h_connack2");
                        // The previous connection is not set Session Expiry Interval.
                        // That means session state is cleared on close.
                        BOOST_TEST(sp == false);
                        break;
                    case 2:
                        MQTT_CHK("h_connack3");
                        BOOST_TEST(sp == false);
                        break;
                    case 3:
                        MQTT_CHK("h_connack4");
                        // The previous connection is not set Session Expiry Interval.
                        // That means session state is cleared on close.
                        BOOST_TEST(sp == false);
                        break;
                    }
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    c->disconnect();
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&chk, &connect, &c, &finish]
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
                    switch (c->get_protocol_version()) {
                    case MQTT_NS::protocol_version::v3_1_1:
                        c->connect();
                        break;
                    case MQTT_NS::protocol_version::v5:
                        c->connect(
                            MQTT_NS::v5::properties{
                                MQTT_NS::v5::property::session_expiry_interval(
                                    MQTT_NS::session_never_expire
                                )
                            }
                        );
                        break;
                    default:
                        BOOST_CHECK(false);
                        break;
                    }
                    ++connect;
                    break;
                case 3:
                    MQTT_CHK("h_close4");
                    finish();
                    break;
                }
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

BOOST_AUTO_TEST_CASE( disconnect_timeout ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    b.set_disconnect_delay(std::chrono::seconds(2));
                    c->disconnect(std::chrono::seconds(1));
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    b.set_disconnect_delay(std::chrono::seconds(2));
                    c->disconnect(std::chrono::seconds(1));
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

BOOST_AUTO_TEST_CASE( disconnect_not_timeout ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    b.set_disconnect_delay(std::chrono::seconds(1));
                    c->disconnect(std::chrono::seconds(2));
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    b.set_disconnect_delay(std::chrono::seconds(1));
                    c->disconnect(std::chrono::seconds(2));
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

BOOST_AUTO_TEST_CASE( async_disconnect_timeout ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    b.set_disconnect_delay(std::chrono::seconds(2));
                    c->async_disconnect(std::chrono::seconds(1));
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    b.set_disconnect_delay(std::chrono::seconds(2));
                    c->async_disconnect(std::chrono::seconds(1));
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

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->async_connect(42); // 42 is dummy session_life_keeper object to increase coverage
            break;
        case MQTT_NS::protocol_version::v5:
            c->async_connect(MQTT_NS::v5::properties(), 42); // 42 is dummy session_life_keeper object to increase coverage
            break;
        default:
            BOOST_CHECK(false);
            break;
        }
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( async_disconnect_not_timeout ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    b.set_disconnect_delay(std::chrono::seconds(1));
                    c->async_disconnect(std::chrono::seconds(2));
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c, &b]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    b.set_disconnect_delay(std::chrono::seconds(1));
                    c->async_disconnect(std::chrono::seconds(2));
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
        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->async_connect(
                []
                (MQTT_NS::error_code ec) {
                    BOOST_TEST( ! ec);
                }
            );
            break;
        case MQTT_NS::protocol_version::v5:
            c->async_connect(
                MQTT_NS::v5::properties(),
                []
                (MQTT_NS::error_code ec) {
                    BOOST_TEST( ! ec);
                }
            );
            break;
        default:
            BOOST_CHECK(false);
            break;
        }
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_CASE( async_keep_alive ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("h_connack"),
            cont("h_pingresp"),
            // disconnect
            cont("h_close"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
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
        c->set_pingresp_handler(
            [&chk, &c]
            () {
                MQTT_CHK("h_pingresp");
                c->async_disconnect();
            });
        c->set_keep_alive_sec(3);
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_SUITE_END()

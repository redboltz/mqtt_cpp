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

BOOST_AUTO_TEST_SUITE(st_connect)

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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    return true;
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
                return true;
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
                    return true;
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
                    return true;
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
                return true;
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
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
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
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    return true;
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
                return true;
            });
        c->set_keep_alive_sec(3);
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( async_keep_alive_and_send_control_packet ) {
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
                            c->async_publish("topic1", "timer_reset", MQTT_NS::qos::at_most_once);
                            tim.expires_after(std::chrono::seconds(4));
                            tim.async_wait(
                                [&chk](MQTT_NS::error_code ec) {
                                    MQTT_CHK("4sec_cancelled");
                                    BOOST_TEST(ec == boost::asio::error::operation_aborted);
                                }
                            );
                        }
                    );
                    return true;
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
                            c->async_publish("topic1", "timer_reset", MQTT_NS::qos::at_most_once);
                            tim.expires_after(std::chrono::seconds(4));
                            tim.async_wait(
                                [&chk](MQTT_NS::error_code ec) {
                                    MQTT_CHK("4sec_cancelled");
                                    BOOST_TEST(ec == boost::asio::error::operation_aborted);
                                }
                            );
                        }
                    );
                    return true;
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
                c->async_disconnect();
                return true;
            });
        c->set_keep_alive_sec(3, std::chrono::seconds(3));
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_CASE( async_pingresp_timeout ) {
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
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    return true;
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
        c->async_connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}


BOOST_AUTO_TEST_CASE( async_connect_session_present_empty_store ) {
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
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                            c->async_disconnect();
                        },
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == true);
                            BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                            c->async_disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    auto ret = MQTT_ORDERED(
                        [&] {
                            MQTT_CHK("h_connack1");
                            BOOST_TEST(sp == false);
                            BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                            c->async_disconnect();
                        },
                        [&] {
                            MQTT_CHK("h_connack2");
                            BOOST_TEST(sp == true);
                            BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                            c->async_disconnect();
                        }
                    );
                    BOOST_TEST(ret);
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&]
            () {
                auto ret = MQTT_ORDERED(
                    [&] {
                        MQTT_CHK("h_close1");
                        async_connect_no_clean(c);
                    },
                    [&] {
                        MQTT_CHK("h_close2");
                        finish();
                    }
                );
                BOOST_TEST(ret);
            });
        c->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });
        async_connect_no_clean(c);
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( async_connect_retry_before_cb ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);

        checker chk = {
            // connect
            cont("async_connect_invalid_host"),
            deps("async_force_disconnect", "async_connect_invalid_host"),
            cont("h_async_force_disconnect"),
            deps("h_async_connect", "async_connect_invalid_host"),
            cont("async_connect_valid_host"),
            cont("h_connack1"),
            // disconnect
            cont("h_close1"),
        };

        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&chk, &c]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack1");
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&]
            () {
                MQTT_CHK("h_close1");
                finish();
            });
        c->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
                BOOST_CHECK(false);
            });

        c->set_host("invalid");
        MQTT_CHK("async_connect_invalid_host");
        c->async_connect(
            [&](MQTT_NS::error_code ec) {
                MQTT_CHK("h_async_connect");
                BOOST_TEST(ec);
                c->set_host(broker_url);
                MQTT_CHK("async_connect_valid_host");
                c->async_connect(
                    [&](MQTT_NS::error_code ec) {
                        BOOST_TEST(!ec);
                    }
                );
            }
        );
        MQTT_CHK("async_force_disconnect");
        c->async_force_disconnect(
            [&](MQTT_NS::error_code ec) {
                MQTT_CHK("h_async_force_disconnect");
                BOOST_TEST(!ec);
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( async_connect_retry_broker_no_connack ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        c->set_client_id("cid1");
        c->set_clean_session(true);
        b.set_connack(false); // set broker no connack send mode for test
        checker chk = {
            cont("async_connect1"),
            cont("async_connect1_timer_set"),
            cont("h_async_connect1"), // underlying connected
            // no CONNACK is sent by broker
            deps("async_connect1_timer_fired", "async_connect1_timer_set"),
            cont("async_force_disconnect"),
            cont("h_async_force_disconnect"),

            // broker recoverd as sending CONNACK
            deps("async_connect2", "async_force_disconnect"),
            cont("async_connect2_timer_set"),
            cont("h_async_connect2"), // underlying connected

            cont("h_connack2"),
            // disconnect
            cont("h_close2"),
            deps("async_connect2_timer_aborted", "h_connack2"),
        };

        boost::asio::steady_timer tim(ioc);
        switch (c->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            c->set_connack_handler(
                [&]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    MQTT_CHK("h_connack2");
                    tim.cancel();
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                    c->async_disconnect();
                    return true;
                });
            break;
        case MQTT_NS::protocol_version::v5:
            c->set_v5_connack_handler(
                [&]
                (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                    MQTT_CHK("h_connack2");
                    tim.cancel();
                    BOOST_TEST(sp == false);
                    BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                    c->async_disconnect();
                    return true;
                });
            break;
        default:
            BOOST_CHECK(false);
            break;
        }

        c->set_close_handler(
            [&]
            () {
                MQTT_CHK("h_close2");
                finish();
            });

        c->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
                b.set_connack(true); // broker recovered for test
                MQTT_CHK("async_connect2");
                c->async_connect(
                    [&](MQTT_NS::error_code ec) {
                        MQTT_CHK("h_async_connect2");
                        BOOST_TEST(!ec);
                    }
                );
                MQTT_CHK("async_connect2_timer_set");
                tim.expires_after(std::chrono::seconds(3));
                tim.async_wait(
                    [&] (boost::system::error_code ec) {
                        BOOST_TEST(ec == boost::asio::error::operation_aborted);
                        MQTT_CHK("async_connect2_timer_aborted");
                    }
                );
            });

        MQTT_CHK("async_connect1");
        c->async_connect(
            [&](MQTT_NS::error_code ec) {
                MQTT_CHK("h_async_connect1");
                BOOST_TEST(!ec);
            }
        );
        tim.expires_after(std::chrono::seconds(3));
        MQTT_CHK("async_connect1_timer_set");
        tim.async_wait(
            [&] (boost::system::error_code ec) {
                BOOST_TEST(!ec);
                MQTT_CHK("async_connect1_timer_fired");
                MQTT_CHK("async_force_disconnect");
                c->async_force_disconnect(
                    [&](MQTT_NS::error_code ec) {
                        MQTT_CHK("h_async_force_disconnect");
                        BOOST_TEST(!ec);
                    }
                );
            }
        );

        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_async(test);
}

BOOST_AUTO_TEST_CASE( connect_prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties con_ps {
            MQTT_NS::v5::property::session_expiry_interval(0x12345678UL),
            MQTT_NS::v5::property::receive_maximum(0x1234U),
            MQTT_NS::v5::property::maximum_packet_size(0x12345678UL),
            MQTT_NS::v5::property::topic_alias_maximum(0x1234U),
            MQTT_NS::v5::property::request_response_information(true),
            MQTT_NS::v5::property::request_problem_information(false),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
            MQTT_NS::v5::property::authentication_method("test authentication method"_mb),
            MQTT_NS::v5::property::authentication_data("test authentication data"_mb)
        };

        std::size_t con_user_prop_count = 0;

        b.set_connect_props_handler(
            [&con_user_prop_count, size = con_ps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(size == props.size());
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::receive_maximum const& t) {
                                BOOST_TEST(t.val() == 0x1234U);
                            },
                            [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                                BOOST_TEST(t.val() == 0x1234U);
                            },
                            [&](MQTT_NS::v5::property::request_response_information const& t) {
                                BOOST_TEST(t.val() == true);
                            },
                            [&](MQTT_NS::v5::property::request_problem_information const& t) {
                                BOOST_TEST(t.val() == false);
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (con_user_prop_count++) {
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
                            [&](MQTT_NS::v5::property::authentication_method const& t) {
                                BOOST_TEST(t.val() == "test authentication method");
                            },
                            [&](MQTT_NS::v5::property::authentication_data const& t) {
                                BOOST_TEST(t.val() == "test authentication data");
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
            [&chk, &c]
            (bool sp, MQTT_NS::v5::connect_reason_code connack_return_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(c->connected() == true);
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::v5::connect_reason_code::success);

                c->disconnect(MQTT_NS::v5::disconnect_reason_code::normal_disconnection, {});
                BOOST_TEST(c->connected() == true);
                return true;
            });

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
        c->connect(std::move(con_ps));
        BOOST_TEST(c->connected() == false);
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( disconnect_prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties discon_ps {
            MQTT_NS::v5::property::session_expiry_interval(0x12345678UL),
            MQTT_NS::v5::property::reason_string("test reason string"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
            MQTT_NS::v5::property::server_reference("test server reference"_mb),
        };

        std::size_t discon_user_prop_count = 0;

        b.set_disconnect_props_handler(
            [&discon_user_prop_count, size = discon_ps.size()] (MQTT_NS::v5::properties const& props) {
                BOOST_TEST(size == props.size());
                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0x12345678UL);
                            },
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test reason string");
                            },
                            [&](MQTT_NS::v5::property::user_property const& t) {
                                switch (discon_user_prop_count++) {
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
                            [&](MQTT_NS::v5::property::server_reference const& t) {
                                BOOST_TEST(t.val() == "test server reference");
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
            [&chk, &c, discon_ps = std::move(discon_ps)]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack");
                BOOST_TEST(c->connected() == true);
                BOOST_TEST(sp == false);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);

                c->disconnect(MQTT_NS::v5::disconnect_reason_code::normal_disconnection, std::move(discon_ps));
                BOOST_TEST(c->connected() == true);
                return true;
            });

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

        c->connect(
            MQTT_NS::v5::properties {
                MQTT_NS::v5::property::session_expiry_interval(1)  // to avoid protocol error
            }
        );
        BOOST_TEST(c->connected() == false);
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_CASE( connack_prop ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& b) {
        auto& c = cs[0];
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }

        c->set_client_id("cid1");
        c->set_clean_session(true);
        BOOST_TEST(c->connected() == false);

        checker chk = {
            // connect
            cont("h_connack"),
            // disconnect
            cont("h_close"),
        };

        MQTT_NS::v5::properties ps {
            MQTT_NS::v5::property::session_expiry_interval(0),
            MQTT_NS::v5::property::receive_maximum(0x1234),
            MQTT_NS::v5::property::maximum_qos(MQTT_NS::qos::at_least_once),
            MQTT_NS::v5::property::retain_available(true),
            MQTT_NS::v5::property::maximum_packet_size(100000),
            MQTT_NS::v5::property::assigned_client_identifier("test cid"_mb),
            MQTT_NS::v5::property::topic_alias_maximum(0),
            MQTT_NS::v5::property::reason_string("test connect success"_mb),
            MQTT_NS::v5::property::user_property("key1"_mb, "val1"_mb),
            MQTT_NS::v5::property::user_property("key2"_mb, "val2"_mb),
            MQTT_NS::v5::property::wildcard_subscription_available(false),
            MQTT_NS::v5::property::subscription_identifier_available(false),
            MQTT_NS::v5::property::shared_subscription_available(false),
            MQTT_NS::v5::property::server_keep_alive(0),
            MQTT_NS::v5::property::response_information("test response information"_mb),
            MQTT_NS::v5::property::server_reference("test server reference"_mb),
            MQTT_NS::v5::property::authentication_method("test authentication method"_mb),
            MQTT_NS::v5::property::authentication_data("test authentication data"_mb)
        };

        auto prop_size = ps.size();
        b.set_connack_props(std::move(ps));

        std::size_t user_prop_count = 0;

        c->set_v5_connack_handler(
            [&chk, &c, &user_prop_count, prop_size]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties props) {
                MQTT_CHK("h_connack");
                BOOST_TEST(c->connected() == true);
                BOOST_TEST(sp == false);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                BOOST_TEST(props.size() == prop_size);

                for (auto const& p : props) {
                    MQTT_NS::visit(
                        MQTT_NS::make_lambda_visitor(
                            [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                                BOOST_TEST(t.val() == 0);
                            },
                            [&](MQTT_NS::v5::property::receive_maximum const& t) {
                                BOOST_TEST(t.val() == 0x1234);
                            },
                            [&](MQTT_NS::v5::property::maximum_qos const& t) {
                                BOOST_TEST(t.val() == 1);
                            },
                            [&](MQTT_NS::v5::property::retain_available const& t) {
                                BOOST_TEST(t.val() == true);
                            },
                            [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                                BOOST_TEST(t.val() == 100000);
                            },
                            [&](MQTT_NS::v5::property::assigned_client_identifier const& t) {
                                BOOST_TEST(t.val() == "test cid");
                            },
                            [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                                BOOST_TEST(t.val() == 0);
                            },
                            [&](MQTT_NS::v5::property::reason_string const& t) {
                                BOOST_TEST(t.val() == "test connect success");
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
                                default:
                                    BOOST_TEST(false);
                                    break;
                                }
                            },
                            [&](MQTT_NS::v5::property::wildcard_subscription_available const& t) {
                                BOOST_TEST(t.val() == false);
                            },
                            [&](MQTT_NS::v5::property::subscription_identifier_available const& t) {
                                BOOST_TEST(t.val() == false);
                            },
                            [&](MQTT_NS::v5::property::shared_subscription_available const& t) {
                                BOOST_TEST(t.val() == false);
                            },
                            [&](MQTT_NS::v5::property::server_keep_alive const& t) {
                                BOOST_TEST(t.val() == 0);
                            },
                            [&](MQTT_NS::v5::property::response_information const& t) {
                                BOOST_TEST(t.val() == "test response information");
                            },
                            [&](MQTT_NS::v5::property::server_reference const& t) {
                                BOOST_TEST(t.val() == "test server reference");
                            },
                            [&](MQTT_NS::v5::property::authentication_method const& t) {
                                BOOST_TEST(t.val() == "test authentication method");
                            },
                            [&](MQTT_NS::v5::property::authentication_data const& t) {
                                BOOST_TEST(t.val() == "test authentication data");
                            },
                            [&](auto&& ...) {
                                BOOST_TEST(false);
                            }
                        ),
                        p
                    );
                }

                c->disconnect();
                BOOST_TEST(c->connected() == true);
                return true;
            });
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
}

BOOST_AUTO_TEST_CASE( session_taken_over ) {
    auto test = [](boost::asio::io_context& ioc, auto& cs, auto finish, auto& /*b*/) {
        auto& c1 = cs[0];
        auto& c2 = cs[1];
        auto& c3 = cs[2];
        clear_ordered();
        if (c1->get_protocol_version() != MQTT_NS::protocol_version::v5) {
            finish();
            return;
        }
        c1->set_client_id("cid1");
        c2->set_client_id("cid1");
        c3->set_client_id("cid1");
        c1->set_clean_start(true);
        c2->set_clean_start(false);
        c3->set_clean_start(true);

        checker chk = {
            // connect
            cont("h_connack1"),
            // connect
            cont("h_disconnect1"),
            cont("h_error1"),
            deps("h_connack2", "h_connack1"),
            // connect
            cont("h_disconnect2"),
            cont("h_error2"),
            deps("h_connack3", "h_connack2"),
            // disconnect
            cont("h_close3"),
        };

        c1->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack1");
                BOOST_TEST(sp == false);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                c2->connect(
                    MQTT_NS::v5::properties{
                        MQTT_NS::v5::property::session_expiry_interval(
                            MQTT_NS::session_never_expire
                        )
                            }
                );
                return true;
            }
        );
        c1->set_v5_disconnect_handler(
            [&](MQTT_NS::v5::disconnect_reason_code disconnect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_disconnect1");
                BOOST_TEST(disconnect_reason_code == MQTT_NS::v5::disconnect_reason_code::session_taken_over);
            }
        );
        c1->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error1");
            }
        );

        c2->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack2");
                BOOST_TEST(sp == true);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                c3->connect();
                return true;
            }
        );
        c2->set_v5_disconnect_handler(
            [&](MQTT_NS::v5::disconnect_reason_code disconnect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_disconnect2");
                BOOST_TEST(disconnect_reason_code == MQTT_NS::v5::disconnect_reason_code::session_taken_over);
            }
        );
        c2->set_error_handler(
            [&]
            (MQTT_NS::error_code) {
                MQTT_CHK("h_error2");
            }
        );

        c3->set_v5_connack_handler(
            [&]
            (bool sp, MQTT_NS::v5::connect_reason_code connect_reason_code, MQTT_NS::v5::properties /*props*/) {
                MQTT_CHK("h_connack3");
                BOOST_TEST(sp == false);
                BOOST_TEST(connect_reason_code == MQTT_NS::v5::connect_reason_code::success);
                c3->disconnect();
                return true;
            }
        );
        c3->set_close_handler(
            [&] {
                MQTT_CHK("h_close3");
                finish();
            }
        );
        c1->connect(
            MQTT_NS::v5::properties{
                MQTT_NS::v5::property::session_expiry_interval(
                    MQTT_NS::session_never_expire
                )
            }
        );
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test, 3);
}

BOOST_AUTO_TEST_SUITE_END()

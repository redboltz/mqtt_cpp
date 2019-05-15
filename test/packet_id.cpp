// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_packet_id)

BOOST_AUTO_TEST_CASE( initial ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
}

BOOST_AUTO_TEST_CASE( increment ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
}

BOOST_AUTO_TEST_CASE( user_register ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    BOOST_TEST(!c->register_packet_id(0));
    BOOST_TEST(c->register_packet_id(1));
    BOOST_TEST(!c->register_packet_id(1));
    BOOST_TEST(c->register_packet_id(2));
}

BOOST_AUTO_TEST_CASE( skip_acquire ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    BOOST_TEST(c->register_packet_id(3));
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
    BOOST_TEST(c->acquire_unique_packet_id() == 4);
    BOOST_TEST(c->acquire_unique_packet_id() == 5);
}

BOOST_AUTO_TEST_CASE( release_but_increment ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
    BOOST_TEST(c->acquire_unique_packet_id() == 3);
    c->release_packet_id(2);
    BOOST_TEST(c->acquire_unique_packet_id() == 4);
    BOOST_TEST(c->acquire_unique_packet_id() == 5);
}

BOOST_AUTO_TEST_CASE( rotate ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    if (sizeof(packet_id_t) == 4) return;
    for (std::uint16_t i = 0; i != 0xffff; ++i) {
        BOOST_TEST(c->acquire_unique_packet_id() == i + 1);
    }
    c->release_packet_id(1);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    c->release_packet_id(5);
    BOOST_TEST(c->acquire_unique_packet_id() == 5);
    c->release_packet_id(2);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
}

BOOST_AUTO_TEST_CASE( exhausted ) {
    boost::asio::io_service ios;
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    if (sizeof(packet_id_t) == 4) return;
    for (std::uint16_t i = 0; i != 0xffff; ++i) {
        c->acquire_unique_packet_id();
    }
    try {
        c->acquire_unique_packet_id();
        BOOST_TEST(false);
    }
    catch (mqtt::packet_id_exhausted_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_SUITE_END()

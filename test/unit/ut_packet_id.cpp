// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <limits>

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(ut_packet_id)

constexpr auto host = "127.0.0.1";
constexpr auto port = 1883;

BOOST_AUTO_TEST_CASE( initial ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
}

BOOST_AUTO_TEST_CASE( increment ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
}

BOOST_AUTO_TEST_CASE( user_register ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    BOOST_TEST(!c->register_packet_id(0));
    BOOST_TEST(c->register_packet_id(1));
    BOOST_TEST(!c->register_packet_id(1));
    BOOST_TEST(c->register_packet_id(2));
}

BOOST_AUTO_TEST_CASE( skip_acquire ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    BOOST_TEST(c->register_packet_id(3));
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
    BOOST_TEST(c->acquire_unique_packet_id() == 4);
    BOOST_TEST(c->acquire_unique_packet_id() == 5);
}

BOOST_AUTO_TEST_CASE( release_but_increment ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
    BOOST_TEST(c->acquire_unique_packet_id() == 3);
    c->release_packet_id(2);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
    BOOST_TEST(c->acquire_unique_packet_id() == 4);
}

BOOST_AUTO_TEST_CASE( rotate ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    std::vector<packet_id_t> result;
    std::vector<packet_id_t> expected;
    for (packet_id_t i = 0; i != std::numeric_limits<packet_id_t>::max(); ++i) {
        result.push_back(c->acquire_unique_packet_id());
        expected.push_back(packet_id_t(i + 1));
    }
    BOOST_TEST(result == expected);
    c->release_packet_id(1);
    BOOST_TEST(c->acquire_unique_packet_id() == 1);
    c->release_packet_id(5);
    BOOST_TEST(c->acquire_unique_packet_id() == 5);
    c->release_packet_id(2);
    BOOST_TEST(c->acquire_unique_packet_id() == 2);
}

BOOST_AUTO_TEST_CASE( exhausted ) {
    boost::asio::io_context ioc;
    auto c = MQTT_NS::make_client(ioc, host, port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
    for (packet_id_t i = 0; i != std::numeric_limits<packet_id_t>::max(); ++i) {
        c->acquire_unique_packet_id();
    }
    try {
        c->acquire_unique_packet_id();
        BOOST_TEST(false);
    }
    catch (MQTT_NS::packet_id_exhausted_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/buffer.hpp>

BOOST_AUTO_TEST_SUITE(ut_buffer)


BOOST_AUTO_TEST_CASE( default_construct ) {
    MQTT_NS::buffer buf;
    BOOST_TEST(buf.empty());
    BOOST_TEST(!buf.has_life());
}

BOOST_AUTO_TEST_CASE( allocate1 ) {
    auto buf = MQTT_NS::allocate_buffer("01234");
    BOOST_TEST(buf == "01234");
    BOOST_TEST(buf.has_life());
    auto ss1 = buf.substr(2, 3);
    BOOST_TEST(ss1 == "234");
    BOOST_TEST(ss1.has_life());
    auto ss2 = ss1.substr(1);
    BOOST_TEST(ss2 == "34");
    BOOST_TEST(ss2.has_life());
}

BOOST_AUTO_TEST_CASE( allocate2 ) {
    std::string s{"01234"};
    auto buf = MQTT_NS::allocate_buffer(s.begin(), s.end());
    BOOST_TEST(buf == "01234");
    BOOST_TEST(buf.has_life());
    auto ss1 = buf.substr(2, 3);
    BOOST_TEST(ss1 == "234");
    BOOST_TEST(ss1.has_life());
    auto ss2 = ss1.substr(1);
    BOOST_TEST(ss2 == "34");
    BOOST_TEST(ss2.has_life());
}

#if 0
// CI doesn't work due to -std=c++1z.
// I couldn't set -std=c++17 on CI
BOOST_AUTO_TEST_CASE( view ) {
    std::string s{"01234"};
    MQTT_NS::buffer buf{ MQTT_NS::string_view{s} };
    BOOST_TEST(buf == "01234");
    BOOST_TEST(!buf.has_life());
    auto ss1 = buf.substr(2, 3);
    BOOST_TEST(ss1 == "234");
    BOOST_TEST(!ss1.has_life());
    auto ss2 = ss1.substr(1);
    BOOST_TEST(ss2 == "34");
    BOOST_TEST(!ss2.has_life());
}
#endif

BOOST_AUTO_TEST_SUITE_END()

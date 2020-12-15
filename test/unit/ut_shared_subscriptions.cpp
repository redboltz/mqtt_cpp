// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/shared_subscriptions.hpp>

BOOST_AUTO_TEST_SUITE(ut_shared_subscriptions)

using namespace MQTT_NS::literals;

// success
BOOST_AUTO_TEST_CASE( parse_success1 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("$share/share_name/topic_filter"_mb);
    BOOST_CHECK(sn_tf_opt);
    BOOST_TEST(sn_tf_opt.value().share_name == "share_name");
    BOOST_TEST(sn_tf_opt.value().topic_filter == "topic_filter");
}

BOOST_AUTO_TEST_CASE( parse_success2 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("topic_filter"_mb);
    BOOST_CHECK(sn_tf_opt);
    BOOST_TEST(sn_tf_opt.value().share_name == "");
    BOOST_TEST(sn_tf_opt.value().topic_filter == "topic_filter");
}

BOOST_AUTO_TEST_CASE( parse_success3 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("$share/share_name//"_mb);
    BOOST_CHECK(sn_tf_opt);
    BOOST_TEST(sn_tf_opt.value().share_name == "share_name");
    BOOST_TEST(sn_tf_opt.value().topic_filter == "/");
}

// error

BOOST_AUTO_TEST_CASE( parse_error1 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("$share//topic_filter"_mb);
    BOOST_CHECK(!sn_tf_opt);
}


BOOST_AUTO_TEST_CASE( parse_error2 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("$share/share_name"_mb);
    BOOST_CHECK(!sn_tf_opt);
}

BOOST_AUTO_TEST_CASE( parse_error3 ) {
    auto sn_tf_opt = MQTT_NS::parse_shared_subscription("$share/share_name/"_mb);
    BOOST_CHECK(!sn_tf_opt);
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer1 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("share_name"_mb, "topic_filter"_mb);
    BOOST_TEST(tfb == "$share/share_name/topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer2 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("share_name", "topic_filter"_mb);
    BOOST_TEST(tfb == "$share/share_name/topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer3 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("share_name"_mb, "topic_filter");
    BOOST_TEST(tfb == "$share/share_name/topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer4 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("share_name", "topic_filter");
    BOOST_TEST(tfb == "$share/share_name/topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer5 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer(""_mb, "topic_filter"_mb);
    BOOST_TEST(tfb == "topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer6 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("", "topic_filter"_mb);
    BOOST_TEST(tfb == "topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer7 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer(""_mb, "topic_filter");
    BOOST_TEST(tfb == "topic_filter");
}

BOOST_AUTO_TEST_CASE( create_topic_filter_buffer8 ) {
    auto tfb = MQTT_NS::create_topic_filter_buffer("", "topic_filter");
    BOOST_TEST(tfb == "topic_filter");
}


BOOST_AUTO_TEST_SUITE_END()

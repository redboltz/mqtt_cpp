// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "test_settings.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_basic_types)

BOOST_AUTO_TEST_CASE( qos ) {
    MQTT_NS::qos q0 = MQTT_NS::qos::at_most_once;
    MQTT_NS::qos q1 = MQTT_NS::qos::at_least_once;
    MQTT_NS::qos q2 = MQTT_NS::qos::exactly_once;
    BOOST_TEST(q0 == q0);
    BOOST_TEST(q0 < q1);
    BOOST_TEST(q1 < q2);
    BOOST_TEST(q0 <= q0);
    BOOST_TEST(q0 <= q1);
    BOOST_TEST(q2 > q1);
    BOOST_TEST(q2 >= q1);
    BOOST_TEST(q2 >= q2);
}

BOOST_AUTO_TEST_SUITE_END()

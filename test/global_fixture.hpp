// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_GLOBAL_FIXTURE_HPP)
#define MQTT_GLOBAL_FIXTURE_HPP

#include <boost/test/included/unit_test.hpp>
#include <mqtt/setup_log.hpp>

struct global_fixture {
    void setup() {
        MQTT_NS::setup_log(MQTT_NS::severity_level::warning);
    }
    void teardown() {
    }
};

BOOST_TEST_GLOBAL_FIXTURE(global_fixture);

#endif // MQTT_GLOBAL_FIXTURE_HPP

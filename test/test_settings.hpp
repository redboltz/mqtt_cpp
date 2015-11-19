// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SETTINGS_HPP)
#define MQTT_TEST_SETTINGS_HPP

#include <boost/test/unit_test.hpp>

constexpr char const* broker_url = "test.mosquitto.org";
constexpr uint16_t const broker_notls_port = 1883;
constexpr uint16_t const broker_tls_port = 8883;

#endif // MQTT_TEST_SETTINGS_HPP

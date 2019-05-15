// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SETTINGS_HPP)
#define MQTT_TEST_SETTINGS_HPP

#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <mqtt/client.hpp>

constexpr char const* broker_url = "localhost";
constexpr uint16_t const broker_notls_port = 1883;
constexpr uint16_t const broker_tls_port = 8883;
constexpr uint16_t const broker_notls_ws_port = 10080;
constexpr uint16_t const broker_tls_ws_port = 10443;

#endif // MQTT_TEST_SETTINGS_HPP

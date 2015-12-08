// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SETTINGS_HPP)
#define MQTT_TEST_SETTINGS_HPP

#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

constexpr char const* broker_url = "test.mosquitto.org";
constexpr uint16_t const broker_notls_port = 1883;
constexpr uint16_t const broker_tls_port = 8883;

class uuid : public boost::uuids::uuid {
public:
    uuid()
        : boost::uuids::uuid(boost::uuids::random_generator()())
    {}

    explicit uuid(boost::uuids::uuid const& u)
        : boost::uuids::uuid(u)
    {}
};

inline std::string const& topic_base() {
    static std::string test_topic = boost::uuids::to_string(uuid());
    return test_topic;
}

#endif // MQTT_TEST_SETTINGS_HPP

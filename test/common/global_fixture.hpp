// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_GLOBAL_FIXTURE_HPP)
#define MQTT_GLOBAL_FIXTURE_HPP

#include <string>
#include <boost/test/unit_test.hpp>

#include <mqtt/setup_log.hpp>
#include <mqtt/string_view.hpp>

struct global_fixture {
    void setup() {
        auto sev =
            [&] {
                auto argc = boost::unit_test::framework::master_test_suite().argc;
                if (argc >= 2) {
                    auto argv = boost::unit_test::framework::master_test_suite().argv;
                    auto sevstr = MQTT_NS::string_view(argv[1]);
                    if (sevstr == "fatal") {
                        return MQTT_NS::severity_level::fatal;
                    }
                    else if (sevstr == "error") {
                        return MQTT_NS::severity_level::error;
                    }
                    else if (sevstr == "warning") {
                        return MQTT_NS::severity_level::warning;
                    }
                    else if (sevstr == "info") {
                        return MQTT_NS::severity_level::info;
                    }
                    else if (sevstr == "debug") {
                        return MQTT_NS::severity_level::debug;
                    }
                    else if (sevstr == "trace") {
                        return MQTT_NS::severity_level::trace;
                    }
                }
                return MQTT_NS::severity_level::warning;
            } ();
        MQTT_NS::setup_log(MQTT_NS::severity_level::trace);
    }
    void teardown() {
    }
};

BOOST_TEST_GLOBAL_FIXTURE(global_fixture);

#endif // MQTT_GLOBAL_FIXTURE_HPP

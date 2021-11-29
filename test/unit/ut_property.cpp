// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <iterator>

#include <boost/lexical_cast.hpp> // for operator<<() test

#include <mqtt/optional.hpp>
#include <mqtt/property.hpp>
#include <mqtt/property_variant.hpp>

BOOST_AUTO_TEST_SUITE(ut_property)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( payload_format_indicator ) {
    MQTT_NS::v5::property::payload_format_indicator v1 { MQTT_NS::v5::property::payload_format_indicator::binary };
    MQTT_NS::v5::property::payload_format_indicator v2 { MQTT_NS::v5::property::payload_format_indicator::string };

    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "binary");
    BOOST_TEST(boost::lexical_cast<std::string>(v2) == "string");
}

BOOST_AUTO_TEST_CASE( message_expiry_interval ) {
    MQTT_NS::v5::property::message_expiry_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( subscription_identifier ) {
    MQTT_NS::v5::property::subscription_identifier v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE(session_expiry_interval  ) {
    MQTT_NS::v5::property::session_expiry_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( server_keep_alive ) {
    MQTT_NS::v5::property::server_keep_alive v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( request_problem_information ) {
    MQTT_NS::v5::property::request_problem_information v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( will_delay_interval ) {
    MQTT_NS::v5::property::will_delay_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( request_response_information ) {
    MQTT_NS::v5::property::request_response_information v { false };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "0");
}

BOOST_AUTO_TEST_CASE( receive_maximum ) {
    MQTT_NS::v5::property::receive_maximum v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( topic_alias_maximum ) {
    MQTT_NS::v5::property::topic_alias_maximum v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( topic_alias ) {
    MQTT_NS::v5::property::topic_alias v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( maximum_qos ) {
    MQTT_NS::v5::property::maximum_qos v { MQTT_NS::qos::at_most_once };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "0");
}

BOOST_AUTO_TEST_CASE( retain_available ) {
    MQTT_NS::v5::property::retain_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( maximum_packet_size ) {
    MQTT_NS::v5::property::maximum_packet_size v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( wildcard_subscription_available ) {
    MQTT_NS::v5::property::wildcard_subscription_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( subscription_identifier_available ) {
    MQTT_NS::v5::property::subscription_identifier_available v { false };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "0");
}

BOOST_AUTO_TEST_CASE( shared_subscription_available ) {
    MQTT_NS::v5::property::shared_subscription_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

// property has _ref

BOOST_AUTO_TEST_CASE( content_type ) {
    MQTT_NS::v5::property::content_type v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( response_topic ) {
    MQTT_NS::v5::property::response_topic v1 { "abc"_mb };

    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( correlation_data ) {
    using namespace std::literals::string_literals;
    MQTT_NS::v5::property::correlation_data v1 { "a\0bc"_mb };
    BOOST_TEST(v1.val() == "a\0bc"s);
}

BOOST_AUTO_TEST_CASE( assigned_client_identifier ) {
    MQTT_NS::v5::property::assigned_client_identifier v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( authentication_method ) {
    MQTT_NS::v5::property::authentication_method v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( authentication_data ) {
    MQTT_NS::v5::property::authentication_data v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( response_information ) {
    MQTT_NS::v5::property::response_information v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( server_reference ) {
    MQTT_NS::v5::property::server_reference v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( reason_string ) {
    MQTT_NS::v5::property::reason_string v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( user_property ) {
    MQTT_NS::v5::property::user_property v1 { "abc"_mb, "def"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc:def");
}

BOOST_AUTO_TEST_CASE(comparison) {
    {
        MQTT_NS::v5::property::payload_format_indicator v1 { MQTT_NS::v5::property::payload_format_indicator::binary };
        MQTT_NS::v5::property::payload_format_indicator v2 { MQTT_NS::v5::property::payload_format_indicator::string };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::message_expiry_interval v1 { 1234 };
        MQTT_NS::v5::property::message_expiry_interval v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::subscription_identifier v1 { 1234 };
        MQTT_NS::v5::property::subscription_identifier v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::session_expiry_interval v1 { 1234 };
        MQTT_NS::v5::property::session_expiry_interval v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::server_keep_alive v1 { 1234 };
        MQTT_NS::v5::property::server_keep_alive v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::request_problem_information v1 { false };
        MQTT_NS::v5::property::request_problem_information v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::will_delay_interval v1 { 1234 };
        MQTT_NS::v5::property::will_delay_interval v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::request_response_information v1 { false };
        MQTT_NS::v5::property::request_response_information v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::receive_maximum v1 { 1234 };
        MQTT_NS::v5::property::receive_maximum v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::topic_alias_maximum v1 { 1234 };
        MQTT_NS::v5::property::topic_alias_maximum v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::topic_alias v1 { 1234 };
        MQTT_NS::v5::property::topic_alias v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::maximum_qos v1 { MQTT_NS::qos::at_most_once };
        MQTT_NS::v5::property::maximum_qos v2 { MQTT_NS::qos::at_least_once };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::retain_available v1 { false };
        MQTT_NS::v5::property::retain_available v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::maximum_packet_size v1 { 1234 };
        MQTT_NS::v5::property::maximum_packet_size v2 { 5678 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::wildcard_subscription_available v1 { false };
        MQTT_NS::v5::property::wildcard_subscription_available v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::subscription_identifier_available v1 { false };
        MQTT_NS::v5::property::subscription_identifier_available v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::shared_subscription_available v1 { false };
        MQTT_NS::v5::property::shared_subscription_available v2 { true };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::content_type v1 { "abc"_mb };
        MQTT_NS::v5::property::content_type v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::response_topic v1 { "abc"_mb };
        MQTT_NS::v5::property::response_topic v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::correlation_data v1 { "ab\0c"_mb };
        MQTT_NS::v5::property::correlation_data v2 { "ab\0f"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::assigned_client_identifier v1 { "abc"_mb };
        MQTT_NS::v5::property::assigned_client_identifier v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::authentication_method v1 { "abc"_mb };
        MQTT_NS::v5::property::authentication_method v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::authentication_data v1 { "abc"_mb };
        MQTT_NS::v5::property::authentication_data v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::response_information v1 { "abc"_mb };
        MQTT_NS::v5::property::response_information v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::server_reference v1 { "abc"_mb };
        MQTT_NS::v5::property::server_reference v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::reason_string v1 { "abc"_mb };
        MQTT_NS::v5::property::reason_string v2 { "def"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
    {
        MQTT_NS::v5::property::user_property v1 { "abc"_mb, "def"_mb };
        MQTT_NS::v5::property::user_property v2 { "abc"_mb, "ghi"_mb };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }

    {
        MQTT_NS::v5::property::user_property v1 { "abc"_mb, "def"_mb };
        MQTT_NS::v5::property::user_property v2 { "abc"_mb, "ghi"_mb };

        MQTT_NS::v5::properties ps1 { v1, v2 };
        MQTT_NS::v5::properties ps2 { v2, v1 };
        BOOST_TEST(v1 == v1);
        BOOST_TEST(v1 != v2);
        BOOST_TEST(v1 < v2);
    }
}

BOOST_AUTO_TEST_SUITE_END()

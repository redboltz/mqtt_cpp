// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

#include <boost/lexical_cast.hpp> // for operator<<() test
#include <iterator>

BOOST_AUTO_TEST_SUITE(test_property)

using namespace mqtt::literals;

BOOST_AUTO_TEST_CASE( payload_format_indicator ) {
    mqtt::v5::property::payload_format_indicator v1 { mqtt::v5::property::payload_format_indicator::binary };
    mqtt::v5::property::payload_format_indicator v2 { mqtt::v5::property::payload_format_indicator::string };

    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "binary");
    BOOST_TEST(boost::lexical_cast<std::string>(v2) == "string");
}

BOOST_AUTO_TEST_CASE( message_expiry_interval ) {
    mqtt::v5::property::message_expiry_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( subscription_identifier ) {
    mqtt::v5::property::subscription_identifier v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE(session_expiry_interval  ) {
    mqtt::v5::property::session_expiry_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( server_keep_alive ) {
    mqtt::v5::property::server_keep_alive v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( request_problem_information ) {
    mqtt::v5::property::request_problem_information v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( will_delay_interval ) {
    mqtt::v5::property::will_delay_interval v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( request_response_information ) {
    mqtt::v5::property::request_response_information v { false };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "0");
}

BOOST_AUTO_TEST_CASE( receive_maximum ) {
    mqtt::v5::property::receive_maximum v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( topic_alias_maximum ) {
    mqtt::v5::property::topic_alias_maximum v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( topic_alias ) {
    mqtt::v5::property::topic_alias v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( maximum_qos ) {
    mqtt::v5::property::maximum_qos v { 2 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "2");
}

BOOST_AUTO_TEST_CASE( retain_available ) {
    mqtt::v5::property::retain_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( maximum_packet_size ) {
    mqtt::v5::property::maximum_packet_size v { 1234 };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1234");
}

BOOST_AUTO_TEST_CASE( wildcard_subscription_available ) {
    mqtt::v5::property::wildcard_subscription_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

BOOST_AUTO_TEST_CASE( subscription_identifier_available ) {
    mqtt::v5::property::subscription_identifier_available v { false };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "0");
}

BOOST_AUTO_TEST_CASE( shared_subscription_available ) {
    mqtt::v5::property::shared_subscription_available v { true };

    BOOST_TEST(boost::lexical_cast<std::string>(v) == "1");
}

// property has _ref

BOOST_AUTO_TEST_CASE( content_type ) {
    mqtt::v5::property::content_type v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( response_topic ) {
    mqtt::v5::property::response_topic v1 { "abc"_mb };

    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( correlation_data ) {
    mqtt::v5::property::correlation_data v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( assigned_client_identifier ) {
    mqtt::v5::property::assigned_client_identifier v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( authentication_method ) {
    mqtt::v5::property::authentication_method v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( authentication_data ) {
    mqtt::v5::property::authentication_data v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( response_information ) {
    mqtt::v5::property::response_information v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( server_reference ) {
    mqtt::v5::property::server_reference v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( reason_string ) {
    mqtt::v5::property::reason_string v1 { "abc"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc");
}

BOOST_AUTO_TEST_CASE( user_property ) {
    mqtt::v5::property::user_property v1 { "abc"_mb, "def"_mb };
    BOOST_TEST(boost::lexical_cast<std::string>(v1) == "abc:def");
}

BOOST_AUTO_TEST_SUITE_END()

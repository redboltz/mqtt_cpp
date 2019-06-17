// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

#include <iterator>

BOOST_AUTO_TEST_SUITE(test_property)

BOOST_AUTO_TEST_CASE( content_type_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::content_type, mqtt::v5::property::content_type_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::content_type_ref, mqtt::v5::property::content_type>::value, "");
    mqtt::v5::property::content_type v1 { "abc" };
    mqtt::v5::property::content_type_ref v_ref1 { "abc" };

    mqtt::v5::property::content_type v2 { v_ref1 };
    mqtt::v5::property::content_type_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( response_topic_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::response_topic, mqtt::v5::property::response_topic_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::response_topic_ref, mqtt::v5::property::response_topic>::value, "");
    mqtt::v5::property::response_topic v1 { "abc" };
    mqtt::v5::property::response_topic_ref v_ref1 { "abc" };

    mqtt::v5::property::response_topic v2 { v_ref1 };
    mqtt::v5::property::response_topic_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( correlation_data_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::correlation_data, mqtt::v5::property::correlation_data_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::correlation_data_ref, mqtt::v5::property::correlation_data>::value, "");
    mqtt::v5::property::correlation_data v1 { "abc" };
    mqtt::v5::property::correlation_data_ref v_ref1 { "abc" };

    mqtt::v5::property::correlation_data v2 { v_ref1 };
    mqtt::v5::property::correlation_data_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( assigned_client_identifier_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::assigned_client_identifier, mqtt::v5::property::assigned_client_identifier_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::assigned_client_identifier_ref, mqtt::v5::property::assigned_client_identifier>::value, "");
    mqtt::v5::property::assigned_client_identifier v1 { "abc" };
    mqtt::v5::property::assigned_client_identifier_ref v_ref1 { "abc" };

    mqtt::v5::property::assigned_client_identifier v2 { v_ref1 };
    mqtt::v5::property::assigned_client_identifier_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( authentication_method_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::authentication_method, mqtt::v5::property::authentication_method_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::authentication_method_ref, mqtt::v5::property::authentication_method>::value, "");
    mqtt::v5::property::authentication_method v1 { "abc" };
    mqtt::v5::property::authentication_method_ref v_ref1 { "abc" };

    mqtt::v5::property::authentication_method v2 { v_ref1 };
    mqtt::v5::property::authentication_method_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( authentication_data_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::authentication_data, mqtt::v5::property::authentication_data_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::authentication_data_ref, mqtt::v5::property::authentication_data>::value, "");
    mqtt::v5::property::authentication_data v1 { "abc" };
    mqtt::v5::property::authentication_data_ref v_ref1 { "abc" };

    mqtt::v5::property::authentication_data v2 { v_ref1 };
    mqtt::v5::property::authentication_data_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( response_information_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::response_information, mqtt::v5::property::response_information_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::response_information_ref, mqtt::v5::property::response_information>::value, "");
    mqtt::v5::property::response_information v1 { "abc" };
    mqtt::v5::property::response_information_ref v_ref1 { "abc" };

    mqtt::v5::property::response_information v2 { v_ref1 };
    mqtt::v5::property::response_information_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( server_reference_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::server_reference, mqtt::v5::property::server_reference_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::server_reference_ref, mqtt::v5::property::server_reference>::value, "");
    mqtt::v5::property::server_reference v1 { "abc" };
    mqtt::v5::property::server_reference_ref v_ref1 { "abc" };

    mqtt::v5::property::server_reference v2 { v_ref1 };
    mqtt::v5::property::server_reference_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( reason_string_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::reason_string, mqtt::v5::property::reason_string_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::reason_string_ref, mqtt::v5::property::reason_string>::value, "");
    mqtt::v5::property::reason_string v1 { "abc" };
    mqtt::v5::property::reason_string_ref v_ref1 { "abc" };

    mqtt::v5::property::reason_string v2 { v_ref1 };
    mqtt::v5::property::reason_string_ref v_ref2 { v1 };

    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_CASE( user_property_ref ) {
    static_assert(std::is_convertible<mqtt::v5::property::user_property, mqtt::v5::property::user_property_ref>::value, "");
    static_assert(std::is_convertible<mqtt::v5::property::user_property_ref, mqtt::v5::property::user_property>::value, "");
    mqtt::v5::property::user_property v1 { "abc", "def" };
    mqtt::v5::property::user_property_ref v_ref1 { "abc", "def" };

    mqtt::v5::property::user_property v2 { v_ref1 };
    mqtt::v5::property::user_property_ref v_ref2 { v1 };

    BOOST_TEST(v2.key() == v_ref2.key());
    BOOST_TEST(v2.val() == v_ref2.val());
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/broker/security.hpp>

#include <sstream>

BOOST_AUTO_TEST_SUITE(ut_broker_security)

void load_config(MQTT_NS::broker::security &security, std::string const& value)
{
    std::stringstream input(value);
    MQTT_NS::broker::security::load(input, security);
}

BOOST_AUTO_TEST_CASE(json_load) {
    MQTT_NS::broker::security security;

    std::string value = "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"mypassword\"},{\"name\":\"u2\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}],\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"]}]}";
    BOOST_CHECK_NO_THROW(load_config(security, value));

    BOOST_CHECK(security.authentication_.size() == 3);

    BOOST_CHECK(security.authentication_["u1"].method_ == MQTT_NS::broker::security::authentication::method::password);
    BOOST_CHECK(security.authentication_["u1"].password);
    BOOST_CHECK(*security.authentication_["u1"].password == "mypassword");

    BOOST_CHECK(security.authentication_["u2"].method_ == MQTT_NS::broker::security::authentication::method::client_cert);
    BOOST_CHECK(!security.authentication_["u2"].password);

    BOOST_CHECK(security.authentication_["anonymous"].method_ == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].password);

    BOOST_CHECK(security.groups_.size() == 1);
    BOOST_CHECK(security.groups_["@g1"].members.size() == 2);

    BOOST_CHECK(security.anonymous);
    BOOST_CHECK(*security.anonymous == "anonymous");
}

BOOST_AUTO_TEST_CASE(check_errors) {
    MQTT_NS::broker::security security;

    BOOST_CHECK(MQTT_NS::broker::security::is_valid_group_name("@test"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_valid_group_name("test"));

    BOOST_CHECK(MQTT_NS::broker::security::is_valid_user_name("test"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_valid_user_name("@test"));

    // Group references non-existing user
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}]}"), std::exception);

    // Auth references non-existing user
    BOOST_CHECK_THROW(load_config(security, "{\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"]}]}"), std::exception);

    // Duplicate user
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"mypassword\"},{\"name\":\"u1\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}]}"), std::exception);

    // Duplicate anonymous
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"u1\",\"method\":\"anonymous\",\"password\":\"mypassword\"},{\"name\":\"u1\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}]}"), std::exception);

    // Duplicate group
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]},{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}]}"), std::exception);

    // Non-existing group
    BOOST_CHECK_THROW(load_config(security, "{\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@nonexist\"]}]}"), std::exception);

    // Invalid username
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"@u1\",\"method\":\"anonymous\"}]}"), std::exception);

    // Invalid group name
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"g1\",\"members\":[\"u1\",\"u2\"]}]}}"), std::exception);

}

BOOST_AUTO_TEST_SUITE_END()

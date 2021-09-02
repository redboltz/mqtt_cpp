// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <string_view>
#include <sstream>
#include <mqtt/connect_return_code.hpp>
#include <mqtt/control_packet_type.hpp>
#include <mqtt/protocol_version.hpp>
#include <mqtt/publish.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/subscribe_options.hpp>

BOOST_AUTO_TEST_SUITE(ut_code)

BOOST_AUTO_TEST_CASE( connect_return_code ) {
    {
        auto c = MQTT_NS::connect_return_code::accepted;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("accepted"));
    }
    {
        auto c = MQTT_NS::connect_return_code::unacceptable_protocol_version;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unacceptable_protocol_version"));
    }
    {
        auto c = MQTT_NS::connect_return_code::identifier_rejected;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("identifier_rejected"));
    }
    {
        auto c = MQTT_NS::connect_return_code::server_unavailable;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_unavailable"));
    }
    {
        auto c = MQTT_NS::connect_return_code::bad_user_name_or_password;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("bad_user_name_or_password"));
    }
    {
        auto c = MQTT_NS::connect_return_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::connect_return_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_connect_return_code"));
    }
}

BOOST_AUTO_TEST_CASE( control_packet_type ) {
    {
        auto c = MQTT_NS::control_packet_type::connect;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("connect"));
    }
    {
        auto c = MQTT_NS::control_packet_type::connack;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("connack"));
    }
    {
        auto c = MQTT_NS::control_packet_type::publish;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("publish"));
    }
    {
        auto c = MQTT_NS::control_packet_type::puback;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("puback"));
    }
    {
        auto c = MQTT_NS::control_packet_type::pubrec;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("pubrec"));
    }
    {
        auto c = MQTT_NS::control_packet_type::pubrel;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("pubrel"));
    }
    {
        auto c = MQTT_NS::control_packet_type::pubcomp;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("pubcomp"));
    }
    {
        auto c = MQTT_NS::control_packet_type::subscribe;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("subscribe"));
    }
    {
        auto c = MQTT_NS::control_packet_type::suback;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("suback"));
    }
    {
        auto c = MQTT_NS::control_packet_type::unsubscribe;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unsubscribe"));
    }
    {
        auto c = MQTT_NS::control_packet_type::unsuback;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unsuback"));
    }
    {
        auto c = MQTT_NS::control_packet_type::pingreq;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("pingreq"));
    }
    {
        auto c = MQTT_NS::control_packet_type::pingresp;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("pingresp"));
    }
    {
        auto c = MQTT_NS::control_packet_type::auth;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("auth"));
    }
    {
        auto c = MQTT_NS::control_packet_type(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_control_packet_type"));
    }
}

BOOST_AUTO_TEST_CASE( protocol_version ) {
    {
        auto c = MQTT_NS::protocol_version::undetermined;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("undetermined"));
    }
    {
        auto c = MQTT_NS::protocol_version::v3_1_1;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("v3_1_1"));
    }
    {
        auto c = MQTT_NS::protocol_version::v5;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("v5"));
    }
    {
        auto c = MQTT_NS::protocol_version(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_protocol_version"));
    }
}

BOOST_AUTO_TEST_CASE( publish ) {
    // retain
    {
        auto c = MQTT_NS::retain::yes;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("yes"));
    }
    {
        auto c = MQTT_NS::retain::no;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no"));
    }
    {
        auto c = MQTT_NS::retain(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_retain"));
    }
    // dup
    {
        auto c = MQTT_NS::dup::yes;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("yes"));
    }
    {
        auto c = MQTT_NS::dup::no;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no"));
    }
    {
        auto c = MQTT_NS::dup(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_dup"));
    }
}

BOOST_AUTO_TEST_CASE( suback_return_code ) {
    {
        auto c = MQTT_NS::suback_return_code::success_maximum_qos_0;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success_maximum_qos_0"));
    }
    {
        auto c = MQTT_NS::suback_return_code::success_maximum_qos_1;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success_maximum_qos_1"));
    }
    {
        auto c = MQTT_NS::suback_return_code::success_maximum_qos_2;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success_maximum_qos_2"));
    }
    {
        auto c = MQTT_NS::suback_return_code::failure;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("failure"));
    }
    {
        auto c = MQTT_NS::suback_return_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_suback_return_code"));
    }
}

BOOST_AUTO_TEST_CASE( connect_reason_code ) {
    {
        auto c = MQTT_NS::v5::connect_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::malformed_packet;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("malformed_packet"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::protocol_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("protocol_error"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::unsupported_protocol_version;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unsupported_protocol_version"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::client_identifier_not_valid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("client_identifier_not_valid"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::bad_user_name_or_password;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("bad_user_name_or_password"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::server_unavailable;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_unavailable"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::server_busy;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_busy"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::banned;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("banned"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::server_shutting_down;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_shutting_down"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::bad_authentication_method;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("bad_authentication_method"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::topic_name_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_name_invalid"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::packet_too_large;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_too_large"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::quota_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("quota_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::payload_format_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("payload_format_invalid"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::retain_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("retain_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::qos_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("qos_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::use_another_server;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("use_another_server"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::server_moved;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_moved"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code::connection_rate_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("connection_rate_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::connect_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_connect_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( disconnect_reason_code ) {
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::normal_disconnection;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("normal_disconnection"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::disconnect_with_will_message;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("disconnect_with_will_message"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::malformed_packet;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("malformed_packet"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::protocol_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("protocol_error"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::server_busy;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_busy"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::server_shutting_down;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_shutting_down"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::keep_alive_timeout;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("keep_alive_timeout"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::session_taken_over;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("session_taken_over"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::topic_filter_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_filter_invalid"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::topic_name_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_name_invalid"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::packet_too_large;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_too_large"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::message_rate_too_high;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("message_rate_too_high"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::quota_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("quota_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::administrative_action;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("administrative_action"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::payload_format_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("payload_format_invalid"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::retain_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("retain_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::qos_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("qos_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::use_another_server;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("use_another_server"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::server_moved;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("server_moved"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::shared_subscriptions_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("shared_subscriptions_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::connection_rate_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("connection_rate_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::maximum_connect_time;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("maximum_connect_time"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::subscription_identifiers_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("subscription_identifiers_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code::wildcard_subscriptions_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("wildcard_subscriptions_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::disconnect_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_disconnect_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( suback_reason_code ) {
    {
        auto c = MQTT_NS::v5::suback_reason_code::granted_qos_0;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("granted_qos_0"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::granted_qos_1;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("granted_qos_1"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::granted_qos_2;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("granted_qos_2"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::topic_filter_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_filter_invalid"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::packet_identifier_in_use;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_in_use"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::quota_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("quota_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::shared_subscriptions_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("shared_subscriptions_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::subscription_identifiers_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("subscription_identifiers_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code::wildcard_subscriptions_not_supported;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("wildcard_subscriptions_not_supported"));
    }
    {
        auto c = MQTT_NS::v5::suback_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_suback_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( unsuback_reason_code ) {
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::no_subscription_existed;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no_subscription_existed"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::topic_filter_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_filter_invalid"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code::packet_identifier_in_use;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_in_use"));
    }
    {
        auto c = MQTT_NS::v5::unsuback_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_unsuback_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( puback_reason_code ) {
    {
        auto c = MQTT_NS::v5::puback_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::no_matching_subscribers;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no_matching_subscribers"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::topic_name_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_name_invalid"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::packet_identifier_in_use;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_in_use"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::quota_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("quota_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code::payload_format_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("payload_format_invalid"));
    }
    {
        auto c = MQTT_NS::v5::puback_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_puback_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( pubrec_reason_code ) {
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::no_matching_subscribers;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no_matching_subscribers"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::unspecified_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unspecified_error"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::implementation_specific_error;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("implementation_specific_error"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::not_authorized;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_authorized"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::topic_name_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("topic_name_invalid"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::packet_identifier_in_use;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_in_use"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::quota_exceeded;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("quota_exceeded"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code::payload_format_invalid;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("payload_format_invalid"));
    }
    {
        auto c = MQTT_NS::v5::pubrec_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_pubrec_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( pubrel_reason_code ) {
    {
        auto c = MQTT_NS::v5::pubrel_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::pubrel_reason_code::packet_identifier_not_found;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_not_found"));
    }
    {
        auto c = MQTT_NS::v5::pubrel_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_pubrel_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( pubcomp_reason_code ) {
    {
        auto c = MQTT_NS::v5::pubcomp_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::pubcomp_reason_code::packet_identifier_not_found;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("packet_identifier_not_found"));
    }
    {
        auto c = MQTT_NS::v5::pubcomp_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_pubcomp_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( auth_reason_code ) {
    {
        auto c = MQTT_NS::v5::auth_reason_code::success;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("success"));
    }
    {
        auto c = MQTT_NS::v5::auth_reason_code::continue_authentication;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("continue_authentication"));
    }
    {
        auto c = MQTT_NS::v5::auth_reason_code::re_authenticate;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("re_authenticate"));
    }
    {
        auto c = MQTT_NS::v5::auth_reason_code(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("unknown_auth_reason_code"));
    }
}

BOOST_AUTO_TEST_CASE( subscribe_options ) {
    // retain_handling
    {
        auto c = MQTT_NS::retain_handling::send;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("send"));
    }
    {
        auto c = MQTT_NS::retain_handling::send_only_new_subscription;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("send_only_new_subscription"));
    }
    {
        auto c = MQTT_NS::retain_handling::not_send;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("not_send"));
    }
    {
        auto c = MQTT_NS::retain_handling(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_retain_handling"));
    }
    // rap
    {
        auto c = MQTT_NS::rap::dont;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("dont"));
    }
    {
        auto c = MQTT_NS::rap::retain;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("retain"));
    }
    {
        auto c = MQTT_NS::rap(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_rap"));
    }
    // nl
    {
        auto c = MQTT_NS::nl::no;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("no"));
    }
    {
        auto c = MQTT_NS::nl::yes;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("yes"));
    }
    {
        auto c = MQTT_NS::nl(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_nl"));
    }
    // qps
    {
        auto c = MQTT_NS::qos::at_most_once;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("at_most_once"));
    }
    {
        auto c = MQTT_NS::qos::at_least_once;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("at_least_once"));
    }
    {
        auto c = MQTT_NS::qos::exactly_once;
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("exactly_once"));
    }
    {
        auto c = MQTT_NS::qos(99);
        std::stringstream ss;
        ss << c;
        BOOST_TEST(ss.str() == MQTT_NS::string_view("invalid_qos"));
    }
}

BOOST_AUTO_TEST_SUITE_END()

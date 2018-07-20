// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_ID_HPP)
#define MQTT_PROPERTY_ID_HPP

#include <cstdint>

namespace mqtt {

namespace v5 {

namespace property {

namespace id {

constexpr std::uint8_t const payload_format_indicator          =  1;
constexpr std::uint8_t const message_expiry_interval           =  2;
constexpr std::uint8_t const content_type                      =  3;
constexpr std::uint8_t const response_topic                    =  8;
constexpr std::uint8_t const correlation_data                  =  9;
constexpr std::uint8_t const subscription_identifier           = 11;
constexpr std::uint8_t const session_expiry_interval           = 17;
constexpr std::uint8_t const assigned_client_identifier        = 18;
constexpr std::uint8_t const server_keep_alive                 = 19;
constexpr std::uint8_t const authentication_method             = 21;
constexpr std::uint8_t const authentication_data               = 22;
constexpr std::uint8_t const request_problem_information       = 23;
constexpr std::uint8_t const will_delay_interval               = 24;
constexpr std::uint8_t const request_response_information      = 25;
constexpr std::uint8_t const response_information              = 26;
constexpr std::uint8_t const server_reference                  = 28;
constexpr std::uint8_t const reason_string                     = 31;
constexpr std::uint8_t const receive_maximum                   = 33;
constexpr std::uint8_t const topic_alias_maximum               = 34;
constexpr std::uint8_t const topic_alias                       = 35;
constexpr std::uint8_t const maximum_qos                       = 36;
constexpr std::uint8_t const retain_available                  = 37;
constexpr std::uint8_t const user_property                     = 38;
constexpr std::uint8_t const maximum_packet_size               = 39;
constexpr std::uint8_t const wildcard_subscription_available   = 40;
constexpr std::uint8_t const subscription_identifier_available = 41;
constexpr std::uint8_t const shared_subscription_available     = 42;

} // namespace id

} // namespace property

} // namespace v5

} // namespace mqtt

#endif // MQTT_PROPERTY_ID_HPP

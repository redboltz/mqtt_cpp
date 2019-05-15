// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_REASON_CODE_HPP)
#define MQTT_REASON_CODE_HPP

#include <cstdint>

namespace mqtt {
namespace v5 {
namespace reason_code {

constexpr std::uint8_t const success                                       = 0x00;
constexpr std::uint8_t const normal_disconnection                          = 0x00;
constexpr std::uint8_t const granted_qos_0                                 = 0x00;
constexpr std::uint8_t const granted_qos_1                                 = 0x01;
constexpr std::uint8_t const granted_qos_2                                 = 0x02;
constexpr std::uint8_t const disconnect_with_will_message                  = 0x04;
constexpr std::uint8_t const no_matching_subscribers                       = 0x10;
constexpr std::uint8_t const no_subscription_existed                       = 0x11;
constexpr std::uint8_t const continue_authentication                       = 0x18;
constexpr std::uint8_t const re_authenticate                               = 0x19;
constexpr std::uint8_t const unspecified_error                             = 0x80;
constexpr std::uint8_t const malformed_packet                              = 0x81;
constexpr std::uint8_t const protocol_error                                = 0x82;
constexpr std::uint8_t const implementation_specific_error                 = 0x83;
constexpr std::uint8_t const unsupported_protocol_version                  = 0x84;
constexpr std::uint8_t const client_identifier_not_valid                   = 0x85;
constexpr std::uint8_t const bad_user_name_or_password                     = 0x86;
constexpr std::uint8_t const not_authorized                                = 0x87;
constexpr std::uint8_t const server_unavailable                            = 0x88;
constexpr std::uint8_t const server_busy                                   = 0x89;
constexpr std::uint8_t const banned                                        = 0x8a;
constexpr std::uint8_t const server_shutting_down                          = 0x8b;
constexpr std::uint8_t const bad_authentication_method                     = 0x8c;
constexpr std::uint8_t const keep_alive_timeout                            = 0x8d;
constexpr std::uint8_t const session_taken_over                            = 0x8e;
constexpr std::uint8_t const topic_filter_invalid                          = 0x8f;
constexpr std::uint8_t const topic_name_invalid                            = 0x90;
constexpr std::uint8_t const packet_identifier_in_use                      = 0x91;
constexpr std::uint8_t const packet_identifier_not_found                   = 0x92;
constexpr std::uint8_t const receive_maximum_exceeded                      = 0x93;
constexpr std::uint8_t const topic_alias_invalid                           = 0x94;
constexpr std::uint8_t const packet_too_large                              = 0x95;
constexpr std::uint8_t const message_rate_too_high                         = 0x96;
constexpr std::uint8_t const quota_exceeded                                = 0x97;
constexpr std::uint8_t const administrative_action                         = 0x98;
constexpr std::uint8_t const payload_format_invalid                        = 0x99;
constexpr std::uint8_t const retain_not_supported                          = 0x9a;
constexpr std::uint8_t const qos_not_supported                             = 0x9b;
constexpr std::uint8_t const use_another_server                            = 0x9c;
constexpr std::uint8_t const server_moved                                  = 0x9d;
constexpr std::uint8_t const shared_subscriptions_not_supported            = 0x9e;
constexpr std::uint8_t const connection_rate_exceeded                      = 0x9f;
constexpr std::uint8_t const maximum_connect_time                          = 0xa0;
constexpr std::uint8_t const subscription_identifiers_not_supported        = 0xa1;
constexpr std::uint8_t const wildcard_subscriptions_not_supported          = 0xa2;

} // namespace reason_code
} // v5
} // namespace mqtt

#endif // MQTT_CONNECT_RETURN_CODE_HPP

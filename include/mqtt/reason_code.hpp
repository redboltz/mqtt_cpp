// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_REASON_CODE_HPP)
#define MQTT_REASON_CODE_HPP

#include <cstdint>
#include <ostream>

#include <mqtt/namespace.hpp>
#include <mqtt/subscribe_options.hpp>

namespace MQTT_NS {

enum class suback_return_code : std::uint8_t {
    success_maximum_qos_0                  = 0x00,
    success_maximum_qos_1                  = 0x01,
    success_maximum_qos_2                  = 0x02,
    failure                                = 0x80,
};

constexpr
char const* suback_return_code_to_str(suback_return_code v) {
    switch(v)
    {
    case suback_return_code::success_maximum_qos_0: return "success_maximum_qos_0";
    case suback_return_code::success_maximum_qos_1: return "success_maximum_qos_1";
    case suback_return_code::success_maximum_qos_2: return "success_maximum_qos_2";
    case suback_return_code::failure:               return "failure";
    default:                                        return "unknown_suback_return_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, suback_return_code val)
{
    os << suback_return_code_to_str(val);
    return os;
}

constexpr suback_return_code qos_to_suback_return_code(qos q) {
    return static_cast<suback_return_code>(q);
}

namespace v5 {

enum class connect_reason_code : std::uint8_t {
    success                       = 0x00,
    unspecified_error             = 0x80,
    malformed_packet              = 0x81,
    protocol_error                = 0x82,
    implementation_specific_error = 0x83,
    unsupported_protocol_version  = 0x84,
    client_identifier_not_valid   = 0x85,
    bad_user_name_or_password     = 0x86,
    not_authorized                = 0x87,
    server_unavailable            = 0x88,
    server_busy                   = 0x89,
    banned                        = 0x8a,
    server_shutting_down          = 0x8b,
    bad_authentication_method     = 0x8c,
    topic_name_invalid            = 0x90,
    packet_too_large              = 0x95,
    quota_exceeded                = 0x97,
    payload_format_invalid        = 0x99,
    retain_not_supported          = 0x9a,
    qos_not_supported             = 0x9b,
    use_another_server            = 0x9c,
    server_moved                  = 0x9d,
    connection_rate_exceeded      = 0x9f,
};

constexpr
char const* connect_reason_code_to_str(connect_reason_code v) {
    switch(v)
    {
        case connect_reason_code::success:                       return "success";
        case connect_reason_code::unspecified_error:             return "unspecified_error";
        case connect_reason_code::malformed_packet:              return "malformed_packet";
        case connect_reason_code::protocol_error:                return "protocol_error";
        case connect_reason_code::implementation_specific_error: return "implementation_specific_error";
        case connect_reason_code::unsupported_protocol_version:  return "unsupported_protocol_version";
        case connect_reason_code::client_identifier_not_valid:   return "client_identifier_not_valid";
        case connect_reason_code::bad_user_name_or_password:     return "bad_user_name_or_password";
        case connect_reason_code::not_authorized:                return "not_authorized";
        case connect_reason_code::server_unavailable:            return "server_unavailable";
        case connect_reason_code::server_busy:                   return "server_busy";
        case connect_reason_code::banned:                        return "banned";
        case connect_reason_code::server_shutting_down:          return "server_shutting_down";
        case connect_reason_code::bad_authentication_method:     return "bad_authentication_method";
        case connect_reason_code::topic_name_invalid:            return "topic_name_invalid";
        case connect_reason_code::packet_too_large:              return "packet_too_large";
        case connect_reason_code::quota_exceeded:                return "quota_exceeded";
        case connect_reason_code::payload_format_invalid:        return "payload_format_invalid";
        case connect_reason_code::retain_not_supported:          return "retain_not_supported";
        case connect_reason_code::qos_not_supported:             return "qos_not_supported";
        case connect_reason_code::use_another_server:            return "use_another_server";
        case connect_reason_code::server_moved:                  return "server_moved";
        case connect_reason_code::connection_rate_exceeded:      return "connection_rate_exceeded";
        default:                                                 return "unknown_connect_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, connect_reason_code val)
{
    os << connect_reason_code_to_str(val);
    return os;
}

enum class disconnect_reason_code : std::uint8_t {
    normal_disconnection                   = 0x00,
    disconnect_with_will_message           = 0x04,
    unspecified_error                      = 0x80,
    malformed_packet                       = 0x81,
    protocol_error                         = 0x82,
    implementation_specific_error          = 0x83,
    not_authorized                         = 0x87,
    server_busy                            = 0x89,
    server_shutting_down                   = 0x8b,
    keep_alive_timeout                     = 0x8d,
    session_taken_over                     = 0x8e,
    topic_filter_invalid                   = 0x8f,
    topic_name_invalid                     = 0x90,
    receive_maximum_exceeded               = 0x93,
    topic_alias_invalid                    = 0x94,
    packet_too_large                       = 0x95,
    message_rate_too_high                  = 0x96,
    quota_exceeded                         = 0x97,
    administrative_action                  = 0x98,
    payload_format_invalid                 = 0x99,
    retain_not_supported                   = 0x9a,
    qos_not_supported                      = 0x9b,
    use_another_server                     = 0x9c,
    server_moved                           = 0x9d,
    shared_subscriptions_not_supported     = 0x9e,
    connection_rate_exceeded               = 0x9f,
    maximum_connect_time                   = 0xa0,
    subscription_identifiers_not_supported = 0xa1,
    wildcard_subscriptions_not_supported   = 0xa2,
};

constexpr
char const* disconnect_reason_code_to_str(disconnect_reason_code v) {
    switch(v)
    {
        case disconnect_reason_code::normal_disconnection:                   return "normal_disconnection";
        case disconnect_reason_code::disconnect_with_will_message:           return "disconnect_with_will_message";
        case disconnect_reason_code::unspecified_error:                      return "unspecified_error";
        case disconnect_reason_code::malformed_packet:                       return "malformed_packet";
        case disconnect_reason_code::protocol_error:                         return "protocol_error";
        case disconnect_reason_code::implementation_specific_error:          return "implementation_specific_error";
        case disconnect_reason_code::not_authorized:                         return "not_authorized";
        case disconnect_reason_code::server_busy:                            return "server_busy";
        case disconnect_reason_code::server_shutting_down:                   return "server_shutting_down";
        case disconnect_reason_code::keep_alive_timeout:                     return "keep_alive_timeout";
        case disconnect_reason_code::session_taken_over:                     return "session_taken_over";
        case disconnect_reason_code::topic_filter_invalid:                   return "topic_filter_invalid";
        case disconnect_reason_code::topic_name_invalid:                     return "topic_name_invalid";
        case disconnect_reason_code::receive_maximum_exceeded:               return "receive_maximum_exceeded";
        case disconnect_reason_code::topic_alias_invalid:                    return "topic_alias_invalid";
        case disconnect_reason_code::packet_too_large:                       return "packet_too_large";
        case disconnect_reason_code::message_rate_too_high:                  return "message_rate_too_high";
        case disconnect_reason_code::quota_exceeded:                         return "quota_exceeded";
        case disconnect_reason_code::administrative_action:                  return "administrative_action";
        case disconnect_reason_code::payload_format_invalid:                 return "payload_format_invalid";
        case disconnect_reason_code::retain_not_supported:                   return "retain_not_supported";
        case disconnect_reason_code::qos_not_supported:                      return "qos_not_supported";
        case disconnect_reason_code::use_another_server:                     return "use_another_server";
        case disconnect_reason_code::server_moved:                           return "server_moved";
        case disconnect_reason_code::shared_subscriptions_not_supported:     return "shared_subscriptions_not_supported";
        case disconnect_reason_code::connection_rate_exceeded:               return "connection_rate_exceeded";
        case disconnect_reason_code::maximum_connect_time:                   return "maximum_connect_time";
        case disconnect_reason_code::subscription_identifiers_not_supported: return "subscription_identifiers_not_supported";
        case disconnect_reason_code::wildcard_subscriptions_not_supported:   return "wildcard_subscriptions_not_supported";
        default:                                                             return "unknown_disconnect_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, disconnect_reason_code val)
{
    os << disconnect_reason_code_to_str(val);
    return os;
}


enum class suback_reason_code : std::uint8_t {
    granted_qos_0                          = 0x00,
    granted_qos_1                          = 0x01,
    granted_qos_2                          = 0x02,
    unspecified_error                      = 0x80,
    implementation_specific_error          = 0x83,
    not_authorized                         = 0x87,
    topic_filter_invalid                   = 0x8f,
    packet_identifier_in_use               = 0x91,
    quota_exceeded                         = 0x97,
    shared_subscriptions_not_supported     = 0x9e,
    subscription_identifiers_not_supported = 0xa1,
    wildcard_subscriptions_not_supported   = 0xa2,
};

constexpr
char const* suback_reason_code_to_str(suback_reason_code v) {
    switch(v)
    {
    case suback_reason_code::granted_qos_0:                          return "granted_qos_0";
    case suback_reason_code::granted_qos_1:                          return "granted_qos_1";
    case suback_reason_code::granted_qos_2:                          return "granted_qos_2";
    case suback_reason_code::unspecified_error:                      return "unspecified_error";
    case suback_reason_code::implementation_specific_error:          return "implementation_specific_error";
    case suback_reason_code::not_authorized:                         return "not_authorized";
    case suback_reason_code::topic_filter_invalid:                   return "topic_filter_invalid";
    case suback_reason_code::packet_identifier_in_use:               return "packet_identifier_in_use";
    case suback_reason_code::quota_exceeded:                         return "quota_exceeded";
    case suback_reason_code::shared_subscriptions_not_supported:     return "shared_subscriptions_not_supported";
    case suback_reason_code::subscription_identifiers_not_supported: return "subscription_identifiers_not_supported";
    case suback_reason_code::wildcard_subscriptions_not_supported:   return "wildcard_subscriptions_not_supported";
    default:                                                         return "unknown_suback_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, suback_reason_code val)
{
    os << suback_reason_code_to_str(val);
    return os;
}

constexpr suback_reason_code qos_to_suback_reason_code(qos q) {
    return static_cast<suback_reason_code>(q);
}

enum class unsuback_reason_code : std::uint8_t {
    success                       = 0x00,
    no_subscription_existed       = 0x11,
    unspecified_error             = 0x80,
    implementation_specific_error = 0x83,
    not_authorized                = 0x87,
    topic_filter_invalid          = 0x8f,
    packet_identifier_in_use      = 0x91,
};

constexpr
char const* unsuback_reason_code_to_str(unsuback_reason_code v) {
    switch(v)
    {
    case unsuback_reason_code::success:                       return "success";
    case unsuback_reason_code::no_subscription_existed:       return "no_subscription_existed";
    case unsuback_reason_code::unspecified_error:             return "unspecified_error";
    case unsuback_reason_code::implementation_specific_error: return "implementation_specific_error";
    case unsuback_reason_code::not_authorized:                return "not_authorized";
    case unsuback_reason_code::topic_filter_invalid:          return "topic_filter_invalid";
    case unsuback_reason_code::packet_identifier_in_use:      return "packet_identifier_in_use";
    default:                                                  return "unknown_unsuback_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, unsuback_reason_code val)
{
    os << unsuback_reason_code_to_str(val);
    return os;
}

enum class puback_reason_code : std::uint8_t {
    success                       = 0x00,
    no_matching_subscribers       = 0x10,
    unspecified_error             = 0x80,
    implementation_specific_error = 0x83,
    not_authorized                = 0x87,
    topic_name_invalid            = 0x90,
    packet_identifier_in_use      = 0x91,
    quota_exceeded                = 0x97,
    payload_format_invalid        = 0x99,
};

constexpr
char const* puback_reason_code_to_str(puback_reason_code v) {
    switch(v)
    {
    case puback_reason_code::success:                       return "success";
    case puback_reason_code::no_matching_subscribers:       return "no_matching_subscribers";
    case puback_reason_code::unspecified_error:             return "unspecified_error";
    case puback_reason_code::implementation_specific_error: return "implementation_specific_error";
    case puback_reason_code::not_authorized:                return "not_authorized";
    case puback_reason_code::topic_name_invalid:            return "topic_name_invalid";
    case puback_reason_code::packet_identifier_in_use:      return "packet_identifier_in_use";
    case puback_reason_code::quota_exceeded:                return "quota_exceeded";
    case puback_reason_code::payload_format_invalid:        return "payload_format_invalid";
    default:                                                return "unknown_puback_reason_code";
    }
}

constexpr
bool is_error(puback_reason_code v) {
    return static_cast<std::uint8_t>(v) >= 0x80;
}

inline
std::ostream& operator<<(std::ostream& os, puback_reason_code val)
{
    os << puback_reason_code_to_str(val);
    return os;
}

enum class pubrec_reason_code : std::uint8_t {
    success                       = 0x00,
    no_matching_subscribers       = 0x10,
    unspecified_error             = 0x80,
    implementation_specific_error = 0x83,
    not_authorized                = 0x87,
    topic_name_invalid            = 0x90,
    packet_identifier_in_use      = 0x91,
    quota_exceeded                = 0x97,
    payload_format_invalid        = 0x99,
};

constexpr
char const* pubrec_reason_code_to_str(pubrec_reason_code v) {
    switch(v)
    {
    case pubrec_reason_code::success:                       return "success";
    case pubrec_reason_code::no_matching_subscribers:       return "no_matching_subscribers";
    case pubrec_reason_code::unspecified_error:             return "unspecified_error";
    case pubrec_reason_code::implementation_specific_error: return "implementation_specific_error";
    case pubrec_reason_code::not_authorized:                return "not_authorized";
    case pubrec_reason_code::topic_name_invalid:            return "topic_name_invalid";
    case pubrec_reason_code::packet_identifier_in_use:      return "packet_identifier_in_use";
    case pubrec_reason_code::quota_exceeded:                return "quota_exceeded";
    case pubrec_reason_code::payload_format_invalid:        return "payload_format_invalid";
    default:                                                return "unknown_pubrec_reason_code";
    }
}

constexpr
bool is_error(pubrec_reason_code v) {
    return static_cast<std::uint8_t>(v) >= 0x80;
}

inline
std::ostream& operator<<(std::ostream& os, pubrec_reason_code val)
{
    os << pubrec_reason_code_to_str(val);
    return os;
}

enum class pubrel_reason_code : std::uint8_t {
    success                     = 0x00,
    packet_identifier_not_found = 0x92,
};

constexpr
char const* pubrel_reason_code_to_str(pubrel_reason_code v) {
    switch(v)
    {
    case pubrel_reason_code::success:                      return "success";
    case pubrel_reason_code::packet_identifier_not_found:  return "packet_identifier_not_found";
    default:                                               return "unknown_pubrel_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, pubrel_reason_code val)
{
    os << pubrel_reason_code_to_str(val);
    return os;
}

enum class pubcomp_reason_code : std::uint8_t {
    success                     = 0x00,
    packet_identifier_not_found = 0x92,
};

constexpr
char const* pubcomp_reason_code_to_str(pubcomp_reason_code v) {
    switch(v)
    {
    case pubcomp_reason_code::success:                      return "success";
    case pubcomp_reason_code::packet_identifier_not_found:  return "packet_identifier_not_found";
    default:                                                return "unknown_pubcomp_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, pubcomp_reason_code val)
{
    os << pubcomp_reason_code_to_str(val);
    return os;
}

enum class auth_reason_code : std::uint8_t {
    success                 = 0x00,
    continue_authentication = 0x18,
    re_authenticate         = 0x19,
};

constexpr
char const* auth_reason_code_to_str(auth_reason_code v) {
    switch(v)
    {
    case auth_reason_code::success:                 return "success";
    case auth_reason_code::continue_authentication: return "continue_authentication";
    case auth_reason_code::re_authenticate:         return "re_authenticate";
    default:                                        return "unknown_auth_reason_code";
    }
}

inline
std::ostream& operator<<(std::ostream& os, auth_reason_code val)
{
    os << auth_reason_code_to_str(val);
    return os;
}

} // v5
} // namespace MQTT_NS

#endif // MQTT_REASON_CODE_HPP

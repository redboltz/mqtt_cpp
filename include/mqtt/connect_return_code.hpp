// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CONNECT_RETURN_CODE_HPP)
#define MQTT_CONNECT_RETURN_CODE_HPP

#include <cstdint>
#include <ostream>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

enum class connect_return_code : std::uint8_t {

    accepted                      = 0,
    unacceptable_protocol_version = 1,
    identifier_rejected           = 2,
    server_unavailable            = 3,
    bad_user_name_or_password     = 4,
    not_authorized                = 5,

};

constexpr
char const* connect_return_code_to_str(connect_return_code v) {
    char const * const str[] = {
        "accepted",
        "unacceptable_protocol_version",
        "identifier_rejected",
        "server_unavailable",
        "bad_user_name_or_password",
        "not_authorized"
    };
    if (static_cast<std::uint8_t>(v) < sizeof(str)) return str[static_cast<std::uint8_t>(v)];
    return "unknown_connect_return_code";
}

inline
std::ostream& operator<<(std::ostream& os, connect_return_code val)
{
    os << connect_return_code_to_str(val);
    return os;
}

} // namespace MQTT_NS

#endif // MQTT_CONNECT_RETURN_CODE_HPP

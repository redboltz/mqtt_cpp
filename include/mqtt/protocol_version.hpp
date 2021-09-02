// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROTOCOL_VERSION_HPP)
#define MQTT_PROTOCOL_VERSION_HPP

#include <cstdint>
#include <ostream>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

enum class protocol_version {
    undetermined  = 0,
    v3_1_1        = 4,
    v5            = 5,
};

constexpr char const* protocol_version_to_str(protocol_version v) {
    switch(v) {
    case protocol_version::undetermined: return "undetermined";
    case protocol_version::v3_1_1: return "v3_1_1";
    case protocol_version::v5: return "v5";
    default: return "unknown_protocol_version";
    }
}

inline
std::ostream& operator<<(std::ostream& os, protocol_version val)
{
    os << protocol_version_to_str(val);
    return os;
}

} // namespace MQTT_NS

#endif // MQTT_PROTOCOL_VERSION_HPP

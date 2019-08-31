// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_QOS_HPP)
#define MQTT_QOS_HPP

#include <cstdint>
#include <mqtt/namespace.hpp>

namespace MQTT_NS {

enum class qos : std::uint8_t {

    at_most_once  = 0,
    at_least_once = 1,
    exactly_once  = 2,

}; // enum qos

inline
char const* qos_to_str(qos v) {
    char const * const str[] = {
        "at_most_once",
        "at_least_once",
        "exactly_once"
    };
    if (static_cast<size_t>(v) < sizeof(str)) return str[static_cast<size_t>(v)];
    return "invalid_qos";
}

template<typename OSTREAM_T>
OSTREAM_T & operator<<(OSTREAM_T & os, qos v)
{
    return os << qos_to_str(v);
}

} // namespace MQTT_NS

#endif // MQTT_QOS_HPP

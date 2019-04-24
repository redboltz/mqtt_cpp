// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROTOCOL_VERSION_HPP)
#define MQTT_PROTOCOL_VERSION_HPP

#include <cstdint>

namespace mqtt {

enum class protocol_version {
    undetermined  = 0,
    v3_1_1        = 4,
    v5            = 5,
};

} // namespace mqtt

#endif // MQTT_PROTOCOL_VERSION_HPP

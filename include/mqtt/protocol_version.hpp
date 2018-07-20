// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROTOCOL_VERSION_HPP)
#define MQTT_PROTOCOL_VERSION_HPP

#include <cstdint>

namespace mqtt {

namespace protocol_version {

constexpr std::uint8_t const undetermined  = 0;
constexpr std::uint8_t const v3_1_1        = 4;
constexpr std::uint8_t const v5            = 5;

} // namespace protocol_version

} // namespace mqtt

#endif // MQTT_PROTOCOL_VERSION_HPP

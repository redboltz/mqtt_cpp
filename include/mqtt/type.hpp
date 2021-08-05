// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TYPE_HPP)
#define MQTT_TYPE_HPP

#include <cstdint>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

using session_expiry_interval_t = std::uint32_t;
using topic_alias_t = std::uint16_t;
using receive_maximum_t = std::uint16_t;

} // namespace MQTT_NS

#endif // MQTT_TYPE_HPP

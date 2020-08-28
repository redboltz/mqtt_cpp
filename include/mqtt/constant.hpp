// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CONSTANT_HPP)
#define MQTT_CONSTANT_HPP

#include <mqtt/type.hpp>

namespace MQTT_NS {

static constexpr session_expiry_interval_t const session_never_expire = 0xffffffffUL;

} // namespace MQTT_NS

#endif // MQTT_CONSTANT_HPP

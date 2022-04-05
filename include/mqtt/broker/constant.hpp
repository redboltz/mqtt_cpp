// Copyright Wouter van Kleunen 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_CONSTANT_HPP)
#define MQTT_BROKER_CONSTANT_HPP


#include <mqtt/namespace.hpp>
#include <mqtt/type.hpp>

namespace MQTT_NS {

namespace broker {

static constexpr std::size_t max_cn_size = 0xffff;

} // namespace broker

} // namespace MQTT_NS

#endif // MQTT_BROKER_CONSTANT_HPP

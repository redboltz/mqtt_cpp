// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_RETAIN_HANDLING_HPP)
#define MQTT_RETAIN_HANDLING_HPP

#include <cstdint>

namespace mqtt {

namespace retain_handling {

constexpr std::uint8_t const send  = 0;
constexpr std::uint8_t const send_only_new_subscription = 1;
constexpr std::uint8_t const not_send  = 2;

} // namespace retain_handling

} // namespace mqtt

#endif // MQTT_RETAIN_HANDLING_HPP

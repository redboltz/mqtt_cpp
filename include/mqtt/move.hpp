// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MOVE_HPP)
#define MQTT_MOVE_HPP

#include <utility>
#include <type_traits>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

template <typename T>
constexpr
typename std::remove_reference_t<T>&&
force_move(T&& t) {
    static_assert(!std::is_const<std::remove_reference_t<T>>::value, "T is const. Fallback to copy.");
    return std::move(t);
}

} // namespace MQTT_NS

#endif // MQTT_MOVE_HPP

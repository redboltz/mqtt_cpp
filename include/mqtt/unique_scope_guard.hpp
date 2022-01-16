// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_UNIQUE_SCOPE_GUARD_HPP)
#define MQTT_UNIQUE_SCOPE_GUARD_HPP

#include <memory>
#include <utility>
#include <mqtt/namespace.hpp>

namespace MQTT_NS {

template <typename Proc>
inline auto unique_scope_guard(Proc&& proc) {
    auto deleter = [proc = std::forward<Proc>(proc)](void*) mutable { std::forward<Proc>(proc)(); };
    return std::unique_ptr<void, decltype(deleter)>(&deleter, force_move(deleter));
}

} // namespace MQTT_NS

#endif // MQTT_UNIQUE_SCOPE_GUARD_HPP

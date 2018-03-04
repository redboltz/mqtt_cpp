// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_UNIQUE_SCOPE_GUARD_HPP)
#define MQTT_UNIQUE_SCOPE_GUARD_HPP

#include <memory>
#include <utility>

#include <mqtt/utility.hpp>

namespace mqtt {

template <typename Proc>
inline auto unique_scope_guard(Proc&& proc) {
    auto deleter = [MQTT_CAPTURE_FORWARD(Proc, proc)](void*) mutable { std::forward<Proc>(proc)(); };
    return std::unique_ptr<void, decltype(deleter)>(&deleter, std::forward<decltype(deleter)>(deleter));
}

} // namespace mqtt

#endif // MQTT_UNIQUE_SCOPE_GUARD_HPP

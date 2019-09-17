// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ATTRIBUTES_HPP)
#define MQTT_ATTRIBUTES_HPP

#include <mqtt/namespace.hpp>

#ifdef _MSC_VER

#define MQTT_ALWAYS_INLINE

#else // GCC or Clang

#define MQTT_ALWAYS_INLINE [[gnu::always_inline]]

#endif // _MSC_VER

#endif // MQTT_ATTRIBUTES_HPP

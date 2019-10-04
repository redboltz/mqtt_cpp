// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_DEPRECATED_HPP)
#define MQTT_DEPRECATED_HPP

#if defined(MQTT_USE_DEPRECATED)

#define MQTT_DEPRECATED(msg) // for test, ignore it

#else  // defined(MQTT_USE_DEPRECATED)

#if __cplusplus >= 201402L

#if defined(_MSC_VER)

#define MQTT_DEPRECATED(msg) __declspec(deprecated(msg))

#else  // _MSC_VER 1914+ with /Zc:__cplusplus, @see https://docs.microsoft.com/cpp/build/reference/zc-cplusplus

#define MQTT_DEPRECATED(msg) [[deprecated(msg)]]

#endif

#else  // __cplusplus >= 201402L

#define MQTT_DEPRECATED(msg)

#endif // __cplusplus >= 201402L

#endif // defined(MQTT_USE_DEPRECATED)

#endif // MQTT_DEPRECATED_HPP

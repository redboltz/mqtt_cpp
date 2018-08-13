// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_UTILITY_HPP)
#define MQTT_UTILITY_HPP

#include <utility>

#if __cplusplus >= 201402L
#define MQTT_CAPTURE_FORWARD(T, v) v = std::forward<T>(v)
#define MQTT_CAPTURE_MOVE(v) v = std::move(v)
#else
#define MQTT_CAPTURE_FORWARD(T, v) v
#define MQTT_CAPTURE_MOVE(v) v
#endif

#if __cplusplus >= 201402L
#define MQTT_DEPRECATED(msg) [[deprecated(msg)]]
#else  // __cplusplus >= 201402L
#define MQTT_DEPRECATED(msg)
#endif // __cplusplus >= 201402L


// string_view

#if __cplusplus >= 201703L

#include <string_view>

namespace mqtt {
using string_view = std::string_view;
} // namespace mqtt

#else  // __cplusplus >= 201703L

#include <boost/version.hpp>
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 61

#include <boost/utility/string_view.hpp>
namespace mqtt {
using string_view = boost::string_view;
} // namespace mqtt

#else // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 61

#include <boost/utility/string_ref.hpp>
namespace mqtt {
using string_view = boost::string_ref;
} // namespace mqtt

#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 61

#endif // __cplusplus >= 201703L

#endif // MQTT_UTILITY_HPP

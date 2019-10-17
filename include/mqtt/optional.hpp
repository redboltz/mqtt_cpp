// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_OPTIONAL_HPP)
#define MQTT_OPTIONAL_HPP

#if defined(MQTT_STD_OPTIONAL)

#include <optional>

#else  // defined(MQTT_STD_OPTIONAL)

#include <boost/optional.hpp>

#endif // defined(MQTT_STD_VARIANT)

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

#if defined(MQTT_STD_OPTIONAL)

using std::optional;
using std::nullopt_t;
using in_place_t = std::in_place_t;
static constexpr auto in_place_init = in_place_t{};
static constexpr auto nullopt = std::nullopt;

#else  // defined(MQTT_STD_OPTIONAL)

using boost::optional;
using nullopt_t = boost::none_t;
using boost::in_place_init;
static const auto nullopt = boost::none;

#endif // defined(MQTT_STD_OPTIONAL)

} // namespace MQTT_NS

#endif // MQTT_OPTIONAL_HPP

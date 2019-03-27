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

namespace mqtt {

#if defined(MQTT_STD_OPTIONAL)

using std::optional;
using std::nullopt_t;
static constexpr auto nullopt = std::nullopt;

#else  // defined(MQTT_STD_OPTIONAL)

using boost::optional;
using nullopt_t = boost::none_t;
static const auto nullopt = boost::none;

#endif // defined(MQTT_STD_OPTIONAL)

} // namespace mqtt

#endif // MQTT_OPTIONAL_HPP

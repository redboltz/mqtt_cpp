// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_NULL_STRAND_HPP)
#define MQTT_NULL_STRAND_HPP

#include <utility>

#include <boost/asio.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

// Using standard executor style null_strand / simple executor
using null_strand = as::io_context::executor_type;

} // namespace MQTT_NS

#endif // MQTT_NULL_STRAND_HPP

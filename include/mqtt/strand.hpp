// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_STRAND_HPP)
#define MQTT_STRAND_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

// Determines which strand to use
#if defined(MQTT_NO_TS_EXECUTORS)

// Use standard executor style strand
using strand = as::strand<as::io_context::executor_type>;

#else // defined(MQTT_NO_TS_EXECUTORS)

// Use networking TS style strand
using strand = as::io_context::strand;

#endif // defined(MQTT_NO_TS_EXECUTORS)
}

#endif // MQTT_STRAND_HPP

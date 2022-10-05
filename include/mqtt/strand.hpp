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

using strand = as::strand<as::io_context::executor_type>;

}

#endif // MQTT_STRAND_HPP

// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ASYNC_HANDLER_HPP)
#define MQTT_ASYNC_HANDLER_HPP

#include <boost/asio.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/move_only_handler.hpp>
#include <mqtt/error_code.hpp>

namespace MQTT_NS {

using async_handler_t = move_only_function<void(error_code ec)>;

} // namespace MQTT_NS

#endif // MQTT_ASYNC_HANDLER_HPP

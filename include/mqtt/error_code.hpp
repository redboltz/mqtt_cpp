// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ERROR_CODE_HPP)
#define MQTT_ERROR_CODE_HPP

#include <mqtt/namespace.hpp>

#include <boost/system/error_code.hpp>

namespace MQTT_NS {

using error_code = boost::system::error_code;

} // namespace MQTT_NS

#endif // MQTT_ERROR_CODE_HPP

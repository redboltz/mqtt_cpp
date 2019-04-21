// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ANY_HPP)
#define MQTT_ANY_HPP

namespace mqtt {

#ifdef MQTT_STD_ANY

#include <any>

using std::any;

#else  // MQTT_STD_ANY

#include <boost/any.hpp>

using boost::any;

#endif // !defined(MQTT_STD_ANY)

} // namespace mqtt

#endif // MQTT_ANY_HPP

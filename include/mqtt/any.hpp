// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ANY_HPP)
#define MQTT_ANY_HPP

#ifdef MQTT_STD_ANY

#include <any>

namespace mqtt {

using std::any;
using std::any_cast;

} // namespace mqtt

#else  // MQTT_STD_ANY

#include <boost/any.hpp>

namespace mqtt {

using boost::any;
using boost::any_cast;

} // namespace mqtt

#endif // !defined(MQTT_STD_ANY)

#endif // MQTT_ANY_HPP

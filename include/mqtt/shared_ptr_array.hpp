// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SHARED_PTR_ARRAY_HPP)
#define MQTT_SHARED_PTR_ARRAY_HPP

#ifdef MQTT_STD_SHARED_PTR_ARRAY

#include <memory>

namespace mqtt {

using shared_ptr_array = std::shared_ptr<char []>;
using shared_ptr_const_array = std::shared_ptr<char const []>;

} // namespace mqtt

#else  // MQTT_STD_SHARED_PTR_ARRAY

#include <boost/shared_ptr.hpp>

namespace mqtt {

using shared_ptr_array = boost::shared_ptr<char []>;
using shared_ptr_const_array = boost::shared_ptr<char const []>;

} // namespace mqtt

#endif // MQTT_STD_SHARED_PTR_ARRAY

#endif // MQTT_SHARED_PTR_ARRAY_HPP

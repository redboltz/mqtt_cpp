// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CONST_BUFFER_UTIL_HPP)
#define MQTT_CONST_BUFFER_UTIL_HPP

#include <boost/asio/buffer.hpp>
#include <mqtt/namespace.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

inline char const* get_pointer(as::const_buffer const& cb) {
    return static_cast<char const*>(cb.data());
}

inline std::size_t get_size(as::const_buffer const& cb) {
    return cb.size();
}

} // namespace MQTT_NS

#endif // MQTT_CONST_BUFFER_UTIL_HPP

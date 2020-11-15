// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TWO_BYTE_UTIL_HPP)
#define MQTT_TWO_BYTE_UTIL_HPP

#include <string>
#include <cstdint>

#include <boost/assert.hpp>
#include <boost/container/static_vector.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

inline boost::container::static_vector<char, 2> num_to_2bytes(std::uint16_t val) {
    return {
        static_cast<char>(val >> 8),
        static_cast<char>(val & 0xff)
    };
}

template <typename T>
inline void add_uint16_t_to_buf(T& buf, std::uint16_t num) {
    buf.push_back(static_cast<char>(num >> 8));
    buf.push_back(static_cast<char>(num & 0xff));
}

template <typename It>
constexpr std::uint16_t make_uint16_t(It b, It e) {
    (void)e; // Avoid warning in release builds about unused variable
    BOOST_ASSERT(std::distance(b, e) == 2);
    auto b1 = b++;
    auto b2 = b++;
    return
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(*b1) & 0xff) << 8 |
            (static_cast<std::uint16_t>(*b2) & 0xff)
        );
}

} // namespace MQTT_NS

#endif // MQTT_TWO_BYTE_UTIL_HPP

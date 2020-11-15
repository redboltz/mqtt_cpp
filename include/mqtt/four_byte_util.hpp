// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_FOUR_BYTE_UTIL_HPP)
#define MQTT_FOUR_BYTE_UTIL_HPP

#include <string>
#include <cstdint>
#include <algorithm>

#include <boost/assert.hpp>
#include <boost/container/static_vector.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

inline boost::container::static_vector<char, 4> num_to_4bytes(std::uint32_t val) {
    return {
        static_cast<char>(val >> 24),
        static_cast<char>(val >> 16),
        static_cast<char>(val >>  8),
        static_cast<char>(val & 0xff)
    };
}

template <typename T>
inline void add_uint32_t_to_buf(T& buf, std::uint32_t num) {
    buf.push_back(static_cast<char>(num >> 24));
    buf.push_back(static_cast<char>(num >> 16));
    buf.push_back(static_cast<char>(num >>  8));
    buf.push_back(static_cast<char>(num & 0xff));
}

template <typename It>
constexpr std::uint32_t make_uint32_t(It b, It e) {
    (void)e; // Avoid warning in release builds about unused variable
    BOOST_ASSERT(std::distance(b, e) == 4);
    auto b1 = b++;
    auto b2 = b++;
    auto b3 = b++;
    auto b4 = b++;
    return
        static_cast<std::uint32_t>(
            (static_cast<std::uint16_t>(*b1) & 0xff) << 24 |
            (static_cast<std::uint16_t>(*b2) & 0xff) << 16 |
            (static_cast<std::uint16_t>(*b3) & 0xff) <<  8 |
            (static_cast<std::uint16_t>(*b4) & 0xff)
        );
}

} // namespace MQTT_NS

#endif // MQTT_FOUR_BYTE_UTIL_HPP

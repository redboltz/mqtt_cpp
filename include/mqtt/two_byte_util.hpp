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

#define MQTT_16BITNUM_TO_BYTE_SEQ(val)                                  \
    static_cast<char>(static_cast<std::uint16_t>(val) >> 8),            \
    static_cast<char>((val) & 0xff)

namespace mqtt {

template <typename T>
inline void add_uint16_t_to_buf(T& buf, std::uint16_t num) {
    buf.push_back(static_cast<char>(num >> 8));
    buf.push_back(static_cast<char>(num & 0xff));
}

template <typename It>
std::uint16_t make_uint16_t(It b, It e) {
    BOOST_ASSERT(std::distance(b, e) == 2);
    auto b2 = b;
    ++b2;
    return
        (static_cast<std::uint16_t>(*b) & 0xff) << 8 |
        (static_cast<std::uint16_t>(*b2) & 0xff);
}

} // namespace mqtt

#endif // MQTT_TWO_BYTE_UTIL_HPP

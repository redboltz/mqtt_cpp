// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_REMAINING_LENGTH_HPP)
#define MQTT_REMAINING_LENGTH_HPP

#include <string>
#include <mqtt/exception.hpp>

namespace mqtt {

inline std::string
remaining_bytes(std::size_t size) {
    if (size > 0xfffffff) throw remaining_length_error();
    std::string bytes;
    while (size > 127) {
        bytes.push_back(size & 0b01111111);
        size >>= 7;
    }
    bytes.push_back(size & 0b01111111);
    return bytes;
}

} // namespace mqtt

#endif // MQTT_REMAINING_LENGTH_HPP

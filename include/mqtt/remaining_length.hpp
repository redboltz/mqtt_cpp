// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_REMAINING_LENGTH_HPP)
#define MQTT_REMAINING_LENGTH_HPP

#include <mqtt/variable_length.hpp>

namespace mqtt {

inline std::string
remaining_bytes(std::size_t size) {
    auto bytes = variable_bytes(size);
    if (bytes.empty()) throw remaining_length_error();
    return bytes;
}

inline std::tuple<std::size_t, std::size_t>
remaining_length(std::string const& bytes) {
    return variable_length(bytes);
}

template <typename Iterator>
inline std::tuple<std::size_t, std::size_t>
remaining_length(Iterator b, Iterator e) {
    return variable_length(b, e);
}

} // namespace mqtt

#endif // MQTT_REMAINING_LENGTH_HPP

// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_REMAINING_LENGTH_HPP)
#define MQTT_REMAINING_LENGTH_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/variable_length.hpp>

namespace MQTT_NS {

inline std::string
remaining_bytes(std::size_t size) {
    std::string bytes = variable_bytes(size);
    if (bytes.empty() || bytes.size() > 4) throw remaining_length_error();
    return bytes;
}

constexpr std::tuple<std::size_t, std::size_t>
remaining_length(string_view bytes) {
    return variable_length(bytes);
}

template <typename Iterator>
constexpr std::tuple<std::size_t, std::size_t>
remaining_length(Iterator b, Iterator e) {
    return variable_length(b, e);
}

} // namespace MQTT_NS

#endif // MQTT_REMAINING_LENGTH_HPP

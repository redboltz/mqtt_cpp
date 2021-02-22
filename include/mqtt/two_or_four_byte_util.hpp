// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TWO_OR_FOUR_BYTE_UTIL_HPP)
#define MQTT_TWO_OR_FOUR_BYTE_UTIL_HPP

#include <cstdint>
#include <cstdlib>

#include <mqtt/namespace.hpp>
#include <mqtt/two_byte_util.hpp>
#include <mqtt/four_byte_util.hpp>

namespace MQTT_NS {

template <std::size_t Bytes>
struct two_or_four_byte_type;

template <>
struct two_or_four_byte_type<2> {
    using type = std::uint16_t;
};

template <>
struct two_or_four_byte_type<4> {
    using type = std::uint32_t;
};

template <std::size_t Bytes>
struct make_two_or_four_byte;

template <>
struct make_two_or_four_byte<2> {
    template <typename It>
    static constexpr std::uint16_t apply(It b, It e) {
        return make_uint16_t(b, e);
    }
};

template <>
struct make_two_or_four_byte<4> {
    template <typename It>
    static constexpr std::uint32_t apply(It b, It e) {
        return make_uint32_t(b, e);
    }
};

template <std::size_t Bytes>
struct add_two_or_four_byte_to_buf;

template <>
struct add_two_or_four_byte_to_buf<2> {
    template <typename T>
    static void apply(T& buf, std::uint16_t two_or_four_byte) {
        add_uint16_t_to_buf(buf, two_or_four_byte);
    }
};

template <>
struct add_two_or_four_byte_to_buf<4> {
    template <typename T>
    static void apply(T& buf, std::uint32_t two_or_four_byte) {
        add_uint32_t_to_buf(buf, two_or_four_byte);
    }
};

} // namespace MQTT_NS

#endif // MQTT_TWO_OR_FOUR_BYTE_UTIL_HPP

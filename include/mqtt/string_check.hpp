// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_STRING_CHECK_HPP)
#define MQTT_STRING_CHECK_HPP

#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/exception.hpp>

namespace mqtt {

inline void utf8string_check(string_view str) {
    if (!utf8string::is_valid_length(str)) throw utf8string_length_error();
    auto r = utf8string::validate_contents(str);
    if (r != utf8string::validation::well_formed) {
        throw utf8string_contents_error(r);
    }
}

} // namespace mqtt

#endif // MQTT_STRING_CHECK_HPP

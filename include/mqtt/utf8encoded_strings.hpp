// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_UTF8ENCODED_STRINGS_HPP)
#define MQTT_UTF8ENCODED_STRINGS_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>

namespace MQTT_NS {

namespace utf8string {

enum struct validation
{
    /**
     * @brief UTF-8 string is well_formed.
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718016
     * 1.5.3 UTF-8 encoded strings
     */
    well_formed = 0,

    /**
     * @brief UTF-8 string is ill_formed or contains null character.
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718016
     * 1.5.3 UTF-8 encoded strings
     */
    ill_formed,

    /**
     * @brief UTF-8 string is well_formed and contains control character and non-character.
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718016
     * 1.5.3 UTF-8 encoded strings
     */
    well_formed_with_non_charactor,
};

constexpr bool
is_valid_length(string_view str) {
    return str.size() <= 0xffff;
}

constexpr validation
validate_contents(string_view str) {
    // This code is based on https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
    auto result = validation::well_formed;
#if defined(MQTT_USE_STR_CHECK)
    auto it = str.begin();
    auto end = str.end();

    while (it != end) {
        if (static_cast<unsigned char>(*(it + 0)) < 0b1000'0000) {
            // 0xxxxxxxxx
            if (static_cast<unsigned char>(*(it + 0)) == 0x00) {
                result = validation::ill_formed;
                break;
            }
            if ((static_cast<unsigned char>(*(it + 0)) >= 0x01 &&
                 static_cast<unsigned char>(*(it + 0)) <= 0x1f) ||
                static_cast<unsigned char>(*(it + 0)) == 0x7f) {
                result = validation::well_formed_with_non_charactor;
            }
            ++it;
        }
        else if ((static_cast<unsigned char>(*(it + 0)) & 0b1110'0000) == 0b1100'0000) {
            // 110XXXXx 10xxxxxx
            if (it + 1 >= end) {
                result = validation::ill_formed;
                break;
            }
            if ((static_cast<unsigned char>(*(it + 1)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 0)) & 0b1111'1110) == 0b1100'0000) { // overlong
                result = validation::ill_formed;
                break;
            }
            if (static_cast<unsigned char>(*(it + 0)) == 0b1100'0010 &&
                static_cast<unsigned char>(*(it + 1)) >= 0b1000'0000 &&
                static_cast<unsigned char>(*(it + 1)) <= 0b1001'1111) {
                result = validation::well_formed_with_non_charactor;
            }
            it += 2;
        }
        else if ((static_cast<unsigned char>(*(it + 0)) & 0b1111'0000) == 0b1110'0000) {
            // 1110XXXX 10Xxxxxx 10xxxxxx
            if (it + 2 >= end) {
                result = validation::ill_formed;
                break;
            }
            if ((static_cast<unsigned char>(*(it + 1)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 2)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 0)) == 0b1110'0000 &&
                 (static_cast<unsigned char>(*(it + 1)) & 0b1110'0000) == 0b1000'0000) || // overlong?
                (static_cast<unsigned char>(*(it + 0)) == 0b1110'1101 &&
                 (static_cast<unsigned char>(*(it + 1)) & 0b1110'0000) == 0b1010'0000)) { // surrogate?
                result = validation::ill_formed;
                break;
            }
            if (static_cast<unsigned char>(*(it + 0)) == 0b1110'1111 &&
                static_cast<unsigned char>(*(it + 1)) == 0b1011'1111 &&
                (static_cast<unsigned char>(*(it + 2)) & 0b1111'1110) == 0b1011'1110) {
                // U+FFFE or U+FFFF?
                result = validation::well_formed_with_non_charactor;
            }
            it += 3;
        }
        else if ((static_cast<unsigned char>(*(it + 0)) & 0b1111'1000) == 0b1111'0000) {
            // 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx
            if (it + 3 >= end) {
                result = validation::ill_formed;
                break;
            }
            if ((static_cast<unsigned char>(*(it + 1)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 2)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 3)) & 0b1100'0000) != 0b1000'0000 ||
                (static_cast<unsigned char>(*(it + 0)) == 0b1111'0000 &&
                 (static_cast<unsigned char>(*(it + 1)) & 0b1111'0000) == 0b1000'0000) ||    // overlong?
                (static_cast<unsigned char>(*(it + 0)) == 0b1111'0100 &&
                 static_cast<unsigned char>(*(it + 1)) > 0b1000'1111) ||
                static_cast<unsigned char>(*(it + 0)) > 0b1111'0100) { // > U+10FFFF?
                result = validation::ill_formed;
                break;
            }
            if ((static_cast<unsigned char>(*(it + 1)) & 0b1100'1111) == 0b1000'1111 &&
                static_cast<unsigned char>(*(it + 2)) == 0b1011'1111 &&
                (static_cast<unsigned char>(*(it + 3)) & 0b1111'1110) == 0b1011'1110) {
                // U+nFFFE or U+nFFFF?
                result = validation::well_formed_with_non_charactor;
            }
            it += 4;
        }
        else {
            result = validation::ill_formed;
            break;
        }
    }
#else // MQTT_USE_STR_CHECK
    static_cast<void>(str);
#endif // MQTT_USE_STR_CHECK
    return result;
}

} // namespace utf8string

} // namespace MQTT_NS

#endif // MQTT_UTF8ENCODED_STRINGS_HPP

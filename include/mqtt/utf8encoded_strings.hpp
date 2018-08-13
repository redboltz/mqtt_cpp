// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_UTF8ENCODED_STRINGS_HPP)
#define MQTT_UTF8ENCODED_STRINGS_HPP

#include <string>

namespace mqtt {

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

inline bool
is_valid_length(std::string const& str) {
    return str.size() <= 0xffff;
}

inline validation
validate_contents(std::string const& str) {
    // This code is based on https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
    auto result = validation::well_formed;
#if defined(MQTT_USE_STR_CHECK)
    auto s = reinterpret_cast<unsigned char const*>(str.c_str());
    auto end = s + str.size();

    while (s != end) {
        if (s[0] < 0b1000'0000) {
            // 0xxxxxxxxx
            if (s[0] == 0x00) {
                result = validation::ill_formed;
                break;
            }
            if ((s[0] >= 0x01 && s[0] <= 0x1f) || s[0] == 0x7f) {
                result = validation::well_formed_with_non_charactor;
            }
            ++s;
        }
        else if ((s[0] & 0b1110'0000) == 0b1100'0000) {
            // 110XXXXx 10xxxxxx
            if ((s[1] & 0b1100'0000) != 0b1000'0000 ||
                (s[0] & 0b1111'1110) == 0b1100'0000) { // overlong
                result = validation::ill_formed;
                break;
            }
            if (s[0] == 0b1100'0010 &&
                s[1] >= 0b1000'0000 && s[1] <= 0b1001'1111) {
                result = validation::well_formed_with_non_charactor;
            }
            s += 2;
        }
        else if ((s[0] & 0b1111'0000) == 0b1110'0000) {
            // 1110XXXX 10Xxxxxx 10xxxxxx
            if ((s[1] & 0b1100'0000) != 0b1000'0000 ||
                (s[2] & 0b1100'0000) != 0b1000'0000 ||
                (s[0] == 0b1110'0000 && (s[1] & 0b1110'0000) == 0b1000'0000) || // overlong?
                (s[0] == 0b1110'1101 && (s[1] & 0b1110'0000) == 0b1010'0000)) { // surrogate?
                result = validation::ill_formed;
                break;
            }
            if (s[0] == 0b1110'1111 &&
                s[1] == 0b1011'1111 &&
                (s[2] & 0b1111'1110) == 0b1011'1110) {
                // U+FFFE or U+FFFF?
                result = validation::well_formed_with_non_charactor;
            }
            s += 3;
        }
        else if ((s[0] & 0b1111'1000) == 0b1111'0000) {
            // 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx
            if ((s[1] & 0b1100'0000) != 0b1000'0000 ||
                (s[2] & 0b1100'0000) != 0b1000'0000 ||
                (s[3] & 0b1100'0000) != 0b1000'0000 ||
                (s[0] == 0b1111'0000 && (s[1] & 0b1111'0000) == 0b1000'0000) ||    // overlong?
                (s[0] == 0b1111'0100 && s[1] > 0b1000'1111) || s[0] > 0b1111'0100) { // > U+10FFFF?
                result = validation::ill_formed;
                break;
            }
            if ((s[1] & 0b1100'1111) == 0b1000'1111 &&
                s[2] == 0b1011'1111 &&
                (s[3] & 0b1111'1110) == 0b1011'1110) {
                // U+nFFFE or U+nFFFF?
                result = validation::well_formed_with_non_charactor;
            }
            s += 4;
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

} // namespace mqtt

#endif // MQTT_UTF8ENCODED_STRINGS_HPP

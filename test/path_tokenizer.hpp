// Copyright wkl04 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PATH_TOKENIZER_HPP)
#define MQTT_PATH_TOKENIZER_HPP

#include <algorithm>

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>

static constexpr char mqtt_level_separator = '/';

template<typename Iterator, typename Output>
inline void mqtt_path_tokenizer(Iterator first, Iterator last, Output write) {
    auto pos = std::find(first, last, mqtt_level_separator);
    while (write(first, pos) && pos != last) {
        first = std::next(pos);
        pos = std::find(first, last, mqtt_level_separator);
    }
}


template<typename Output>
inline void mqtt_path_tokenizer(MQTT_NS::string_view str, Output write) {
    mqtt_path_tokenizer(
        std::begin(str),
        std::end(str),
        [&write](MQTT_NS::string_view::const_iterator token_begin, MQTT_NS::string_view::const_iterator token_end) {
            return write(MQTT_NS::string_view(token_begin, std::distance(token_begin, token_end)));
        }
    );
}

// See if the topic is valid (does not contain # or +)
inline bool mqtt_valid_topic(MQTT_NS::string_view topic) {
    /*
     * Confirm the topic name is valid
     * Use rules from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    // The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name
    // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
    return
        ! topic.empty()
        && (topic.size() <= std::numeric_limits<std::uint16_t>::max())
        && (MQTT_NS::string_view::npos == topic.find_first_of(MQTT_NS::string_view("\0+#", 3)));
}

// See if the subscription is valid
inline bool mqtt_valid_subscription(MQTT_NS::string_view subscription) {
    /*
     * Confirm the topic pattern is valid before registering it.
     * Use rules from http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    if (subscription.empty() || (subscription.size() > std::numeric_limits<std::uint16_t>::max())) {
        return false;
    }

    for (MQTT_NS::string_view::size_type idx = subscription.find_first_of(MQTT_NS::string_view("\0+#", 3));
         MQTT_NS::string_view::npos != idx;
         idx = subscription.find_first_of(MQTT_NS::string_view("\0+#", 3), idx+1)) {
        BOOST_ASSERT(
            ('\0' == subscription[idx])
            || ('+'  == subscription[idx])
            || ('#'  == subscription[idx])
        );
        if ('\0' == subscription[idx]) {
            // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
            return false;
        }
        else if ('+' == subscription[idx]) {
            /*
             * Either must be the first character,
             * or be preceeded by a topic seperator.
             */
            if ((0 != idx) && ('/' != subscription[idx-1])) {
                return false;
            }

            /*
             * Either must be the last character,
             * or be followed by a topic seperator.
             */
            if ((subscription.size()-1 != idx) && ('/' != subscription[idx+1])) {
                return false;
            }
        }
        // multilevel wildcard
        else if ('#' == subscription[idx]) {
            /*
             * Must be absolute last character.
             * Must only be one multi level wild card.
             */
            if (idx != subscription.size()-1) {
                return false;
            }

            /*
             * If not the first character, then the
             * immediately preceeding character must
             * be a topic level separator.
             */
            if ((0 != idx) && ('/' != subscription[idx-1])) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return true;
}

#endif // !defined(MQTT_PATH_TOKENIZER_HPP)

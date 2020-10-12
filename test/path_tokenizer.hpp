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
     * Confirm the topic pattern is valid before using it.
     * Use rules from http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
     */
    for (size_t idx = topic.find_first_of("+#");
         MQTT_NS::string_view::npos != idx;
         idx = topic.find_first_of("+#", idx+1)) {
        if ('+' == topic[idx]) {
            /*
             * Either must be the first character,
             * or be preceeded by a topic seperator.
             */
            if ((0 != idx) && (mqtt_level_separator != topic[idx-1])) {
                return false;
            }
            /*
             * Either must be the last character,
             * or be followed by a topic seperator.
             */
            if ((topic.size()-1 != idx) && (mqtt_level_separator != topic[idx+1])) {
                return false;
            }
        }
        else { // multilevel wildcard
            /*
             * Must be absolute last character.
             * Must only be one multi level wild card.
             */
            if (idx != topic.size()-1) {
                return false;
            }
            /*
             * If not the first character, then the
             * immediately preceeding character must
             * be a topic level separator.
             */
            if ((0 != idx) && (mqtt_level_separator != topic[idx-1])) {
                return false;
            }
        }
    }

    return !topic.empty();
}

// See if the subscription is valid
inline bool mqtt_valid_subscription(MQTT_NS::string_view subscription) {
    if (subscription.empty()) {
        return false;
    }

    // If hash position is found, it should be the last character
    const size_t hash_position = subscription.find('#');
    return (hash_position == MQTT_NS::string_view::npos) || (hash_position == subscription.size() - 1);
}

#endif // !defined(MQTT_PATH_TOKENIZER_HPP)

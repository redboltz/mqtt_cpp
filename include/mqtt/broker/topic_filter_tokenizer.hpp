// Copyright wkl04 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_TOPIC_FILTER_TOKENIZER_HPP)
#define MQTT_BROKER_TOPIC_FILTER_TOKENIZER_HPP

#include <algorithm>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/string_view.hpp>

MQTT_BROKER_NS_BEGIN

static constexpr char topic_filter_separator = '/';

template<typename Iterator, typename Output>
inline void topic_filter_tokenizer(Iterator first, Iterator last, Output write) {
    auto pos = std::find(first, last, topic_filter_separator);
    while (write(first, pos) && pos != last) {
        first = std::next(pos);
        pos = std::find(first, last, topic_filter_separator);
    }
}


template<typename Output>
inline void topic_filter_tokenizer(string_view str, Output write) {
    topic_filter_tokenizer(
        std::begin(str),
        std::end(str),
        [&write](string_view::const_iterator token_begin, string_view::const_iterator token_end) {
            return write(
                string_view(
                    token_begin,
                    static_cast<std::size_t>(std::distance(token_begin, token_end)))
            );
        }
    );
}

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_TOPIC_FILTER_TOKENIZER_HPP

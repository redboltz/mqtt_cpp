// Copyright wkl04 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_TOPIC_FILTER_HPP)
#define MQTT_BROKER_TOPIC_FILTER_HPP

#include <algorithm>
#include <limits>
#include <cstdint>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/string_view.hpp>

#include <boost/assert.hpp>

MQTT_BROKER_NS_BEGIN

static constexpr char topic_filter_separator = '/';

template<typename Iterator>
inline Iterator topic_filter_tokenizer_next(Iterator first, Iterator last) {
    return std::find(first, last, topic_filter_separator);
}

template<typename Iterator, typename Output>
inline std::size_t topic_filter_tokenizer(Iterator first, Iterator last, Output write) {
    std::size_t count = 1;
    auto pos = topic_filter_tokenizer_next(first, last);
    while (write(first, pos) && pos != last) {
        first = std::next(pos);
        pos = topic_filter_tokenizer_next(first, last);
        ++count;
    }
    return count;
}


template<typename Output>
inline std::size_t topic_filter_tokenizer(string_view str, Output write) {
    return topic_filter_tokenizer(
        std::begin(str),
        std::end(str),
        [&write](string_view::const_iterator token_begin, string_view::const_iterator token_end) {
            return write(
                make_string_view(
                    token_begin,
                    token_end
                )
            );
        }
    );
}

#if defined(MQTT_STD_STRING_VIEW)
#define MQTT_STRING_VIEW_CONSTEXPR constexpr
#else  // defined(MQTT_STD_STRING_VIEW)
#define MQTT_STRING_VIEW_CONSTEXPR
#endif // defined(MQTT_STD_STRING_VIEW)


// TODO: Technically this function is simply wrong, since it's treating the
// topic pattern as if it were an ASCII sequence.
// To make this function correct per the standard, it would be necessary
// to conduct the search for the wildcard characters using a proper
// UTF-8 API to avoid problems of interpreting parts of multi-byte characters
// as if they were individual ASCII characters
MQTT_STRING_VIEW_CONSTEXPR
inline bool validate_topic_filter(string_view topic_filter) {
    /*
     * Confirm the topic pattern is valid before registering it.
     * Use rules from http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    if (topic_filter.empty() || (topic_filter.size() > std::numeric_limits<std::uint16_t>::max())) {
        return false;
    }

    for (string_view::size_type idx = topic_filter.find_first_of(string_view("\0+#", 3));
         string_view::npos != idx;
         idx = topic_filter.find_first_of(string_view("\0+#", 3), idx+1)) {
        BOOST_ASSERT(
            ('\0' == topic_filter[idx])
            || ('+'  == topic_filter[idx])
            || ('#'  == topic_filter[idx])
        );
        if ('\0' == topic_filter[idx]) {
            // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
            return false;
        }
        else if ('+' == topic_filter[idx]) {
            /*
             * Either must be the first character,
             * or be preceeded by a topic seperator.
             */
            if ((0 != idx) && ('/' != topic_filter[idx-1])) {
                return false;
            }

            /*
             * Either must be the last character,
             * or be followed by a topic seperator.
             */
            if ((topic_filter.size()-1 != idx) && ('/' != topic_filter[idx+1])) {
                return false;
            }
        }
        // multilevel wildcard
        else if ('#' == topic_filter[idx]) {
            /*
             * Must be absolute last character.
             * Must only be one multi level wild card.
             */
            if (idx != topic_filter.size()-1) {
                return false;
            }

            /*
             * If not the first character, then the
             * immediately preceeding character must
             * be a topic level separator.
             */
            if ((0 != idx) && ('/' != topic_filter[idx-1])) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return true;
}

#if defined(MQTT_STD_STRING_VIEW)
// The following rules come from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
static_assert( ! validate_topic_filter(""), "All Topic Names and Topic Filters MUST be at least one character long");
static_assert(validate_topic_filter("/"), "A Topic Name or Topic Filter consisting only of the ‘/’ character is valid");
static_assert( ! validate_topic_filter(string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
static_assert(validate_topic_filter(" "), "Topic Names and Topic Filters can include the space character");
static_assert(validate_topic_filter("/////"), "Topic level separators can appear anywhere in a Topic Filter or Topic Name. Adjacent Topic level separators indicate a zero-length topic level");
static_assert(validate_topic_filter("#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert(validate_topic_filter("/#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert(validate_topic_filter("+/#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("+#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("++"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("f#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("#/"), "In either case the multi-level wildcard character MUST be the last character specified in the Topic Filter");

static_assert(validate_topic_filter("+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/alice/sue"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("bob/alice/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/alice/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/+/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/+/sue/#"), "The single-level wildcard can be used at more than one level in the Topic Filter and can be used in conjunction with the multi-level wildcard");
static_assert( ! validate_topic_filter("+a"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("a+"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("/a+"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("a+/"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("/a+/"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
#endif // defined(MQTT_STD_STRING_VIEW)

MQTT_STRING_VIEW_CONSTEXPR
inline bool validate_topic_name(string_view topic_name) {
    /*
     * Confirm the topic name is valid
     * Use rules from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    // The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name
    // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
    return
        ! topic_name.empty()
        && (topic_name.size() <= std::numeric_limits<std::uint16_t>::max())
        && (string_view::npos == topic_name.find_first_of(string_view("\0+#", 3)));
}

#if defined(MQTT_STD_STRING_VIEW)
// The following rules come from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
static_assert( ! validate_topic_name(""), "All Topic Names and Topic Filters MUST be at least one character long");
static_assert(validate_topic_name("/"), "A Topic Name or Topic Filter consisting only of the ‘/’ character is valid");
static_assert( ! validate_topic_name(string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
static_assert(validate_topic_name(" "), "Topic Names and Topic Filters can include the space character");
static_assert(validate_topic_name("/////"), "Topic level separators can appear anywhere in a Topic Filter or Topic Name. Adjacent Topic level separators indicate a zero-length topic level");
static_assert( ! validate_topic_name("#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("+"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("/#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("+/#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("f#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("#/"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
#endif // defined(MQTT_STD_STRING_VIEW)

MQTT_STRING_VIEW_CONSTEXPR
inline bool compare_topic_filter(string_view topic_filter, string_view topic_name) {
    if ( ! validate_topic_filter(topic_filter)) {
        BOOST_ASSERT(validate_topic_filter(topic_filter));
        return false;
    }

    if ( ! validate_topic_name(topic_name)) {
        BOOST_ASSERT(validate_topic_name(topic_name));
        return false;
    }

    // TODO: The Server MUST NOT match Topic Filters starting with a wildcard character (# or +) with Topic Names beginning with a $ character
    for (string_view::size_type idx = topic_filter.find_first_of("+#");
         string_view::npos != idx;
         idx = topic_filter.find_first_of("+#")) {
        BOOST_ASSERT(
            ('+' == topic_filter[idx])
            || ('#' == topic_filter[idx])
        );

        if ('+' == topic_filter[idx]) {
            // Compare everything up to the first +
            if (topic_filter.substr(0, idx) == topic_name.substr(0, idx)) {
                /*
                 * We already know thanks to the topic filter being validated
                 * that the + symbol is directly touching '/'s on both sides
                 * (if not the first or last character), so we don't need to
                 * double check that.
                 *
                 * By simply removing the prefix that we've compared and letting
                 * the loop continue, we get the proper comparison of the '/'s
                 * automatically when the loop continues.
                 */
                topic_filter.remove_prefix(idx+1);
                /*
                 * It's a bit more complicated for the incoming topic though
                 * as we need to remove everything up to the next seperator.
                 */
                topic_name.remove_prefix(topic_name.find('/', idx));
            }
            else {
                return false;
            }
        }
        // multilevel wildcard
        else {
            /*
             * Compare up to where the multilevel wild card is found
             * and then anything after that matches the wildcard.
             */
            return topic_filter.substr(0, idx) == topic_name.substr(0, idx);
        }
    }

    // No + or # found in the remaining topic filter. Just do a string compare.
    return topic_filter == topic_name;
}

#if defined(MQTT_STD_STRING_VIEW)
static_assert(compare_topic_filter("bob", "bob"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("Bob", "bob"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("bob", "boB"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("/bob", "bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob/", "bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob", "/bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob", "bob/"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert(compare_topic_filter("bob/alice", "bob/alice"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/sue", "bob/alice/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob//////sue", "bob//////sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/#", "bob//////sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob///#", "bob/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/+/sue", "bob/alice/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob/+/sue", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/mary/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob/alice/mary/sue/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
#endif // defined(MQTT_STD_STRING_VIEW)

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_TOPIC_FILTER_HPP

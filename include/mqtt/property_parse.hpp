// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_PARSE_HPP)
#define MQTT_PROPERTY_PARSE_HPP

#include <vector>

#include <mqtt/optional.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/variable_length.hpp>

namespace mqtt {
namespace v5 {
namespace property {

template <typename It>
inline
mqtt::optional<property_variant> parse_one(It& begin, It end) {
    if (begin == end) return mqtt::nullopt;
    try {
        auto it = begin;
        switch (static_cast<property::id>(*it++)) {
        case id::payload_format_indicator: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = payload_format_indicator(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::message_expiry_interval: {
            if (it + 4 > end) return mqtt::nullopt;
            auto p = message_expiry_interval(it, it + 4);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::content_type: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = content_type_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::response_topic: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = response_topic_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::correlation_data: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = correlation_data_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::subscription_identifier: {
            auto lim = std::min(end, it + 4);
            auto val_consumed = variable_length(it, lim);
            auto val = std::get<0>(val_consumed);
            auto consumed = std::get<1>(val_consumed);
            if (consumed == 0) return mqtt::nullopt;
            auto p = subscription_identifier(val);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::session_expiry_interval: {
            if (it + 4 > end) return mqtt::nullopt;
            auto p = session_expiry_interval(it, it + 4);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::assigned_client_identifier: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = assigned_client_identifier_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::server_keep_alive: {
            if (it + 2 > end) return mqtt::nullopt;
            auto p = server_keep_alive(it, it + 2);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::authentication_method: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = authentication_method_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::authentication_data: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = authentication_data_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::request_problem_information: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = request_problem_information(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::will_delay_interval: {
            if (it + 4 > end) return mqtt::nullopt;
            auto p = will_delay_interval(it, it + 4);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::request_response_information: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = request_response_information(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::response_information: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = response_information_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::server_reference: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = server_reference_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::reason_string: {
            if (it + 2 > end) return mqtt::nullopt;
            auto len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + len > end) return mqtt::nullopt;
            auto p = reason_string_ref(mqtt::string_view(&*it, len));
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::receive_maximum: {
            if (it + 2 > end) return mqtt::nullopt;
            auto p = receive_maximum(it, it + 2);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::topic_alias_maximum: {
            if (it + 2 > end) return mqtt::nullopt;
            auto p = topic_alias_maximum(it, it + 2);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::topic_alias: {
            if (it + 2 > end) return mqtt::nullopt;
            auto p = topic_alias(it, it + 2);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::maximum_qos: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = maximum_qos(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::retain_available: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = retain_available(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::user_property: {
            if (it + 2 > end) return mqtt::nullopt;
            auto key_len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + key_len > end) return mqtt::nullopt;
            auto key = mqtt::string_view(&*it, key_len);
            it += key_len;

            if (it + 2 > end) return mqtt::nullopt;
            auto val_len = make_uint16_t(it, it + 2);
            it += 2;
            if (it + val_len > end) return mqtt::nullopt;
            auto val = mqtt::string_view(&*it, key_len);

            auto p = user_property_ref(key, val);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::maximum_packet_size: {
            if (it + 4 > end) return mqtt::nullopt;
            auto p = maximum_packet_size(it, it + 4);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::wildcard_subscription_available: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = wildcard_subscription_available(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::subscription_identifier_available: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = subscription_identifier_available(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        case id::shared_subscription_available: {
            if (it + 1 > end) return mqtt::nullopt;
            auto p = shared_subscription_available(it, it + 1);
            std::advance(begin, static_cast<typename std::iterator_traits<It>::difference_type>(p.size()));
            return property_variant(p);
        } break;
        }
    }
    catch (property_parse_error const&) {
        return mqtt::nullopt;
    }
    return mqtt::nullopt;
}

template <typename It>
inline
std::vector<property_variant> parse(It& it, It end) {
    std::vector<property_variant> props;
    while (true) {
        if (auto p = parse_one(it, end)) {
            props.push_back(std::move(*p));
        }
        else {
            break;
        }
    }
    return props;
}

template <typename It>
inline
mqtt::optional<std::vector<property_variant>> parse_with_length(It& it, It end) {
    auto r = variable_length(
        it,
        std::min(it + 4, end)
    );
    auto property_length = static_cast<typename std::iterator_traits<It>::difference_type>(std::get<0>(r));
    it += static_cast<typename std::iterator_traits<It>::difference_type>(std::get<1>(r));

    if (end < it + property_length) {
        return mqtt::nullopt;
    }

    It prop_begin = it;
    It prop_end = it +  property_length;
    auto pvs = v5::property::parse(prop_begin, prop_end);
    std::advance(it, property_length);
    return pvs;
}

} // namespace property
} // namespace v5
} // namespace mqtt

#endif // MQTT_PROPERTY_PARSE_HPP

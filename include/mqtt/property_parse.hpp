// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_PARSE_HPP)
#define MQTT_PROPERTY_PARSE_HPP

#include <vector>

#include <mqtt/namespace.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/variable_length.hpp>
#include <mqtt/move.hpp>

namespace MQTT_NS {
namespace v5 {
namespace property {

inline
optional<property_variant> parse_one(buffer& buf) {
    if (buf.empty()) return nullopt;
    try {
        auto id = static_cast<property::id>(buf.front());
        buf.remove_prefix(1);
        switch (id) {
        case id::payload_format_indicator: {
            if (buf.size() < 1) return nullopt;
            auto p = payload_format_indicator(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::message_expiry_interval: {
            if (buf.size() < 4) return nullopt;
            auto p = message_expiry_interval(buf.begin(), std::next(buf.begin(), 4));
            buf.remove_prefix(4);
            return property_variant(p);
        } break;
        case id::content_type: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = content_type(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::response_topic: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = response_topic(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::correlation_data: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = correlation_data(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::subscription_identifier: {
            auto val_consumed = variable_length(buf.begin(), buf.end());
            auto val = std::get<0>(val_consumed);
            auto consumed = std::get<1>(val_consumed);
            if (consumed == 0) return nullopt;
            auto p = subscription_identifier(val);
            buf.remove_prefix(consumed);
            return property_variant(p);
        } break;
        case id::session_expiry_interval: {
            if (buf.size() < 4) return nullopt;
            auto p = session_expiry_interval(buf.begin(), std::next(buf.begin(), 4));
            buf.remove_prefix(4);
            return property_variant(p);
        } break;
        case id::assigned_client_identifier: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = assigned_client_identifier(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::server_keep_alive: {
            if (buf.size() < 2) return nullopt;
            auto p = server_keep_alive(buf.begin(), std::next(buf.begin(), 2));
            buf.remove_prefix(2);
            return property_variant(p);
        } break;
        case id::authentication_method: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = authentication_method(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::authentication_data: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = authentication_data(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::request_problem_information: {
            if (buf.size() < 1) return nullopt;
            auto p = request_problem_information(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::will_delay_interval: {
            if (buf.size() < 4) return nullopt;
            auto p = will_delay_interval(buf.begin(), std::next(buf.begin(), 4));
            buf.remove_prefix(4);
            return property_variant(p);
        } break;
        case id::request_response_information: {
            if (buf.size() < 1) return nullopt;
            auto p = request_response_information(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::response_information: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = response_information(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::server_reference: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = server_reference(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::reason_string: {
            if (buf.size() < 2) return nullopt;
            auto len = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + len) return nullopt;
            auto p = reason_string(buf.substr(2, len));
            buf.remove_prefix(2 + len);
            return property_variant(p);
        } break;
        case id::receive_maximum: {
            if (buf.size() < 2) return nullopt;
            auto p = receive_maximum(buf.begin(), std::next(buf.begin(), 2));
            buf.remove_prefix(2);
            return property_variant(p);
        } break;
        case id::topic_alias_maximum: {
            if (buf.size() < 2) return nullopt;
            auto p = topic_alias_maximum(buf.begin(), std::next(buf.begin(), 2));
            buf.remove_prefix(2);
            return property_variant(p);
        } break;
        case id::topic_alias: {
            if (buf.size() < 2) return nullopt;
            auto p = topic_alias(buf.begin(), std::next(buf.begin(), 2));
            buf.remove_prefix(2);
            return property_variant(p);
        } break;
        case id::maximum_qos: {
            if (buf.size() < 1) return nullopt;
            auto p = maximum_qos(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::retain_available: {
            if (buf.size() < 1) return nullopt;
            auto p = retain_available(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::user_property: {
            if (buf.size() < 2) return nullopt;
            auto keylen = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + keylen) return nullopt;
            auto key = buf.substr(2, keylen);
            buf.remove_prefix(2 + keylen);

            if (buf.size() < 2) return nullopt;
            auto vallen = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
            if (buf.size() < 2U + vallen) return nullopt;
            auto val = buf.substr(2, vallen);

            auto p = user_property(force_move(key), force_move(val));
            buf.remove_prefix(2 + vallen);

            return property_variant(p);
        } break;
        case id::maximum_packet_size: {
            if (buf.size() < 4) return nullopt;
            auto p = maximum_packet_size(buf.begin(), std::next(buf.begin(), 4));
            buf.remove_prefix(4);
            return property_variant(p);
        } break;
        case id::wildcard_subscription_available: {
            if (buf.size() < 1) return nullopt;
            auto p = wildcard_subscription_available(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::subscription_identifier_available: {
            if (buf.size() < 1) return nullopt;
            auto p = subscription_identifier_available(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        case id::shared_subscription_available: {
            if (buf.size() < 1) return nullopt;
            auto p = shared_subscription_available(buf.begin(), std::next(buf.begin(), 1));
            buf.remove_prefix(1);
            return property_variant(p);
        } break;
        }
    }
    catch (property_parse_error const&) {
        return nullopt;
    }
    return nullopt;
}

inline
std::vector<property_variant> parse(buffer buf) {
    std::vector<property_variant> props;
    while (true) {
        if (auto ret = parse_one(buf)) {
            props.push_back(force_move(ret.value()));
        }
        else {
            break;
        }
    }
    return props;
}

} // namespace property
} // namespace v5
} // namespace MQTT_NS

#endif // MQTT_PROPERTY_PARSE_HPP

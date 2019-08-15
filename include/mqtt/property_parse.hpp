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

inline
std::pair<mqtt::optional<property_variant>, buffer> parse_one(buffer buf) {
    if (buf.empty()) return { mqtt::nullopt, std::move(buf) };
    try {
        auto id = static_cast<property::id>(buf.front());
        buf = std::move(buf).substr(1);
        switch (id) {
        case id::payload_format_indicator: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = payload_format_indicator(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::message_expiry_interval: {
            if (buf.size() < 4) return { mqtt::nullopt, std::move(buf) };
            auto p = message_expiry_interval(buf.begin(), buf.begin() + 4);
            buf = std::move(buf).substr(4);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::content_type: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = content_type(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::response_topic: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = response_topic(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::correlation_data: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = correlation_data(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::subscription_identifier: {
            auto val_consumed = variable_length(buf.begin(), buf.end());
            auto val = std::get<0>(val_consumed);
            auto consumed = std::get<1>(val_consumed);
            if (consumed == 0) return { mqtt::nullopt, std::move(buf) };
            auto p = subscription_identifier(val);
            buf = std::move(buf).substr(consumed);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::session_expiry_interval: {
            if (buf.size() < 4) return { mqtt::nullopt, std::move(buf) };
            auto p = session_expiry_interval(buf.begin(), buf.begin() + 4);
            buf = std::move(buf).substr(4);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::assigned_client_identifier: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = assigned_client_identifier(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::server_keep_alive: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto p = server_keep_alive(buf.begin(), buf.begin() + 2);
            buf = std::move(buf).substr(2);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::authentication_method: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = authentication_method(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::authentication_data: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = authentication_data(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::request_problem_information: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = request_problem_information(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::will_delay_interval: {
            if (buf.size() < 4) return { mqtt::nullopt, std::move(buf) };
            auto p = will_delay_interval(buf.begin(), buf.begin() + 4);
            buf = std::move(buf).substr(4);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::request_response_information: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = request_response_information(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::response_information: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = response_information(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::server_reference: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = server_reference(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::reason_string: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto len = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + len) return { mqtt::nullopt, std::move(buf) };
            auto p = reason_string(buf.substr(2, len));
            buf = std::move(buf).substr(2 + len);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::receive_maximum: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto p = receive_maximum(buf.begin(), buf.begin() + 2);
            buf = std::move(buf).substr(2);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::topic_alias_maximum: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto p = topic_alias_maximum(buf.begin(), buf.begin() + 2);
            buf = std::move(buf).substr(2);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::topic_alias: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto p = topic_alias(buf.begin(), buf.begin() + 2);
            buf = std::move(buf).substr(2);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::maximum_qos: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = maximum_qos(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::retain_available: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = retain_available(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::user_property: {
            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto keylen = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + keylen) return { mqtt::nullopt, std::move(buf) };
            auto key = buf.substr(2, keylen);
            buf = std::move(buf).substr(2 + keylen);

            if (buf.size() < 2) return { mqtt::nullopt, std::move(buf) };
            auto vallen = make_uint16_t(buf.begin(), buf.begin() + 2);
            if (buf.size() < 2U + vallen) return { mqtt::nullopt, std::move(buf) };
            auto val = buf.substr(2, vallen);

            auto p = user_property(std::move(key), std::move(val));
            buf = std::move(buf).substr(2 + vallen);

            return { property_variant(p), std::move(buf) };
        } break;
        case id::maximum_packet_size: {
            if (buf.size() < 4) return { mqtt::nullopt, std::move(buf) };
            auto p = maximum_packet_size(buf.begin(), buf.begin() + 4);
            buf = std::move(buf).substr(4);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::wildcard_subscription_available: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = wildcard_subscription_available(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::subscription_identifier_available: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = subscription_identifier_available(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        case id::shared_subscription_available: {
            if (buf.size() < 1) return { mqtt::nullopt, std::move(buf) };
            auto p = shared_subscription_available(buf.begin(), buf.begin() + 1);
            buf = std::move(buf).substr(1);
            return { property_variant(p), std::move(buf) };
        } break;
        }
    }
    catch (property_parse_error const&) {
        return { mqtt::nullopt, std::move(buf) };
    }
    return { mqtt::nullopt, std::move(buf) };
}

inline
std::pair<std::vector<property_variant>, buffer> parse(buffer buf) {
    std::vector<property_variant> props;
    while (true) {
        auto ret = parse_one(std::move(buf));
        if (ret.first) {
            props.push_back(std::move(ret.first.value()));
            buf = std::move(ret.second);
        }
        else {
            break;
        }
    }
    return { props, std::move(buf) };
}

} // namespace property
} // namespace v5
} // namespace mqtt

#endif // MQTT_PROPERTY_PARSE_HPP

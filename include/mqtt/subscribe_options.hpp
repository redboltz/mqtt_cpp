// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIBE_OPTIONS_HPP)
#define MQTT_SUBSCRIBE_OPTIONS_HPP

#include <cstdint>
#include <ostream>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

enum class retain_handling : std::uint8_t
{
    send = 0b00000000,
    send_only_new_subscription = 0b00010000,
    not_send = 0b00100000,
};
enum class rap : std::uint8_t
{
    dont = 0b00000000,
    retain = 0b00001000,
};
enum class nl : std::uint8_t
{
    no = 0b00000000,
    yes = 0b00000100,
};
enum class qos : std::uint8_t
{
    at_most_once = 0b00000000,
    at_least_once = 0b00000001,
    exactly_once = 0b00000010,
};

struct subscribe_options final {
    constexpr subscribe_options() = delete;
    ~subscribe_options() = default;
    constexpr subscribe_options(subscribe_options &&) = default;
    constexpr subscribe_options(subscribe_options const&) = default;
    constexpr subscribe_options& operator=(subscribe_options &&) = default;
    constexpr subscribe_options& operator=(subscribe_options const&) = default;

    explicit constexpr subscribe_options(std::uint8_t value) : data_(value) { }

    constexpr subscribe_options(retain_handling value) : data_(static_cast<std::uint8_t>(value)) { }
    constexpr subscribe_options(rap value)             : data_(static_cast<std::uint8_t>(value)) { }
    constexpr subscribe_options(nl value)              : data_(static_cast<std::uint8_t>(value)) { }
    constexpr subscribe_options(qos value)             : data_(static_cast<std::uint8_t>(value)) { }

    constexpr subscribe_options operator|(subscribe_options rhs) const { return subscribe_options(data_ | rhs.data_); }
    constexpr subscribe_options operator|(retain_handling rhs) const   { return *this | subscribe_options(rhs); }
    constexpr subscribe_options operator|(rap rhs) const               { return *this | subscribe_options(rhs); }
    constexpr subscribe_options operator|(nl rhs) const                { return *this | subscribe_options(rhs); }
    constexpr subscribe_options operator|(qos rhs) const               { return *this | subscribe_options(rhs); }

    constexpr subscribe_options& operator|=(subscribe_options rhs) { return (*this = (*this | rhs)); }
    constexpr subscribe_options& operator|=(retain_handling rhs)   { return (*this = (*this | rhs)); }
    constexpr subscribe_options& operator|=(rap rhs)               { return (*this = (*this | rhs)); }
    constexpr subscribe_options& operator|=(nl rhs)                { return (*this = (*this | rhs)); }
    constexpr subscribe_options& operator|=(qos rhs)               { return (*this = (*this | rhs)); }

    constexpr retain_handling get_retain_handling() const
    { return static_cast<retain_handling>(data_ & 0b00110000); }
    constexpr rap get_rap() const
    { return static_cast<rap>(data_ & 0b00001000); }
    constexpr nl get_nl() const
    { return static_cast<nl>(data_ & 0b00000100); }
    constexpr qos get_qos() const
    { return static_cast<qos>(data_ & 0b00000011); }

    explicit constexpr operator std::uint8_t() const { return data_; }
private:
    std::uint8_t data_;
};

constexpr subscribe_options operator|(retain_handling lhs, rap rhs) { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(retain_handling lhs, nl rhs)  { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(retain_handling lhs, qos rhs) { return subscribe_options(lhs) | rhs; }

constexpr subscribe_options operator|(rap lhs, retain_handling rhs) { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(rap lhs, nl rhs)              { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(rap lhs, qos rhs)             { return subscribe_options(lhs) | rhs; }

constexpr subscribe_options operator|(nl lhs, retain_handling rhs)  { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(nl lhs, rap rhs)              { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(nl lhs, qos rhs)              { return subscribe_options(lhs) | rhs; }

constexpr subscribe_options operator|(qos lhs, retain_handling rhs) { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(qos lhs, rap rhs)             { return subscribe_options(lhs) | rhs; }
constexpr subscribe_options operator|(qos lhs, nl rhs)              { return subscribe_options(lhs) | rhs; }

constexpr char const* retain_handling_to_str(retain_handling v) {
    switch(v) {
    case retain_handling::send:                       return "send";
    case retain_handling::send_only_new_subscription: return "send_only_new_subscription";
    case retain_handling::not_send:                   return "not_send";
    default:                                          return "invalid_retain_handling";
    }
}

inline
std::ostream& operator<<(std::ostream& os, retain_handling val)
{
    os << retain_handling_to_str(val);
    return os;
}

constexpr char const* rap_to_str(rap v) {
    switch(v) {
    case rap::dont:   return "dont";
    case rap::retain: return "retain";
    default:          return "invalid_rap";
    }
}

inline
std::ostream& operator<<(std::ostream& os, rap val)
{
    os << rap_to_str(val);
    return os;
}

constexpr char const* nl_to_str(nl v) {
    switch(v) {
    case nl::no:   return "no";
    case nl::yes:  return "yes";
    default:       return "invalid_nl";
    }
}

inline
std::ostream& operator<<(std::ostream& os, nl val)
{
    os << nl_to_str(val);
    return os;
}

constexpr char const* qos_to_str(qos v) {
    switch(v) {
    case qos::at_most_once:  return "at_most_once";
    case qos::at_least_once: return "at_least_once";
    case qos::exactly_once:  return "exactly_once";
    default:                 return "invalid_qos";
    }
}

inline
std::ostream& operator<<(std::ostream& os, qos val)
{
    os << qos_to_str(val);
    return os;
}

} // namespace MQTT_NS

#endif // MQTT_SUBSCRIBE_OPTIONS_HPP

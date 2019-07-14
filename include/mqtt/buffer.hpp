// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BUFFER_HPP)
#define MQTT_BUFFER_HPP

#include <stdexcept>
#include <utility>
#include <type_traits>

#include <mqtt/string_view.hpp>
#include <mqtt/shared_ptr_array.hpp>

namespace mqtt {

class buffer : public mqtt::string_view {
public:
    explicit constexpr buffer(mqtt::string_view v = mqtt::string_view())
        : mqtt::string_view(std::move(v)) {}

    buffer(std::string) = delete; // to avoid misuse

    buffer(mqtt::string_view view, shared_ptr_array spa)
        : mqtt::string_view(std::move(view)),
          lifetime_(std::move(spa)) {
    }

    buffer substr(std::size_t offset, std::size_t length = mqtt::string_view::npos) const& {
        // range is checked in mqtt::string_view::substr.
        return buffer(mqtt::string_view::substr(offset, length), lifetime_);
    }

    buffer substr(std::size_t offset, std::size_t length = mqtt::string_view::npos) && {
        // range is checked in mqtt::string_view::substr.
        return buffer(mqtt::string_view::substr(offset, length), std::move(lifetime_));
    }

private:
    shared_ptr_array lifetime_;
};

inline namespace literals {

inline buffer operator""_mb(char const* str, std::size_t length) {
    return buffer(mqtt::string_view(str, length));
}

} // namespace literals

} // namespace mqtt

#include <boost/asio/buffer.hpp>

namespace boost {
namespace asio {

inline const_buffer buffer(mqtt::buffer const& data) {
    return buffer(data.data(), data.size());
}

} // namespace asio
} // namespace boost

#endif // MQTT_BUFFER_HPP

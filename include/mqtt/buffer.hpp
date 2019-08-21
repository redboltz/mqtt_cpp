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

/**
 * @brief buffer that has string_view interface
 * This class provides string_view interface.
 * This class hold string_view target's lifetime optionally.
 */
class buffer : public mqtt::string_view {
public:
    /**
     * @brief string_view constructor
     * @param sv string_view
     * This constructor doesn't hold the sv target's lifetime.
     * It behaves as string_view. Caller needs to manage the target lifetime.
     */
    explicit constexpr buffer(mqtt::string_view sv = mqtt::string_view())
        : mqtt::string_view(std::move(sv)) {}

    /**
     * @brief string constructor (deleted)
     * @param string
     * This constructor is intentionally deleted.
     * Consider `buffer(std::string("ABC"))`, the buffer points to dangling reference.
     */
    buffer(std::string) = delete; // to avoid misuse

    /**
     * @brief string_view and lifetime constructor
     * @param sv string_view
     * @param spa shared_ptr_array that holds sv target's lifetime
     * If user creates buffer via this constructor, spa's lifetime is held by the buffer.
     */
    buffer(mqtt::string_view sv, shared_ptr_array spa)
        : mqtt::string_view(std::move(sv)),
          lifetime_(std::move(spa)) {
    }

    /**
     * @brief get substring
     * The returned buffer ragnge is the same as std::string_view::substr().
     * In addition the lifetime is shared between returned buffer and this buffer.
     * @param offset offset point of the buffer
     * @param length length of the buffer, If the length is mqtt::string_view::npos
     *               then the length is from offset to the end of string.
     */
    buffer substr(std::size_t offset, std::size_t length = mqtt::string_view::npos) const& {
        // range is checked in mqtt::string_view::substr.
        return buffer(mqtt::string_view::substr(offset, length), lifetime_);
    }

    /**
     * @brief get substring
     * The returned buffer ragnge is the same as std::string_view::substr().
     * In addition the lifetime is moved to returned buffer.
     * @param offset offset point of the buffer
     * @param length length of the buffer, If the length is mqtt::string_view::npos
     *               then the length is from offset to the end of string.
     */
    buffer substr(std::size_t offset, std::size_t length = mqtt::string_view::npos) && {
        // range is checked in mqtt::string_view::substr.
        return buffer(mqtt::string_view::substr(offset, length), std::move(lifetime_));
    }

private:
    shared_ptr_array lifetime_;
};

inline namespace literals {

/**
 * @brief user defined literals for buffer
 * If user use this out of mqtt scope, then user need to declare
 * `using namespace mqtt::literals`.
 * When user write "ABC"_mb, then this function is called.
 * The created buffer doesn't hold any lifetimes because the string literals
 * has static strage duration, so buffer doesn't need to hold the lifetime.
 *
 * @param str     the address of the string literal
 * @param length  the length of the string literal
 * @return buffer
 */
inline buffer operator""_mb(char const* str, std::size_t length) {
    return buffer(mqtt::string_view(str, length));
}

/**
 * @brief create buffer from the pair of iterators
 * It copies string that from b to e into shared_ptr_array.
 * Then create buffer and return it.
 * The buffer holds the lifetime of shared_ptr_array.
 *
 * @param str     the address of the string literal
 * @param length  the length of the string literal
 * @return buffer
 */
template <typename Iterator>
inline buffer allocate_buffer(Iterator b, Iterator e) {
    auto size = static_cast<std::size_t>(std::distance(b, e));
    auto spa = make_shared_ptr_array(size);
    std::copy(b, e, spa.get());
    return buffer(string_view(spa.get(), size), spa);
}

} // namespace literals

} // namespace mqtt

#include <boost/asio/buffer.hpp>

namespace boost {
namespace asio {

/**
 * @brief create boost::asio::const_buffer from the mqtt::buffer
 * boost::asio::const_buffer is a kind of view class.
 * So the class doesn't hold any lifetimes.
 * The caller needs to manage data's lifetime.
 *
 * @param  data  source mqtt::buffer
 * @return boost::asio::const_buffer
 */
inline const_buffer buffer(mqtt::buffer const& data) {
    return buffer(data.data(), data.size());
}

} // namespace asio
} // namespace boost

#endif // MQTT_BUFFER_HPP

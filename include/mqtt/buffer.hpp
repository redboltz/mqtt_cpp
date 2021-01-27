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

#include <boost/asio/buffer.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/shared_ptr_array.hpp>
#include <mqtt/move.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

/**
 * @brief buffer that has string_view interface
 * This class provides string_view interface.
 * This class hold string_view target's lifetime optionally.
 */
class buffer : public string_view {
public:
    /**
     * @brief string_view constructor
     * @param sv string_view
     * This constructor doesn't hold the sv target's lifetime.
     * It behaves as string_view. Caller needs to manage the target lifetime.
     */
    explicit constexpr buffer(string_view sv = string_view())
        : string_view(force_move(sv)) {}

    /**
     * @brief string constructor (deleted)
     * @param string
     * This constructor is intentionally deleted.
     * Consider `buffer(std::string("ABC"))`, the buffer points to dangling reference.
     */
    explicit buffer(std::string) = delete; // to avoid misuse

    /**
     * @brief string_view and lifetime constructor
     * @param sv string_view
     * @param spa shared_ptr_array that holds sv target's lifetime
     * If user creates buffer via this constructor, spa's lifetime is held by the buffer.
     */
    buffer(string_view sv, const_shared_ptr_array spa)
        : string_view(force_move(sv)),
          lifetime_(force_move(spa)) {
    }

    /**
     * @brief get substring
     * The returned buffer ragnge is the same as std::string_view::substr().
     * In addition the lifetime is shared between returned buffer and this buffer.
     * @param offset offset point of the buffer
     * @param length length of the buffer, If the length is string_view::npos
     *               then the length is from offset to the end of string.
     */
    buffer substr(std::size_t offset, std::size_t length = string_view::npos) const& {
        // range is checked in string_view::substr.
        return buffer(string_view::substr(offset, length), lifetime_);
    }

    /**
     * @brief get substring
     * The returned buffer ragnge is the same as std::string_view::substr().
     * In addition the lifetime is moved to returned buffer.
     * @param offset offset point of the buffer
     * @param length length of the buffer, If the length is string_view::npos
     *               then the length is from offset to the end of string.
     */
    buffer substr(std::size_t offset, std::size_t length = string_view::npos) && {
        // range is checked in string_view::substr.
        return buffer(string_view::substr(offset, length), force_move(lifetime_));
    }

    /**
     * @brief check the buffer has lifetime.
     * @return true  the buffer has lifetime.
     *         false the buffer doesn't have lifetime, This means the buffer is a pure view.
     */
    bool has_life() const {
        return lifetime_.get();
    }

private:
    const_shared_ptr_array lifetime_;
};

inline namespace literals {

/**
 * @brief user defined literals for buffer
 * If user use this out of mqtt scope, then user need to declare
 * `using namespace literals`.
 * When user write "ABC"_mb, then this function is called.
 * The created buffer doesn't hold any lifetimes because the string literals
 * has static strage duration, so buffer doesn't need to hold the lifetime.
 *
 * @param str     the address of the string literal
 * @param length  the length of the string literal
 * @return buffer
 */
inline buffer operator""_mb(char const* str, std::size_t length) {
    return buffer(string_view(str, length));
}

} // namespace literals

/**
 * @brief create buffer from the pair of iterators
 * It copies string that from b to e into shared_ptr_array.
 * Then create buffer and return it.
 * The buffer holds the lifetime of shared_ptr_array.
 *
 * @param b  begin position iterator
 * @param e  end position iterator
 * @return buffer
 */
template <typename Iterator>
inline buffer allocate_buffer(Iterator b, Iterator e) {
    auto size = static_cast<std::size_t>(std::distance(b, e));
    auto spa = make_shared_ptr_array(size);
    std::copy(b, e, spa.get());
    auto view = string_view(spa.get(), size);
    return buffer(view, force_move(spa));
}

/**
 * @brief create buffer from the string_view
 * It copies string that from string_view into shared_ptr_array.
 * Then create buffer and return it.
 * The buffer holds the lifetime of shared_ptr_array.
 *
 * @param sv  the source string_view
 * @return buffer
 */
inline buffer allocate_buffer(string_view sv) {
    return allocate_buffer(sv.begin(), sv.end());
}

inline buffer const* buffer_sequence_begin(buffer const& buf) {
    return std::addressof(buf);
}

inline buffer const* buffer_sequence_end(buffer const& buf) {
    return std::addressof(buf) + 1;
}

template <typename Col>
inline typename Col::const_iterator buffer_sequence_begin(Col const& col) {
    return col.cbegin();
}

template <typename Col>
inline typename Col::const_iterator buffer_sequence_end(Col const& col) {
    return col.cend();
}

namespace detail {

template <typename>
char buffer_sequence_begin_helper(...);

template <typename T>
char (&buffer_sequence_begin_helper(
    T* t,
    typename std::enable_if<
        !std::is_same<
            decltype(buffer_sequence_begin(*t)),
            void
        >::value
    >::type*)
)[2];

template <typename>
char buffer_sequence_end_helper(...);

template <typename T>
char (&buffer_sequence_end_helper(
    T* t,
    typename std::enable_if<
        !std::is_same<
            decltype(buffer_sequence_end(*t)),
            void
        >::value
    >::type*)
)[2];

template <typename, typename>
char (&buffer_sequence_element_type_helper(...))[2];

template <typename T, typename Buffer>
char buffer_sequence_element_type_helper(
    T* t,
    typename std::enable_if<
        std::is_convertible<
            decltype(*buffer_sequence_begin(*t)),
            Buffer
        >::value
    >::type*
);

template <typename T, typename Buffer>
struct is_buffer_sequence_class
    : std::integral_constant<bool,
      sizeof(buffer_sequence_begin_helper<T>(0, 0)) != 1 &&
      sizeof(buffer_sequence_end_helper<T>(0, 0)) != 1 &&
      sizeof(buffer_sequence_element_type_helper<T, Buffer>(0, 0)) == 1>
{
};

} // namespace detail

template <typename T>
struct is_buffer_sequence :
    std::conditional<
        std::is_class<T>::value,
        detail::is_buffer_sequence_class<T, buffer>,
        std::false_type
    >::type
{
};

template <>
struct is_buffer_sequence<buffer> : std::true_type
{
};

} // namespace MQTT_NS

namespace boost {
namespace asio {

/**
 * @brief create boost::asio::const_buffer from the MQTT_NS::buffer
 * boost::asio::const_buffer is a kind of view class.
 * So the class doesn't hold any lifetimes.
 * The caller needs to manage data's lifetime.
 *
 * @param  data  source MQTT_NS::buffer
 * @return boost::asio::const_buffer
 */
inline const_buffer buffer(MQTT_NS::buffer const& data) {
    return buffer(data.data(), data.size());
}

} // namespace asio
} // namespace boost

#endif // MQTT_BUFFER_HPP

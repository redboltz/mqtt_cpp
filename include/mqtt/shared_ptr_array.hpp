// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SHARED_PTR_ARRAY_HPP)
#define MQTT_SHARED_PTR_ARRAY_HPP

#if defined(_DOXYGEN_)

/**
 * @brief Type alias of shared_ptr char array.
 * You can choose the target type.
 * - If MQTT_STD_SHARED_PTR_ARRAY is defined, `std::shared_ptr<char []>` is used.
 *   - std::shared_ptr<char []> is supported since C++17.
 * - If MQTT_STD_SHARED_PTR_ARRAY is not defined (default), `boost::shared_ptr<char []>` is used.
 *   - `boost::shared_ptr<char []>` can be used on C++14.
 */
using shared_ptr_array = std::shared_ptr<char []>;

/**
 * @brief shared_ptr_array creating function.
 * You can choose the target type.
 * - If MQTT_STD_SHARED_PTR_ARRAY is defined,
 *   - and if your compiler setting is C++20 or later, then `std::make_shared<char[]>(size)` is used.
 *      - It can allocate an array of characters and the control block in a single allocation.
 *   - otherwise `std::shared_ptr<char[]>(new char[size])` is used.
 *      - It requires two times allocations.
 * - If MQTT_STD_SHARED_PTR_ARRAY is not defined (default), then `boost::make_shared<char[]>(size)` is used.
 *      - It can allocate an array of characters and the control block in a single allocation.
 */
inline shared_ptr_array make_shared_ptr_array(std::size_t size);

#else  // defined(_DOXYGEN_)

#ifdef MQTT_STD_SHARED_PTR_ARRAY

#include <memory>

namespace mqtt {

using shared_ptr_array = std::shared_ptr<char []>;

inline shared_ptr_array make_shared_ptr_array(std::size_t size) {
#if __cplusplus > 201703L // C++20 date is not determined yet
    return std::make_shared<char[]>(size);
#else  // __cplusplus > 201703L
    return std::shared_ptr<char[]>(new char[size]);
#endif // __cplusplus > 201703L
}

} // namespace mqtt

#else  // MQTT_STD_SHARED_PTR_ARRAY

#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace mqtt {

using shared_ptr_array = boost::shared_ptr<char []>;

inline shared_ptr_array make_shared_ptr_array(std::size_t size) {
    return boost::make_shared<char[]>(size);
}

} // namespace mqtt

#endif // MQTT_STD_SHARED_PTR_ARRAY

#endif // defined(_DOXYGEN_)

#endif // MQTT_SHARED_PTR_ARRAY_HPP

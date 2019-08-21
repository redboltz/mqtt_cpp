// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SHARED_ANY_HPP)
#define MQTT_SHARED_ANY_HPP

#include <memory>

#include <boost/type_erasure/any.hpp>

namespace mqtt {

namespace mpl = boost::mpl;

namespace detail {

template <typename Concept>
using shared_any_base = boost::type_erasure::any<Concept, boost::type_erasure::_self&>;

} // namespace detail

/**
 * @brief boost::type_erasure wrapper that supports shared_ptr
 *
 * ## Motivation
 *
 *   mqtt_cpp treats four different kind of sockets.
 *   They are tcp, tcp(tls), websocket, and websocket(tls).
 *   They are not copyable.
 *   They used to be a template argument of the class template mqtt::endpoint.
 *   It causes template bloat and as the result of that, compile times
 *   become longer.
 *   In order to avoid that, type erasure mechanism is required.
 *
 * ## Existing Solutions
 *
 *   There are three type erasure solutions.
 *   1. The first one is C++ intrinsic virtual function based polymorphism.
 *      This is not template friendly and derived classes need to inherit
 *      the base class. It is difficult to maintain.
 *   2. The second one is `std::variant` or `boost::variant`.
        - https://en.cppreference.com/w/cpp/utility/variant
 *      - They need to list all of actual types. In addition, they don't
 *        support natural member function calling syntax such as `a.foo()`.
 *        They require `visit()`.
 *   3. The third one is `boost::type_erasure::any`.
 *      - https://www.boost.org/doc/html/boost_typeerasure.html
 *      - It doesn't require inheritance. It supports natural member function
 *        calling syntax.
 *      - It doesn't support shared_ptr.
 *
 * ## shared_any
 *
 *    - shared_any is an expansion of `boost::type_erasure::any`.
 *    - shared_any supports shared_ptr. The lifetime of the object is held by
 *      this class as shared_ptr<void>. And pointee object's reference is
 *      passed to the base class `boost::type_erasure::any`.
 *    - shared_any isn't default constructible and re-assignable.
 *      Because the reference of the pointee object is held by base class
 *      `boost::type_erasure::any`.
 *      shared_any is copyable and movable. In copy case, the pointee object
 *      doesn't copy, just shared the reference and the reference count of
 *      the lifetime is incremented.
 *    - shared_any provides natural member function calling syntax the same
 *      as `boost::type_erasure::any`.
 *
 * ## Where is shared_any used?
 *
 *    shared_any is used by mqtt::socket in the file type_erased_socket.hpp.
 *    It creates type erased socket using `boost::type_erasure::any` mannar.
 *    mqtt::socket is a member variable of the class template mqtt::endpoint.
 *    So mqtt::endpoint no longer has four different kind of sockets.
 *    As the result of that the compile times are decreased.
 */
template <typename Concept>
class shared_any : public detail::shared_any_base<Concept> {
    using base_type = detail::shared_any_base<Concept>;
    std::shared_ptr<void> ownership_;
public:
    template <class U>
    shared_any(std::shared_ptr<U> p)
        : base_type(*p), ownership_(p) {}
};

} // namespace mqtt

#endif // MQTT_SHARED_ANY_HPP

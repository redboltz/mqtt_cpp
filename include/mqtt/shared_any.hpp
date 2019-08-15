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

template <typename Concept>
class shared_any : public boost::type_erasure::any<Concept, boost::type_erasure::_self&> {
    using base_type = boost::type_erasure::any<Concept, boost::type_erasure::_self&>;
    std::shared_ptr<void> ownership_;
public:
    template <class U>
    shared_any(std::shared_ptr<U> p)
        : base_type(*p), ownership_(p) {}
};

} // namespace mqtt

#endif // MQTT_SHARED_ANY_HPP

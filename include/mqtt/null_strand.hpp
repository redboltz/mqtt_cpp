// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_NULL_STRAND_HPP)
#define MQTT_NULL_STRAND_HPP

#include <boost/asio.hpp>

namespace mqtt {

namespace as = boost::asio;

struct null_strand {
    null_strand(as::io_service&){}
    template <typename Func>
    void post(Func const&f) {
        f();
    }
    template <typename Func>
    Func const& wrap(Func const&f) {
        return f;
    }
};

} // namespace mqtt

#endif // MQTT_NULL_STRAND_HPP

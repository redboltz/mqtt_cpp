// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TYPE_ERASED_SOCKET_HPP)
#define MQTT_TYPE_ERASED_SOCKET_HPP

#include <cstdlib>

#include <boost/config/workaround.hpp>
#include <boost/type_erasure/member.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

#include <mqtt/shared_any.hpp>

namespace mqtt {

namespace as = boost::asio;

BOOST_TYPE_ERASURE_MEMBER(has_async_read, async_read)
BOOST_TYPE_ERASURE_MEMBER(has_async_write, async_write)
BOOST_TYPE_ERASURE_MEMBER(has_write, write)
BOOST_TYPE_ERASURE_MEMBER(has_post, post)
BOOST_TYPE_ERASURE_MEMBER(has_lowest_layer, lowest_layer)
BOOST_TYPE_ERASURE_MEMBER(has_close, close)

using namespace boost::type_erasure;

/**
 * @brief type alias of the type erased socket
 * - mqtt::socket is a type erased socket.
 * - shared_ptr of any classes that have listed functions (or matching funtion template)
 *   can be used as the initializer of mqtt::socket.
 * - The class template endpoint uses mqtt::socket via listed interface.
 * - lowest_layer is provided for users to configure the socket (e.g. set delay, buffer size, etc)
 *
 */
using socket = shared_any<
    mpl::vector<
        destructible<>,
        has_async_read<void(as::mutable_buffer, std::function<void(boost::system::error_code const&, std::size_t)>)>,
        has_async_write<void(std::vector<as::const_buffer>, std::function<void(boost::system::error_code const&, std::size_t)>)>,
        has_write<std::size_t(std::vector<as::const_buffer>, boost::system::error_code&)>,
        has_post<void(std::function<void()>)>,
        has_lowest_layer<as::basic_socket<as::ip::tcp>&()>,
        has_close<void(boost::system::error_code&)>
    >
>;

} // namespace mqtt

#endif // MQTT_TYPE_ERASED_SOCKET_HPP

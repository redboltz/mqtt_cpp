// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TYPE_ERASED_SOCKET_HPP)
#define MQTT_TYPE_ERASED_SOCKET_HPP

#include <cstdlib>

#include <boost/asio.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/error_code.hpp>
#include <mqtt/any.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

class socket {
public:
    virtual ~socket() = default;
    virtual void async_read(as::mutable_buffer, std::function<void(error_code, std::size_t)>) = 0;
    virtual void async_write(std::vector<as::const_buffer>, std::function<void(error_code, std::size_t)>) = 0;
    virtual std::size_t write(std::vector<as::const_buffer>, boost::system::error_code&) = 0;
    virtual void post(std::function<void()>) = 0;
    virtual void dispatch(std::function<void()>) = 0;
    virtual void defer(std::function<void()>) = 0;
    virtual bool running_in_this_thread() const = 0;
    virtual as::ip::tcp::socket::lowest_layer_type& lowest_layer() = 0;
    virtual any native_handle() = 0;
    virtual void clean_shutdown_and_close(boost::system::error_code&) = 0;
    virtual void async_clean_shutdown_and_close(std::function<void(error_code)>) = 0;
    virtual void force_shutdown_and_close(boost::system::error_code&) = 0;
    virtual as::any_io_executor get_executor() = 0;
};

} // namespace MQTT_NS

#endif // MQTT_TYPE_ERASED_SOCKET_HPP

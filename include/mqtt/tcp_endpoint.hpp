// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TCP_ENDPOINT_HPP)
#define MQTT_TCP_ENDPOINT_HPP

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/type_erased_socket.hpp>
#include <mqtt/move.hpp>
#include <mqtt/attributes.hpp>
#include <mqtt/tls.hpp>
#include <mqtt/log.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Socket, typename Strand>
class tcp_endpoint : public socket {
public:
    template <typename... Args>
    explicit tcp_endpoint(as::io_context& ioc, Args&&... args)
        :tcp_(ioc, std::forward<Args>(args)...),
         strand_(ioc.get_executor())
    {}

    MQTT_ALWAYS_INLINE void async_read(
        as::mutable_buffer buffers,
        std::function<void(error_code, std::size_t)> handler
    ) override final {
        as::async_read(
            tcp_,
            force_move(buffers),
            as::bind_executor(
                strand_,
                force_move(handler)
            )
        );
    }

    MQTT_ALWAYS_INLINE void async_write(
        std::vector<as::const_buffer> buffers,
        std::function<void(error_code, std::size_t)> handler
    ) override final {
        as::async_write(
            tcp_,
            force_move(buffers),
            as::bind_executor(
                strand_,
                force_move(handler)
            )
        );
    }

    MQTT_ALWAYS_INLINE std::size_t write(
        std::vector<as::const_buffer> buffers,
        boost::system::error_code& ec
    ) override final {
        return as::write(tcp_,force_move(buffers), ec);
    }

    MQTT_ALWAYS_INLINE void post(std::function<void()> handler) override final {
        as::post(
            strand_,
            force_move(handler)
        );
    }

    MQTT_ALWAYS_INLINE void dispatch(std::function<void()> handler) override final {
        as::dispatch(
            strand_,
            force_move(handler)
        );
    }

    MQTT_ALWAYS_INLINE void defer(std::function<void()> handler) override final {
        as::defer(
            strand_,
            force_move(handler)
        );
    }

    MQTT_ALWAYS_INLINE bool running_in_this_thread() const override final {
        return strand_.running_in_this_thread();
    }

    MQTT_ALWAYS_INLINE as::ip::tcp::socket::lowest_layer_type& lowest_layer() override final {
        return tcp_.lowest_layer();
    }

    MQTT_ALWAYS_INLINE any native_handle() override final {
        return tcp_.native_handle();
    }

    MQTT_ALWAYS_INLINE void clean_shutdown_and_close(boost::system::error_code& ec) override final {
        shutdown_and_close_impl(tcp_, ec);
    }

    MQTT_ALWAYS_INLINE void async_clean_shutdown_and_close(std::function<void(error_code)> handler) override final {
        async_shutdown_and_close_impl(tcp_, force_move(handler));
    }

    MQTT_ALWAYS_INLINE void force_shutdown_and_close(boost::system::error_code& ec) override final {
        tcp_.lowest_layer().shutdown(as::ip::tcp::socket::shutdown_both, ec);
        tcp_.lowest_layer().close(ec);
    }

    MQTT_ALWAYS_INLINE as::any_io_executor get_executor() override final {
        return strand_;
    }

    auto& socket() { return tcp_; }
    auto const& socket() const { return tcp_; }

    template <typename... Args>
    void set_option(Args&& ... args) {
        tcp_.set_option(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_accept(Args&& ... args) {
        tcp_.async_accept(std::forward<Args>(args)...);
    }

#if defined(MQTT_USE_TLS)

    template <typename... Args>
    void handshake(Args&& ... args) {
        tcp_.handshake(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_handshake(Args&& ... args) {
        tcp_.async_handshake(std::forward<Args>(args)...);
    }

#endif // defined(MQTT_USE_TLS)

private:
    void shutdown_and_close_impl(as::basic_socket<boost::asio::ip::tcp>& s, boost::system::error_code& ec) {
        s.shutdown(as::ip::tcp::socket::shutdown_both, ec);
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "shutdown ec:"
            << ec.message();
        s.close(ec);
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "close ec:"
            << ec.message();
    }

    void async_shutdown_and_close_impl(as::basic_socket<boost::asio::ip::tcp>& s, std::function<void(error_code)> handler) {
        post(
            [this, &s, handler = force_move(handler)] () mutable {
                error_code ec;
                shutdown_and_close_impl(s, ec);
                force_move(handler)(ec);
            }
        );
    }

#if defined(MQTT_USE_TLS)
    void shutdown_and_close_impl(tls::stream<as::ip::tcp::socket>& s, boost::system::error_code& ec) {
        s.shutdown(ec);
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "shutdown ec:"
            << ec.message();
        shutdown_and_close_impl(lowest_layer(), ec);
    }
    void async_shutdown_and_close_impl(tls::stream<as::ip::tcp::socket>& s, std::function<void(error_code)> handler) {
        s.async_shutdown(
            as::bind_executor(
                strand_,
                [this, &s, handler = force_move(handler)] (error_code ec) mutable {
                    MQTT_LOG("mqtt_impl", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "shutdown ec:"
                        << ec.message();
                    shutdown_and_close_impl(s.lowest_layer(), ec);
                    force_move(handler)(ec);
                }
            )
        );
    }
#endif // defined(MQTT_USE_TLS)

private:
    Socket tcp_;
    Strand strand_;
};

} // namespace MQTT_NS

#endif // MQTT_TCP_ENDPOINT_HPP

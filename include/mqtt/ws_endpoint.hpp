// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_WS_ENDPOINT_HPP)
#define MQTT_WS_ENDPOINT_HPP

#include <boost/beast/websocket.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/bind_executor.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/type_erased_socket.hpp>
#include <mqtt/move.hpp>
#include <mqtt/attributes.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/error_code.hpp>
#include <mqtt/tls.hpp>
#include <mqtt/log.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Socket, typename Strand>
class ws_endpoint : public socket {
public:
    template <typename... Args>
    explicit ws_endpoint(as::io_context& ioc, Args&&... args)
        :ws_(ioc, std::forward<Args>(args)...),
         strand_(ioc.get_executor())
    {
        ws_.binary(true);
        ws_.set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type& req) {
                    req.set("Sec-WebSocket-Protocol", "mqtt");
                }
            )
        );
    }

    MQTT_ALWAYS_INLINE void async_read(
        as::mutable_buffer buffers,
        std::function<void(error_code, std::size_t)> handler
    ) override final {
        auto req_size = as::buffer_size(buffers);

        using beast_read_handler_t =
            std::function<void(error_code ec, std::shared_ptr<void>)>;

        std::shared_ptr<beast_read_handler_t> beast_read_handler;
        if (req_size <= buffer_.size()) {
            as::buffer_copy(buffers, buffer_.data(), req_size);
            buffer_.consume(req_size);
            handler(boost::system::errc::make_error_code(boost::system::errc::success), req_size);
            return;
        }

        beast_read_handler.reset(
            new beast_read_handler_t(
                [this, req_size, buffers, handler = force_move(handler)]
                (error_code ec, std::shared_ptr<void> const& v) mutable {
                    if (ec) {
                        force_move(handler)(ec, 0);
                        return;
                    }
                    if (!ws_.got_binary()) {
                        buffer_.consume(buffer_.size());
                        force_move(handler)
                            (boost::system::errc::make_error_code(boost::system::errc::bad_message), 0);
                        return;
                    }
                    if (req_size > buffer_.size()) {
                        auto beast_read_handler = std::static_pointer_cast<beast_read_handler_t>(v);
                        ws_.async_read(
                            buffer_,
                            as::bind_executor(
                                strand_,
                                [beast_read_handler]
                                (error_code ec, std::size_t) {
                                    (*beast_read_handler)(ec, beast_read_handler);
                                }
                            )
                        );
                        return;
                    }
                    as::buffer_copy(buffers, buffer_.data(), req_size);
                    buffer_.consume(req_size);
                    force_move(handler)(boost::system::errc::make_error_code(boost::system::errc::success), req_size);
                }
            )
        );
        ws_.async_read(
            buffer_,
            as::bind_executor(
                strand_,
                [beast_read_handler]
                (error_code ec, std::size_t) {
                    (*beast_read_handler)(ec, beast_read_handler);
                }
            )
        );
    }

    MQTT_ALWAYS_INLINE void async_write(
        std::vector<as::const_buffer> buffers,
        std::function<void(error_code, std::size_t)> handler
    ) override final {
        ws_.async_write(
            buffers,
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
        ws_.write(buffers, ec);
        return as::buffer_size(buffers);
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
        return boost::beast::get_lowest_layer(ws_);
    }

    MQTT_ALWAYS_INLINE any native_handle() override final {
        return next_layer().native_handle();
    }

    MQTT_ALWAYS_INLINE void clean_shutdown_and_close(boost::system::error_code& ec) override final {
        if (ws_.is_open()) {
            // WebSocket closing process
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "call beast close";
            ws_.close(boost::beast::websocket::close_code::normal, ec);
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "ws close ec:"
                << ec.message();
            if (!ec) {
                do {
                    boost::beast::flat_buffer buffer;
                    ws_.read(buffer, ec);
                } while (!ec);
                if (ec == boost::beast::websocket::error::closed) {
                    ec = boost::system::errc::make_error_code(boost::system::errc::success);
                }
                MQTT_LOG("mqtt_impl", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "ws read ec:"
                    << ec.message();
            }
        }
        shutdown_and_close_impl(next_layer(), ec);
    }

    MQTT_ALWAYS_INLINE void async_clean_shutdown_and_close(std::function<void(error_code)> handler) override final {
        if (ws_.is_open()) {
            // WebSocket closing process
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "call beast async_close";
            ws_.async_close(
                boost::beast::websocket::close_code::normal,
                as::bind_executor(
                    strand_,
                    [this, handler = force_move(handler)]
                    (error_code ec) mutable {
                        if (ec) {
                            MQTT_LOG("mqtt_impl", trace)
                                << MQTT_ADD_VALUE(address, this)
                                << "ws async_close ec:"
                                << ec.message();
                            async_shutdown_and_close_impl(next_layer(), force_move(handler));
                        }
                        else {
                            async_read_until_closed(force_move(handler));
                        }
                    }
                )
            );
        }
        else {
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "ws async_close already closed";
            async_shutdown_and_close_impl(next_layer(), force_move(handler));
        }
    }

    MQTT_ALWAYS_INLINE void force_shutdown_and_close(boost::system::error_code& ec) override final {
        lowest_layer().shutdown(as::ip::tcp::socket::shutdown_both, ec);
        lowest_layer().close(ec);
    }

    MQTT_ALWAYS_INLINE as::any_io_executor get_executor() override final {
        return strand_;
    }

    typename boost::beast::websocket::stream<Socket>::next_layer_type& next_layer() {
        return ws_.next_layer();
    }

    template <typename T>
    void set_option(T&& t) {
        ws_.set_option(std::forward<T>(t));
    }

    template <typename ConstBufferSequence, typename AcceptHandler>
    void async_accept(
        ConstBufferSequence const& buffers,
        AcceptHandler&& handler) {
        ws_.async_accept(buffers, std::forward<AcceptHandler>(handler));
    }

    template<typename ConstBufferSequence, typename ResponseDecorator, typename AcceptHandler>
    void async_accept_ex(
        ConstBufferSequence const& buffers,
        ResponseDecorator const& decorator,
        AcceptHandler&& handler) {
        ws_.async_accept_ex(buffers, decorator, std::forward<AcceptHandler>(handler));
    }

    template <typename... Args>
    void async_handshake(Args&& ... args) {
        ws_.async_handshake(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void handshake(Args&& ... args) {
        ws_.handshake(std::forward<Args>(args)...);
    }

    template <typename ConstBufferSequence>
    std::size_t write(
        ConstBufferSequence const& buffers) {
        ws_.write(buffers);
        return as::buffer_size(buffers);
    }

private:
    void async_read_until_closed(std::function<void(error_code)> handler) {
        auto buffer = std::make_shared<boost::beast::flat_buffer>();
        ws_.async_read(
            *buffer,
            as::bind_executor(
                strand_,
                [this, handler = force_move(handler)]
                (error_code ec, std::size_t) mutable {
                    if (ec) {
                        if (ec == boost::beast::websocket::error::closed) {
                            ec = boost::system::errc::make_error_code(boost::system::errc::success);
                        }
                        MQTT_LOG("mqtt_impl", trace)
                            << MQTT_ADD_VALUE(address, this)
                            << "ws async_read ec:"
                            << ec.message();
                        async_shutdown_and_close_impl(next_layer(), force_move(handler));
                    }
                    else {
                        async_read_until_closed(force_move(handler));
                    }
                }
            )
        );
    }

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
                [this, handler = force_move(handler)] (error_code ec) mutable {
                    MQTT_LOG("mqtt_impl", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "shutdown ec:"
                        << ec.message();
                    shutdown_and_close_impl(lowest_layer(), ec);
                    force_move(handler)(ec);
                }
            )
        );
    }
#endif // defined(MQTT_USE_TLS)

private:
    boost::beast::websocket::stream<Socket> ws_;
    boost::beast::flat_buffer buffer_;
    Strand strand_;
};

} // namespace MQTT_NS

#endif // MQTT_WS_ENDPOINT_HPP

// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TCP_ENDPOINT_HPP)
#define MQTT_TCP_ENDPOINT_HPP

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>

#if defined(MQTT_USE_TLS)
#include <boost/asio/ssl.hpp>
#endif // defined(MQTT_USE_TLS)

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Socket, typename Strand>
class tcp_endpoint {
public:
    template <typename... Args>
    tcp_endpoint(as::io_context& ioc, Args&&... args)
        :tcp_(ioc, std::forward<Args>(args)...),
         strand_(ioc) {
    }

    template <typename... Args>
    void close(Args&&... args) {
        tcp_.lowest_layer().close(std::forward<Args>(args)...);
    }

    as::io_context& get_io_context() {
        return tcp_.get_io_context();
    }

    Socket& socket() { return tcp_; }
    Socket const& socket() const { return tcp_; }

    typename Socket::lowest_layer_type& lowest_layer() {
        return tcp_.lowest_layer();
    }

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
    void async_handshake(Args&& ... args) {
        tcp_.async_handshake(std::forward<Args>(args)...);
    }
#endif // defined(MQTT_USE_TLS)

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read(
        MutableBufferSequence && buffers,
        ReadHandler&& handler) {
        as::async_read(
            tcp_,
            std::forward<MutableBufferSequence>(buffers),
            as::bind_executor(
                strand_,
                std::forward<ReadHandler>(handler)
            )
        );
    }

    template <typename... Args>
    std::size_t write(Args&& ... args) {
        return as::write(tcp_, std::forward<Args>(args)...);
    }

    template <typename ConstBufferSequence, typename WriteHandler>
    void async_write(
        ConstBufferSequence && buffers,
        WriteHandler&& handler) {
        as::async_write(
            tcp_,
            std::forward<ConstBufferSequence>(buffers),
            as::bind_executor(
                strand_,
                std::forward<WriteHandler>(handler)
            )
        );
    }

    template <typename PostHandler>
    void post(PostHandler&& handler) {
        as::post(
            strand_,
            std::forward<PostHandler>(handler)
        );
    }

private:
    Socket tcp_;
    Strand strand_;
};

template <typename Socket, typename Strand, typename MutableBufferSequence, typename ReadHandler>
inline void async_read(
    tcp_endpoint<Socket, Strand>& ep,
    MutableBufferSequence && buffers,
    ReadHandler&& handler) {
    ep.async_read(std::forward<MutableBufferSequence>(buffers), std::forward<ReadHandler>(handler));
}

template <typename Socket, typename Strand, typename ConstBufferSequence>
inline std::size_t write(
    tcp_endpoint<Socket, Strand>& ep,
    ConstBufferSequence && buffers) {
    return ep.write(std::forward<ConstBufferSequence>(buffers));
}

template <typename Socket, typename Strand, typename ConstBufferSequence>
inline std::size_t write(
    tcp_endpoint<Socket, Strand>& ep,
    ConstBufferSequence && buffers,
    boost::system::error_code& ec) {
    return ep.write(std::forward<ConstBufferSequence>(buffers), ec);
}

template <typename Socket, typename Strand, typename ConstBufferSequence, typename WriteHandler>
inline void async_write(
    tcp_endpoint<Socket, Strand>& ep,
    ConstBufferSequence && buffers,
    WriteHandler&& handler) {
    ep.async_write(std::forward<ConstBufferSequence>(buffers), std::forward<WriteHandler>(handler));
}

} // namespace MQTT_NS

#endif // MQTT_TCP_ENDPOINT_HPP

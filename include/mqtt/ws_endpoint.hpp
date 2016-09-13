// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_WS_ENDPOINT_HPP)
#define MQTT_WS_ENDPOINT_HPP

#include <beast/websocket.hpp>

#include <mqtt/endpoint.hpp>

namespace mqtt {

namespace as = boost::asio;

template <typename Socket>
class ws_endpoint {
public:
    ws_endpoint(as::io_service& ios)
        :ws_(ios) {
    }
    void close() {
    }
    as::io_service& get_io_service() {
        return ws_.get_io_service();
    }
    auto& lowest_layer() {
        return ws_.lowest_layer();
    }
    auto& next_layer() {
        return ws_.next_layer();
    }
private:
    beast::websocket::stream<Socket> ws_;
};

template <typename Socket, typename MutableBufferSequence>
inline std::size_t read(
    ws_endpoint<Socket>& ep,
    MutableBufferSequence const& buffers) {
    return 0;
}

template <typename Socket, typename MutableBufferSequence, typename ReadHandler>
inline void async_read(
    ws_endpoint<Socket>& ep,
    MutableBufferSequence const& buffers,
    ReadHandler handler) {
}

template <typename Socket, typename ConstBufferSequence>
inline std::size_t write(
    ws_endpoint<Socket>& ep,
    ConstBufferSequence const& buffers) {
    return 0;
}

template <typename Socket, typename ConstBufferSequence>
inline std::size_t write(
    ws_endpoint<Socket>& ep,
    ConstBufferSequence const& buffers,
    boost::system::error_code& ec) {
    return 0;
}

template <typename Socket, typename ConstBufferSequence, typename WriteHandler>
inline void async_write(
    ws_endpoint<Socket>& ep,
    ConstBufferSequence const& buffers,
    WriteHandler handler) {
}


} // namespace mqtt

#endif // MQTT_WS_ENDPOINT_HPP

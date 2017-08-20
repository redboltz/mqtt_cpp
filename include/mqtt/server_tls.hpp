// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <boost/asio.hpp>

#if !defined(MQTT_SERVER_TLS_HPP)
#define MQTT_SERVER_TLS_HPP

#include <boost/asio/ssl.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

#include <mqtt/endpoint.hpp>

namespace mqtt {

namespace as = boost::asio;

template <typename Strand = as::io_service::strand, typename Mutex = std::mutex, template<typename...> class LockGuard = std::lock_guard>
class server_tls {
public:
    using endpoint_t = endpoint<as::ip::tcp::socket, Strand, Mutex, LockGuard>;
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> const& ep)>;

    /**
     * @breif Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con,
        AcceptorConfig&& config)
        : ios_accept_(ios_accept),
          ios_con_(ios_con),
          acceptor_(ios_accept_, std::forward<AsioEndpoint>(ep)),
          close_request_(false) {
        config(acceptor_);
    }

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con)
        : server_tls(std::forward<AsioEndpoint>(ep), ios_accept, ios_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::io_service& ios,
        AcceptorConfig&& config)
        : server_tls(std::forward<AsioEndpoint>(ep), ios, ios, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::io_service& ios)
        : server_tls(std::forward<AsioEndpoint>(ep), ios, ios, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        renew_socket();
        do_accept();
    }

    void close() {
        close_request_ = true;
        acceptor_.close();
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = std::move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = std::move(h);
    }

private:
    void renew_socket() {
        socket_.reset(new as::ip::tcp::socket(ios_con_));
    }

    void do_accept() {
        if (close_request_) return;
        acceptor_.async_accept(
            *socket_,
            [this]
            (boost::system::error_code const& ec) {
                if (ec) {
                    if (h_error_) h_error_(ec);
                }
                else {
                    if (h_accept_) h_accept_(std::make_shared<endpoint_t>(std::move(socket_)));
                    renew_socket();
                    do_accept();
                }
            }
        );
    }

private:
    as::io_service& ios_accept_;
    as::io_service& ios_con_;
    as::ip::tcp::acceptor acceptor_;
    std::unique_ptr<as::ip::tcp::socket> socket_;
    bool close_request_;
    accept_handler h_accept_;
    error_handler h_error_;
};

} // namespace mqtt

#endif // MQTT_SERVER_TLS_HPP

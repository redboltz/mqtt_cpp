// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/variant.hpp> // should be top to configure variant limit

#include <memory>
#include <boost/asio.hpp>

#if !defined(MQTT_SERVER_HPP)
#define MQTT_SERVER_HPP

#if !defined(MQTT_NO_TLS)
#include <boost/asio/ssl.hpp>
#endif // !defined(MQTT_NO_TLS)

#include <mqtt/tcp_endpoint.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

#include <mqtt/endpoint.hpp>
#include <mqtt/null_strand.hpp>

namespace mqtt {

namespace as = boost::asio;

template <
    typename Strand = as::io_service::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server {
public:
    using socket_t = tcp_endpoint<as::ip::tcp::socket, Strand>;
    using endpoint_t = endpoint<Mutex, LockGuard, PacketIdBytes>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(endpoint_t& ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ios_accept_(ios_accept),
          ios_con_(ios_con),
          acceptor_(as::ip::tcp::acceptor(ios_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con)
        : server(std::forward<AsioEndpoint>(ep), ios_accept, ios_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_service& ios,
        AcceptorConfig&& config)
        : server(std::forward<AsioEndpoint>(ep), ios, ios, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server(
        AsioEndpoint&& ep,
        as::io_service& ios)
        : server(std::forward<AsioEndpoint>(ep), ios, ios, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ios_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                ios_accept_.post(
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept();
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
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

    /**
     * @brief Set MQTT protocol version
     * @param version accepting protocol version
     * If the specific version is set, only set version is accepted.
     * If the version is set to protocol_version::undetermined, all versions are accepted.
     * Initial value is protocol_version::undetermined.
     */
    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto socket = std::make_shared<socket_t>(ios_con_);
        acceptor_.value().async_accept(
            socket->lowest_layer(),
            [this, socket]
            (boost::system::error_code const& ec) {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec);
                    return;
                }
                auto sp = std::make_shared<endpoint_t>(std::move(socket), version_);
                if (h_accept_) h_accept_(*sp);
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_service& ios_accept_;
    as::io_service& ios_con_;
    mqtt::optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    mqtt::protocol_version version_ = protocol_version::undetermined;
};

#if !defined(MQTT_NO_TLS)

template <
    typename Strand = as::io_service::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls {
public:
    using socket_t = tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = endpoint<Mutex, LockGuard, PacketIdBytes>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(endpoint_t& ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios_accept,
        as::io_service& ios_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ios_accept_(ios_accept),
          ios_con_(ios_con),
          acceptor_(as::ip::tcp::acceptor(ios_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(std::move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios_accept,
        as::io_service& ios_con)
        : server_tls(std::forward<AsioEndpoint>(ep), std::move(ctx), ios_accept, ios_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios,
        AcceptorConfig&& config)
        : server_tls(std::forward<AsioEndpoint>(ep), std::move(ctx), ios, ios, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios)
        : server_tls(std::forward<AsioEndpoint>(ep), std::move(ctx), ios, ios, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ios_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                ios_accept_.post(
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept();
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
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

    /**
     * @brief Set MQTT protocol version
     * @param version accepting protocol version
     * If the specific version is set, only set version is accepted.
     * If the version is set to protocol_version::undetermined, all versions are accepted.
     * Initial value is protocol_version::undetermined.
     */
    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(boost::posix_time::time_duration timeout) {
        underlying_connect_timeout_ = std::move(timeout);
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto socket = std::make_shared<socket_t>(ios_con_, ctx_);
        acceptor_.value().async_accept(
            socket->lowest_layer(),
            [this, socket]
            (boost::system::error_code const& ec) {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::deadline_timer>(ios_con_);
                tim->expires_from_now(underlying_connect_timeout_);
                tim->async_wait(
                    [socket, tim, underlying_finished]
                    (boost::system::error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return;
                        boost::system::error_code close_ec;
                        socket->lowest_layer().close(close_ec);
                    }
                );
                socket->async_handshake(
                    as::ssl::stream_base::server,
                    [this, socket, tim, underlying_finished]
                    (boost::system::error_code ec) {
                        *underlying_finished = true;
                        tim->cancel();
                        if (ec) {
                            return;
                        }
                        auto sp = std::make_shared<endpoint_t>(std::move(socket), version_);
                        if (h_accept_) h_accept_(*sp);
                    }
                );
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_service& ios_accept_;
    as::io_service& ios_con_;
    mqtt::optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    as::ssl::context ctx_;
    mqtt::protocol_version version_ = protocol_version::undetermined;
    boost::posix_time::time_duration underlying_connect_timeout_ = boost::posix_time::seconds(10);
};

#endif // !defined(MQTT_NO_TLS)

#if defined(MQTT_USE_WS)

class set_subprotocols {
public:
    template <typename T>
    explicit
    set_subprotocols(T&& s)
        : s_(std::forward<T>(s)) {
    }
    template<bool isRequest, class Headers>
    void operator()(boost::beast::http::header<isRequest, Headers>& m) const {
        m.fields.replace("Sec-WebSocket-Protocol", s_);
    }
private:
    std::string s_;
};

template <
    typename Strand = as::io_service::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_ws {
public:
    using socket_t = ws_endpoint<as::ip::tcp::socket, Strand>;
    using endpoint_t = endpoint<Mutex, LockGuard, PacketIdBytes>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(endpoint_t& ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ios_accept_(ios_accept),
          ios_con_(ios_con),
          acceptor_(as::ip::tcp::acceptor(ios_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_ws(
        AsioEndpoint&& ep,
        as::io_service& ios_accept,
        as::io_service& ios_con)
        : server_ws(std::forward<AsioEndpoint>(ep), ios_accept, ios_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_service& ios,
        AcceptorConfig&& config)
        : server_ws(std::forward<AsioEndpoint>(ep), ios, ios, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_ws(
        AsioEndpoint&& ep,
        as::io_service& ios)
        : server_ws(std::forward<AsioEndpoint>(ep), ios, ios, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ios_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                ios_accept_.post(
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept();
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
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

    /**
     * @brief Set MQTT protocol version
     * @param version accepting protocol version
     * If the specific version is set, only set version is accepted.
     * If the version is set to protocol_version::undetermined, all versions are accepted.
     * Initial value is protocol_version::undetermined.
     */
    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(boost::posix_time::time_duration timeout) {
        underlying_connect_timeout_ = std::move(timeout);
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto socket = std::make_shared<socket_t>(ios_con_);
        acceptor_.value().async_accept(
            socket->next_layer(),
            [this, socket]
            (boost::system::error_code const& ec) {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::deadline_timer>(ios_con_);
                tim->expires_from_now(underlying_connect_timeout_);
                tim->async_wait(
                    [socket, tim, underlying_finished]
                    (boost::system::error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return;
                        boost::system::error_code close_ec;
                        socket->lowest_layer().close(close_ec);
                    }
                );

                auto sb = std::make_shared<boost::asio::streambuf>();
                auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                boost::beast::http::async_read(
                    socket->next_layer(),
                    *sb,
                    *request,
                    [this, socket, sb, request, tim, underlying_finished]
                    (boost::system::error_code const& ec, std::size_t) {
                        if (ec) {
                            *underlying_finished = true;
                            tim->cancel();
                            return;
                        }
                        if (!boost::beast::websocket::is_upgrade(*request)) {
                            *underlying_finished = true;
                            tim->cancel();
                            return;
                        }
                        socket->async_accept_ex(
                            *request,
                            [request]
                            (boost::beast::websocket::response_type& m) {
                                auto it = request->find("Sec-WebSocket-Protocol");
                                if (it != request->end()) {
                                    m.insert(it->name(), it->value());
                                }
                            },
                            [this, socket, tim, underlying_finished]
                            (boost::system::error_code const& ec) {
                                *underlying_finished = true;
                                tim->cancel();
                                if (ec) {
                                    return;
                                }
                                auto sp = std::make_shared<endpoint_t>(std::move(socket), version_);
                                if (h_accept_) h_accept_(*sp);
                            }
                        );
                    }
                );
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_service& ios_accept_;
    as::io_service& ios_con_;
    mqtt::optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    mqtt::protocol_version version_ = protocol_version::undetermined;
    boost::posix_time::time_duration underlying_connect_timeout_ = boost::posix_time::seconds(10);
};


#if !defined(MQTT_NO_TLS)

template <
    typename Strand = as::io_service::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls_ws {
public:
    using socket_t = mqtt::ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = endpoint<Mutex, LockGuard, PacketIdBytes>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(endpoint_t& ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios_accept,
        as::io_service& ios_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ios_accept_(ios_accept),
          ios_con_(ios_con),
          acceptor_(as::ip::tcp::acceptor(ios_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(std::move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios_accept,
        as::io_service& ios_con)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), std::move(ctx), ios_accept, ios_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios,
        AcceptorConfig&& config)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), std::move(ctx), ios, ios, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_service& ios)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), std::move(ctx), ios, ios, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ios_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                ios_accept_.post(
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept();
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
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

    /**
     * @brief Set MQTT protocol version
     * @param version accepting protocol version
     * If the specific version is set, only set version is accepted.
     * If the version is set to protocol_version::undetermined, all versions are accepted.
     * Initial value is protocol_version::undetermined.
     */
    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(boost::posix_time::time_duration timeout) {
        underlying_connect_timeout_ = std::move(timeout);
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto socket = std::make_shared<socket_t>(ios_con_, ctx_);
        acceptor_.value().async_accept(
            socket->next_layer().next_layer(),
            [this, socket]
            (boost::system::error_code const& ec) {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::deadline_timer>(ios_con_);
                tim->expires_from_now(underlying_connect_timeout_);
                tim->async_wait(
                    [socket, tim, underlying_finished]
                    (boost::system::error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return;
                        boost::system::error_code close_ec;
                        socket->lowest_layer().close(close_ec);
                    }
                );
                socket->next_layer().async_handshake(
                    as::ssl::stream_base::server,
                    [this, socket, tim, underlying_finished]
                    (boost::system::error_code ec) {
                        if (ec) {
                            *underlying_finished = true;
                            tim->cancel();
                            return;
                        }
                        auto sb = std::make_shared<boost::asio::streambuf>();
                        auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                        boost::beast::http::async_read(
                            socket->next_layer(),
                            *sb,
                            *request,
                            [this, socket, sb, request, tim, underlying_finished]
                            (boost::system::error_code const& ec, std::size_t) {
                                if (ec) {
                                    *underlying_finished = true;
                                    tim->cancel();
                                    return;
                                }
                                if (!boost::beast::websocket::is_upgrade(*request)) {
                                    *underlying_finished = true;
                                    tim->cancel();
                                    return;
                                }
                                socket->async_accept_ex(
                                    *request,
                                    [request]
                                    (boost::beast::websocket::response_type& m) {
                                        auto it = request->find("Sec-WebSocket-Protocol");
                                        if (it != request->end()) {
                                            m.insert(it->name(), it->value());
                                        }
                                    },
                                    [this, socket, tim, underlying_finished]
                                    (boost::system::error_code const& ec) {
                                        *underlying_finished = true;
                                        tim->cancel();
                                        if (ec) {
                                            return;
                                        }
                                        auto sp = std::make_shared<endpoint_t>(std::move(socket), version_);
                                        if (h_accept_) h_accept_(*sp);
                                    }
                                );
                            }
                        );
                    }
                );
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_service& ios_accept_;
    as::io_service& ios_con_;
    mqtt::optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    as::ssl::context ctx_;
    mqtt::protocol_version version_ = protocol_version::undetermined;
    boost::posix_time::time_duration underlying_connect_timeout_ = boost::posix_time::seconds(10);
};

#endif // !defined(MQTT_NO_TLS)

#endif // defined(MQTT_USE_WS)

} // namespace mqtt

#endif // MQTT_SERVER_HPP

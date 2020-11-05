// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SERVER_HPP)
#define MQTT_SERVER_HPP

#include <mqtt/variant.hpp> // should be top to configure variant limit

#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>

#include <mqtt/namespace.hpp>

#if defined(MQTT_USE_TLS)
#include <boost/asio/ssl.hpp>
#endif // defined(MQTT_USE_TLS)

#include <mqtt/tcp_endpoint.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

#include <mqtt/endpoint.hpp>
#include <mqtt/null_strand.hpp>
#include <mqtt/move.hpp>
#include <mqtt/callable_overlay.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Mutex, template<typename...> class LockGuard, std::size_t PacketIdBytes>
class server_endpoint : public endpoint<Mutex, LockGuard, PacketIdBytes> {
public:
    using endpoint<Mutex, LockGuard, PacketIdBytes>::endpoint;
protected:
    bool on_v5_connack(bool, v5::connect_reason_code, v5::properties) noexcept override { return true; }
    void on_pre_send() noexcept override {}
    void on_close() noexcept override {}
    void on_error(error_code /*ec*/) noexcept override {}
protected:
    ~server_endpoint() = default;
};

template <
    typename Strand = as::io_context::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server {
public:
    using socket_t = tcp_endpoint<as::ip::tcp::socket, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(ioc_con),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server(std::forward<AsioEndpoint>(ep), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server(std::forward<AsioEndpoint>(ep), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc)
        : server(std::forward<AsioEndpoint>(ep), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ioc_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                as::post(
                    ioc_accept_,
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept([this](error_code ec, std::shared_ptr<endpoint_t> sp) {
                if(ec) {
                    if (h_error_) h_error_(ec);
                        return;
                }

                if (h_accept_) h_accept_(force_move(sp));
            }
        );
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = force_move(h);
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
     * @brief Get reference of boost::asio::io_context for connections
     * @return reference of boost::asio::io_context for connections
     */
    as::io_context& ioc_con() const {
        return ioc_con_;
    }

    /**
     * @brief Get reference of boost::asio::io_context for acceptor
     * @return reference of boost::asio::io_context for acceptor
     */
    as::io_context& ioc_accept() const {
        return ioc_accept_;
    }

private:
    template<typename CompletionHandler>
    auto do_accept(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {
        return as::async_compose<CompletionHandler, void(boost::system::error_code, std::shared_ptr<endpoint_t>)> (
                   [this,
                    coro = as::coroutine(),
                    socket = std::shared_ptr<socket_t>{}]
                   (auto &self, boost::system::error_code ec = {}) mutable
                {
                    BOOST_ASIO_CORO_REENTER(coro)
                    {
                        if (close_request_) return self.complete(ec, std::shared_ptr<endpoint_t>{});

                        socket = std::make_shared<socket_t>(ioc_con_);
                        BOOST_ASIO_CORO_YIELD
                            acceptor_.value().async_accept(
                                socket->lowest_layer(),
                                force_move(self));
                        if (ec) {
                            this->acceptor_.reset();
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }
                    }
                    return self.complete(ec, std::make_shared<endpoint_t>(ioc_con_, force_move(socket), version_));
                },
                handler, ioc_con_);
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context& ioc_con_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    protocol_version version_ = protocol_version::undetermined;
};

#if defined(MQTT_USE_TLS)

template <
    typename Strand = as::io_context::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls {
public:
    using socket_t = tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(ioc_con),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ioc_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                as::post(
                    ioc_accept_,
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept([this](error_code ec, std::shared_ptr<endpoint_t> sp) {
                if(ec) {
                    if (h_error_) h_error_(ec);
                    return;
                }

                if (h_accept_) h_accept_(force_move(sp));
            }
        );
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = force_move(h);
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
     * @brief Get reference of boost::asio::io_context for connections
     * @return reference of boost::asio::io_context for connections
     */
    as::io_context& ioc_con() const {
        return ioc_con_;
    }

    /**
     * @brief Get reference of boost::asio::io_context for acceptor
     * @return reference of boost::asio::io_context for acceptor
     */
    as::io_context& ioc_accept() const {
        return ioc_accept_;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context& get_ssl_context() {
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context const& get_ssl_context() const {
        return ctx_;
    }

private:
    template<typename CompletionHandler>
    auto do_accept(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {
        return as::async_compose<CompletionHandler, void(boost::system::error_code, std::shared_ptr<endpoint_t>)> (
                   [this,
                    coro = as::coroutine(),
                    underlying_finished = false,
                    tim = as::steady_timer{ioc_con_, underlying_connect_timeout_},
                    socket = std::shared_ptr<socket_t>{}]
                   (auto &self, boost::system::error_code ec = {}) mutable
                {
                    auto pGuard = shared_scope_guard([&](void)
                    {
                        underlying_finished = true;
                        tim.cancel();
                    });
                    BOOST_ASIO_CORO_REENTER(coro)
                    {
                        if (close_request_) return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        tim.async_wait([&](error_code ec) {
                                if (ec) return;
                                if (underlying_finished) return;
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                            }
                        );

                        socket = std::make_shared<socket_t>(ioc_con_, ctx_);

                        BOOST_ASIO_CORO_YIELD
                            acceptor_.value().async_accept(
                                socket->lowest_layer(),
                                force_move(self));
                        if (ec) {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

                        BOOST_ASIO_CORO_YIELD
                            socket->async_handshake(
                                as::ssl::stream_base::server,
                                force_move(self));
                        if (ec) {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }
                    }
                    return self.complete(ec, std::make_shared<endpoint_t>(ioc_con_, force_move(socket), version_));
                },
                handler, ioc_con_);
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context& ioc_con_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    as::ssl::context ctx_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};

#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)

template <
    typename Strand = as::io_context::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_ws {
public:
    using socket_t = ws_endpoint<as::ip::tcp::socket, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(ioc_con),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server_ws(std::forward<AsioEndpoint>(ep), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server_ws(std::forward<AsioEndpoint>(ep), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc)
        : server_ws(std::forward<AsioEndpoint>(ep), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ioc_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                as::post(
                    ioc_accept_,
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept([this](error_code ec, std::shared_ptr<endpoint_t> sp) {
                if(ec) {
                    if (h_error_) h_error_(ec);
                    return;
                }

                if (h_accept_) h_accept_(force_move(sp));
            }
        );
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = force_move(h);
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
     * @brief Get reference of boost::asio::io_context for connections
     * @return reference of boost::asio::io_context for connections
     */
    as::io_context& ioc_con() const {
        return ioc_con_;
    }

    /**
     * @brief Get reference of boost::asio::io_context for acceptor
     * @return reference of boost::asio::io_context for acceptor
     */
    as::io_context& ioc_accept() const {
        return ioc_accept_;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

private:
    template<typename CompletionHandler>
    auto do_accept(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {
        return as::async_compose<CompletionHandler, void(boost::system::error_code, std::shared_ptr<endpoint_t>)> (
                   [this,
                    coro = as::coroutine(),
                    underlying_finished = false,
                    tim = as::steady_timer{ioc_con_, underlying_connect_timeout_},
                    socket = std::shared_ptr<socket_t>{},
                    sb = std::shared_ptr<boost::asio::streambuf>{},
                    request = std::shared_ptr<boost::beast::http::request<boost::beast::http::string_body>>{}]
                   (auto &self, boost::system::error_code ec = {}, size_t = 0) mutable
                {
                    auto pGuard = shared_scope_guard([&](void) {
                            underlying_finished = true;
                            tim.cancel();
                        }
                    );

                    BOOST_ASIO_CORO_REENTER(coro)
                    {
                        if (close_request_) return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        tim.async_wait([&](error_code ec) {
                                if (ec) return;
                                if (underlying_finished) return;
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                            }
                        );

                        socket = std::make_shared<socket_t>(ioc_con_);
                        BOOST_ASIO_CORO_YIELD
                            acceptor_.value().async_accept(
                                socket->lowest_layer(),
                                force_move(self));
                        if (ec) {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

                        sb = std::make_shared<boost::asio::streambuf>();
                        request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                        BOOST_ASIO_CORO_YIELD
                            boost::beast::http::async_read(
                                socket->next_layer(),
                                *sb,
                                *request,
                                force_move(self));
                        if(ec || ! boost::beast::websocket::is_upgrade(*request))
                        {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

#if BOOST_BEAST_VERSION >= 248

                        auto it = request->find("Sec-WebSocket-Protocol");
                        if (it != request->end()) {
                            socket->set_option(
                                boost::beast::websocket::stream_base::decorator(
                                    [name = it->name(), value = it->value()] // name is enum, value is boost::string_view
                                    (boost::beast::websocket::response_type& res) {
                                        res.set(name, value);
                                    }
                                )
                            );
                        }
                        BOOST_ASIO_CORO_YIELD
                            socket->async_accept(*request, force_move(self));

#else  // BOOST_BEAST_VERSION >= 248

                        BOOST_ASIO_CORO_YIELD
                            socket->async_accept_ex(
                                *request,
                                [&]
                                (boost::beast::websocket::response_type& m) {
                                    auto it = request->find("Sec-WebSocket-Protocol");
                                    if (it != request->end()) {
                                        m.insert(it->name(), it->value());
                                    }
                                },
                                force_move(self));

#endif // BOOST_BEAST_VERSION >= 248

                         if (ec) {
                             return self.complete(ec, std::shared_ptr<endpoint_t>{});
                         }
                    }
                    return self.complete(ec, std::make_shared<endpoint_t>(ioc_con_, force_move(socket), version_));
                },
                handler, ioc_con_);
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context& ioc_con_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};


#if defined(MQTT_USE_TLS)

template <
    typename Strand = as::io_context::strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls_ws {
public:
    using socket_t = ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(ioc_con),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        as::ssl::context&& ctx,
        as::io_context& ioc)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    void listen() {
        close_request_ = false;

        if (!acceptor_) {
            try {
                acceptor_.emplace(ioc_accept_, ep_);
                config_(acceptor_.value());
            }
            catch (boost::system::system_error const& e) {
                as::post(
                    ioc_accept_,
                    [this, ec = e.code()] {
                        if (h_error_) h_error_(ec);
                    }
                );
                return;
            }
        }
        do_accept([this](error_code ec, std::shared_ptr<endpoint_t> sp) {
                if(ec) {
                    if (h_error_) h_error_(ec);
                    return;
                }

                if (h_accept_) h_accept_(force_move(sp));
        });
    }

    unsigned short port() const { return acceptor_.value().local_endpoint().port(); }

    void close() {
        close_request_ = true;
        acceptor_.reset();
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = force_move(h);
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
     * @brief Get reference of boost::asio::io_context for connections
     * @return reference of boost::asio::io_context for connections
     */
    as::io_context& ioc_con() const {
        return ioc_con_;
    }

    /**
     * @brief Get reference of boost::asio::io_context for acceptor
     * @return reference of boost::asio::io_context for acceptor
     */
    as::io_context& ioc_accept() const {
        return ioc_accept_;
    }

    /**
     * @bried Set underlying layer connection timeout.
     * The timer is set after TCP layer connection accepted.
     * The timer is cancelled just before accept handler is called.
     * If the timer is fired, the endpoint is removed, the socket is automatically closed.
     * The default timeout value is 10 seconds.
     * @param timeout timeout value
     */
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context& get_ssl_context() {
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context const& get_ssl_context() const {
        return ctx_;
    }

private:
    template<typename CompletionHandler>
    auto do_accept(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {
        return as::async_compose<CompletionHandler, void(boost::system::error_code, std::shared_ptr<endpoint_t>)> (
                   [this,
                    coro = as::coroutine(),
                    underlying_finished = false,
                    tim = as::steady_timer{ioc_con_, underlying_connect_timeout_},
                    socket = std::shared_ptr<socket_t>{},
                    sb = std::shared_ptr<boost::asio::streambuf>{},
                    request = std::shared_ptr<boost::beast::http::request<boost::beast::http::string_body>>{}]
                   (auto &self, boost::system::error_code ec = {}, size_t = 0) mutable
                {
                    auto pGuard = shared_scope_guard([&](void) {
                            underlying_finished = true;
                            tim.cancel();
                        }
                    );

                    BOOST_ASIO_CORO_REENTER(coro)
                    {
                        if (close_request_) return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        tim.async_wait([&](error_code ec) {
                                if (underlying_finished) return;
                                if (ec) return;
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                            }
                        );

                        socket = std::make_shared<socket_t>(ioc_con_, ctx_);

                        BOOST_ASIO_CORO_YIELD
                            acceptor_.value().async_accept(
                                socket->lowest_layer(),
                                force_move(self));
                        if (ec) {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

                        BOOST_ASIO_CORO_YIELD
                            socket->next_layer().async_handshake(
                                as::ssl::stream_base::server,
                                force_move(self));
                        if (ec) {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

                        sb = std::make_shared<boost::asio::streambuf>();
                        request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();

                        BOOST_ASIO_CORO_YIELD
                            boost::beast::http::async_read(
                                socket->next_layer(),
                                *sb,
                                *request,
                                force_move(self));
                        if(ec || ! boost::beast::websocket::is_upgrade(*request))
                        {
                            return self.complete(ec, std::shared_ptr<endpoint_t>{});
                        }

#if BOOST_BEAST_VERSION >= 248

                        auto it = request->find("Sec-WebSocket-Protocol");
                        if (it != request->end()) {
                            socket->set_option(
                                boost::beast::websocket::stream_base::decorator(
                                    [name = it->name(), value = it->value()] // name is enum, value is boost::string_view
                                    (boost::beast::websocket::response_type& res) {
                                        res.set(name, value);
                                    }
                                )
                            );
                        }

                        BOOST_ASIO_CORO_YIELD
                            socket->async_accept(*request, force_move(self));

#else  // BOOST_BEAST_VERSION >= 248

                        BOOST_ASIO_CORO_YIELD
                            socket->async_accept_ex(
                                    *request,
                                    [&]
                                    (boost::beast::websocket::response_type& m) {
                                        auto it = request->find("Sec-WebSocket-Protocol");
                                        if (it != request->end()) {
                                            m.insert(it->name(), it->value());
                                        }
                                    },
                                    force_move(self));

#endif // BOOST_BEAST_VERSION >= 248

                         if (ec) {
                             return self.complete(ec, std::shared_ptr<endpoint_t>{});
                         }
                    }
                    return self.complete(ec, std::make_shared<endpoint_t>(ioc_con_, force_move(socket), version_));
                },
                handler, ioc_con_);
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context& ioc_con_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    error_handler h_error_;
    as::ssl::context ctx_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};

#endif // defined(MQTT_USE_TLS)

#endif // defined(MQTT_USE_WS)

} // namespace MQTT_NS

#endif // MQTT_SERVER_HPP

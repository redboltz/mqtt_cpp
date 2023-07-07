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

#include <mqtt/namespace.hpp>
#include <mqtt/tcp_endpoint.hpp>

#include <mqtt/endpoint.hpp>
#include <mqtt/move.hpp>
#include <mqtt/callable_overlay.hpp>
#include <mqtt/strand.hpp>
#include <mqtt/null_strand.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Mutex, template<typename...> class LockGuard, std::size_t PacketIdBytes>
class server_endpoint : public endpoint<Mutex, LockGuard, PacketIdBytes> {
public:
    using endpoint<Mutex, LockGuard, PacketIdBytes>::endpoint;
protected:
    void on_pre_send() noexcept override {}
    void on_close() noexcept override {}
    void on_error(error_code /*ec*/) noexcept override {}
protected:
    ~server_endpoint() = default;
};

template <
    typename Strand = strand,
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
     *        After this handler called, the next accept will automatically start.
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler during after accepted before connection established
     *        After this handler called, the next accept will automatically start.
     * @param ec error code
     * @param ioc_con io_context for incoming connection
     */
    using connection_error_handler = std::function<void(error_code ec, as::io_context& ioc_con)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     * @param ioc_con io_context for listen or accept
     */
    using error_handler_with_ioc = std::function<void(error_code ec, as::io_context& ioc_accept)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(&ioc_con),
          ioc_con_getter_([this]() -> as::io_context& { return *ioc_con_; }),
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

    template <typename AsioEndpoint, typename AcceptorConfig>
    server(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        AcceptorConfig&& config = [](as::ip::tcp::acceptor&) {})
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_getter_(force_move(ioc_con_getter)),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }

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
                        if (h_error_) h_error_(ec, ioc_accept_);
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
        as::post(
            ioc_accept_,
            [this] {
                acceptor_.reset();
            }
        );
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler h) {
        h_error_ =
            [h = force_move(h)]
            (error_code ec, as::io_context&) {
                if (h) h(ec);
            };
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler_with_ioc h = error_handler_with_ioc()) {
        h_error_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_connection_error_handler(connection_error_handler h = connection_error_handler()) {
        h_connection_error_ = force_move(h);
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
        auto& ioc_con = ioc_con_getter_();
        auto socket = std::make_shared<socket_t>(ioc_con);
        acceptor_.value().async_accept(
            socket->lowest_layer(),
            [this, socket, &ioc_con]
            (error_code ec) mutable {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec, ioc_con);
                    return;
                }
                auto sp = std::make_shared<endpoint_t>(ioc_con, force_move(socket), version_);
                if (h_accept_) h_accept_(force_move(sp));
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context* ioc_con_ = nullptr;
    std::function<as::io_context&()> ioc_con_getter_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    connection_error_handler h_connection_error_;
    error_handler_with_ioc h_error_;
    protocol_version version_ = protocol_version::undetermined;
};

#if defined(MQTT_USE_TLS)

template <
    typename Strand = strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls {
public:
    using socket_t = tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     *        After this handler called, the next accept will automatically start.
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler during after accepted before connection established
     *        After this handler called, the next accept will automatically start.
     * @param ec error code
     * @param ioc_con io_context for incoming connection
     */
    using connection_error_handler = std::function<void(error_code ec, as::io_context& ioc_con)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     * @param ioc_con io_context for listen or accept
     */
    using error_handler_with_ioc = std::function<void(error_code ec, as::io_context& ioc_accept)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(&ioc_con),
          ioc_con_getter_([this]() -> as::io_context& { return *ioc_con_; }),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc)
        : server_tls(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        AcceptorConfig&& config = [](as::ip::tcp::acceptor&) {})
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_getter_(force_move(ioc_con_getter)),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

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
                        if (h_error_) h_error_(ec, ioc_accept_);
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
        as::post(
            ioc_accept_,
            [this] {
                acceptor_.reset();
            }
        );
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler h) {
        h_error_ =
            [h = force_move(h)]
            (error_code ec, as::io_context&) {
                if (h) h(ec);
            };
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler_with_ioc h = error_handler_with_ioc()) {
        h_error_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_connection_error_handler(connection_error_handler h = connection_error_handler()) {
        h_connection_error_ = force_move(h);
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
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context& get_ssl_context() {
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context const& get_ssl_context() const {
        return ctx_;
    }

    using verify_cb_t = std::function<bool (bool, boost::asio::ssl::verify_context&, std::shared_ptr<optional<std::string>> const&) >;

    void set_verify_callback(verify_cb_t verify_cb) {
        verify_cb_with_username_ = verify_cb;
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto& ioc_con = ioc_con_getter_();
        auto username = std::make_shared<optional<std::string>>(); // shared_ptr for username
        auto verify_cb_ = [this, username] // copy capture socket shared_ptr
            (bool preverified, boost::asio::ssl::verify_context& ctx) {
                // user can set username in the callback
                return verify_cb_with_username_
                    ? verify_cb_with_username_(preverified, ctx, username)
                    : false;
        };

        ctx_.set_verify_mode(MQTT_NS::tls::verify_peer);
        ctx_.set_verify_callback(verify_cb_);
        auto socket = std::make_shared<socket_t>(ioc_con, ctx_);

        auto ps = socket.get();
        acceptor_.value().async_accept(
            ps->lowest_layer(),
            [this, socket = force_move(socket), &ioc_con, username]
            (error_code ec) mutable {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec, ioc_con);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto connection_error_called = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::steady_timer>(ioc_con);
                tim->expires_after(underlying_connect_timeout_);
                tim->async_wait(
                    [
                        this,
                        socket,
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con
                    ]
                    (error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return; // timer cancelled
                        socket->post(
                            [this, socket, connection_error_called, &ioc_con] {
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                                if (h_connection_error_ && !*connection_error_called) {
                                    h_connection_error_(
                                        boost::system::errc::make_error_code(
                                            boost::system::errc::stream_timeout
                                        ),
                                        ioc_con
                                    );
                                    *connection_error_called = true;
                                }
                            }
                        );
                    }
                );
                auto ps = socket.get();

                ps->async_handshake(
                    tls::stream_base::server,
                    [
                        this,
                        socket = force_move(socket),
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con,
                        username
                    ]
                    (error_code ec) mutable {
                        *underlying_finished = true;
                        tim->cancel();
                        if (ec) {
                            if (h_connection_error_ && !*connection_error_called) {
                                h_connection_error_(ec, ioc_con);
                                *connection_error_called = true;
                            }
                            do_accept();
                            return;
                        }
                        auto sp = std::make_shared<endpoint_t>(ioc_con, force_move(socket), version_);
                        sp->set_preauthed_user_name(*username);
                        if (h_accept_) h_accept_(force_move(sp));
                        do_accept();
                    }
                );
            }
        );
    }

private:
    verify_cb_t verify_cb_with_username_;

    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context* ioc_con_ = nullptr;
    std::function<as::io_context&()> ioc_con_getter_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    connection_error_handler h_connection_error_;
    error_handler_with_ioc h_error_;
    tls::context ctx_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};

#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)

template <
    typename Strand = strand,
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
     * @brief Error handler during after accepted before connection established
     *        After this handler called, the next accept will automatically start.
     * @param ec error code
     * @param ioc_con io_context for incoming connection
     */
    using connection_error_handler = std::function<void(error_code ec, as::io_context& ioc_con)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     * @param ioc_con io_context for listen or accept
     */
    using error_handler_with_ioc = std::function<void(error_code ec, as::io_context& ioc_accept)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(&ioc_con),
          ioc_con_getter_([this]() -> as::io_context& { return *ioc_con_; }),
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

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_ws(
        AsioEndpoint&& ep,
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        AcceptorConfig&& config = [](as::ip::tcp::acceptor&) {})
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_getter_(force_move(ioc_con_getter)),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)) {
        config_(acceptor_.value());
    }
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
                        if (h_error_) h_error_(ec, ioc_accept_);
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
        as::post(
            ioc_accept_,
            [this] {
                acceptor_.reset();
            }
        );
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler h) {
        h_error_ =
            [h = force_move(h)]
            (error_code ec, as::io_context&) {
                if (h) h(ec);
            };
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler_with_ioc h = error_handler_with_ioc()) {
        h_error_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_connection_error_handler(connection_error_handler h = connection_error_handler()) {
        h_connection_error_ = force_move(h);
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
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto& ioc_con = ioc_con_getter_();
        auto socket = std::make_shared<socket_t>(ioc_con);
        auto ps = socket.get();
        acceptor_.value().async_accept(
            ps->next_layer(),
            [this, socket = force_move(socket), &ioc_con]
            (error_code ec) mutable {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec, ioc_con);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto connection_error_called = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::steady_timer>(ioc_con);
                tim->expires_after(underlying_connect_timeout_);
                tim->async_wait(
                    [
                        this,
                        socket,
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con
                    ]
                    (error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return; // timer cancelled
                        socket->post(
                            [this, socket, connection_error_called, &ioc_con] {
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                                if (h_connection_error_ && !*connection_error_called) {
                                    h_connection_error_(
                                        boost::system::errc::make_error_code(
                                            boost::system::errc::stream_timeout
                                        ),
                                        ioc_con
                                    );
                                    *connection_error_called = true;
                                }
                            }
                        );
                    }
                );

                auto sb = std::make_shared<boost::asio::streambuf>();
                auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                auto ps = socket.get();
                boost::beast::http::async_read(
                    ps->next_layer(),
                    *sb,
                    *request,
                    [
                        this,
                        socket = force_move(socket),
                        sb,
                        request,
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con
                    ]
                    (error_code ec, std::size_t) mutable {
                        if (ec) {
                            *underlying_finished = true;
                            tim->cancel();
                            if (h_connection_error_ && !*connection_error_called) {
                                h_connection_error_(ec, ioc_con);
                                *connection_error_called = true;
                            }
                            return;
                        }
                        if (!boost::beast::websocket::is_upgrade(*request)) {
                            *underlying_finished = true;
                            tim->cancel();
                            if (h_connection_error_ && !*connection_error_called) {
                                h_connection_error_(
                                    boost::system::errc::make_error_code(
                                        boost::system::errc::protocol_error
                                    ),
                                    ioc_con
                                );
                                *connection_error_called = true;
                            }
                            return;
                        }
                        auto ps = socket.get();

#if BOOST_BEAST_VERSION >= 248

                        auto it = request->find("Sec-WebSocket-Protocol");
                        if (it != request->end()) {
                            ps->set_option(
                                boost::beast::websocket::stream_base::decorator(
                                    [name = it->name(), value = it->value()] // name is enum, value is boost::string_view
                                    (boost::beast::websocket::response_type& res) {
                                        // This lambda is called before the scope out point *1
                                        res.set(name, value);
                                    }
                                )
                            );
                        }
                        ps->async_accept(
                            *request,
                            [
                                this,
                                socket = force_move(socket),
                                tim,
                                underlying_finished,
                                connection_error_called,
                                &ioc_con
                            ]
                            (error_code ec) mutable {
                                *underlying_finished = true;
                                tim->cancel();
                                if (ec) {
                                    if (h_connection_error_ && !*connection_error_called) {
                                        h_connection_error_(ec, ioc_con);
                                    }
                                    *connection_error_called = true;
                                    return;
                                }
                                auto sp = std::make_shared<endpoint_t>(ioc_con, force_move(socket), version_);
                                if (h_accept_) h_accept_(force_move(sp));
                            }
                        );

#else  // BOOST_BEAST_VERSION >= 248

                        ps->async_accept_ex(
                            *request,
                            [request, connection_error_called]
                            (boost::beast::websocket::response_type& m) {
                                auto it = request->find("Sec-WebSocket-Protocol");
                                if (it != request->end()) {
                                    m.insert(it->name(), it->value());
                                }
                            },
                            [this, socket = force_move(socket), tim, underlying_finished, &ioc_con]
                            (error_code ec) mutable {
                                *underlying_finished = true;
                                tim->cancel();
                                if (ec) {
                                    if (h_connection_error_ && !*connection_error_called) {
                                        h_connection_error_(ec, ioc_con);
                                        *connection_error_called = true;
                                    }
                                    return;
                                }
                                auto sp = std::make_shared<endpoint_t>(ioc_con, force_move(socket), version_);
                                if (h_accept_) h_accept_(force_move(sp));
                            }
                        );

#endif // BOOST_BEAST_VERSION >= 248

                        // scope out point *1
                    }
                );
                do_accept();
            }
        );
    }

private:
    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context* ioc_con_ = nullptr;
    std::function<as::io_context&()> ioc_con_getter_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    connection_error_handler h_connection_error_;
    error_handler_with_ioc h_error_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};


#if defined(MQTT_USE_TLS)

template <
    typename Strand = strand,
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2
>
class server_tls_ws {
public:
    using socket_t = ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>;
    using endpoint_t = callable_overlay<server_endpoint<Mutex, LockGuard, PacketIdBytes>>;

    /**
     * @brief Accept handler
     * @param ep endpoint of the connecting client
     */
    using accept_handler = std::function<void(std::shared_ptr<endpoint_t> ep)>;

    /**
     * @brief Error handler during after accepted before connection established
     *        After this handler called, the next accept will automatically start.
     * @param ec error code
     * @param ioc_con io_context for incoming connection
     */
    using connection_error_handler = std::function<void(error_code ec, as::io_context& ioc_con)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     */
    using error_handler = std::function<void(error_code ec)>;

    /**
     * @brief Error handler for listen and accpet
     *        After this handler called, the next accept won't  start
     *        You need to call listen() again if you want to restart accepting.
     * @param ec error code
     * @param ioc_con io_context for listen or accept
     */
    using error_handler_with_ioc = std::function<void(error_code ec, as::io_context& ioc_accept)>;

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con,
        AcceptorConfig&& config)
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_(&ioc_con),
          ioc_con_getter_([this]() -> as::io_context& { return *ioc_con_; }),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        as::io_context& ioc_con)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc_accept, ioc_con, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc,
        AcceptorConfig&& config)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, std::forward<AcceptorConfig>(config)) {}

    template <typename AsioEndpoint>
    server_tls_ws(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc)
        : server_tls_ws(std::forward<AsioEndpoint>(ep), force_move(ctx), ioc, ioc, [](as::ip::tcp::acceptor&) {}) {}

    template <typename AsioEndpoint, typename AcceptorConfig>
    server_tls_ws(
        AsioEndpoint&& ep,
        tls::context&& ctx,
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        AcceptorConfig&& config = [](as::ip::tcp::acceptor&) {})
        : ep_(std::forward<AsioEndpoint>(ep)),
          ioc_accept_(ioc_accept),
          ioc_con_getter_(force_move(ioc_con_getter)),
          acceptor_(as::ip::tcp::acceptor(ioc_accept_, ep_)),
          config_(std::forward<AcceptorConfig>(config)),
          ctx_(force_move(ctx)) {
        config_(acceptor_.value());
    }

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
                        if (h_error_) h_error_(ec, ioc_accept_);
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
        as::post(
            ioc_accept_,
            [this] {
                acceptor_.reset();
            }
        );
    }

    void set_accept_handler(accept_handler h = accept_handler()) {
        h_accept_ = force_move(h);
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler h) {
        h_error_ =
            [h = force_move(h)]
            (error_code ec, as::io_context&) {
                if (h) h(ec);
            };
    }

    /**
     * @brief Set error handler for listen and accept
     * @param h handler
     */
    void set_error_handler(error_handler_with_ioc h = error_handler_with_ioc()) {
        h_error_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_connection_error_handler(connection_error_handler h = connection_error_handler()) {
        h_connection_error_ = force_move(h);
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
    void set_underlying_connect_timeout(std::chrono::steady_clock::duration timeout) {
        underlying_connect_timeout_ = force_move(timeout);
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context& get_ssl_context() {
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context const& get_ssl_context() const {
        return ctx_;
    }

    using verify_cb_t = std::function<bool (bool, boost::asio::ssl::verify_context&, std::shared_ptr<optional<std::string>> const&) >;

    void set_verify_callback(verify_cb_t verify_cb) {
        verify_cb_with_username_ = verify_cb;
    }

private:
    void do_accept() {
        if (close_request_) return;
        auto& ioc_con = ioc_con_getter_();
        auto username = std::make_shared<optional<std::string>>(); // shared_ptr for username
        auto verify_cb_ = [this, username] // copy capture socket shared_ptr
            (bool preverified, boost::asio::ssl::verify_context& ctx) {
                // user can set username in the callback
                return verify_cb_with_username_
                    ? verify_cb_with_username_(preverified, ctx, username)
                    : false;
        };

        ctx_.set_verify_mode(MQTT_NS::tls::verify_peer);
        ctx_.set_verify_callback(verify_cb_);

        auto socket = std::make_shared<socket_t>(ioc_con, ctx_);
        auto ps = socket.get();
        acceptor_.value().async_accept(
            ps->next_layer().next_layer(),
            [this, socket = force_move(socket), &ioc_con, username]
            (error_code ec) mutable {
                if (ec) {
                    acceptor_.reset();
                    if (h_error_) h_error_(ec, ioc_con);
                    return;
                }
                auto underlying_finished = std::make_shared<bool>(false);
                auto connection_error_called = std::make_shared<bool>(false);
                auto tim = std::make_shared<as::steady_timer>(ioc_con);
                tim->expires_after(underlying_connect_timeout_);
                tim->async_wait(
                    [
                        this,
                        socket,
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con
                    ]
                    (error_code ec) {
                        if (*underlying_finished) return;
                        if (ec) return; // timer cancelled
                        socket->post(
                            [this, socket, connection_error_called, &ioc_con] {
                                boost::system::error_code close_ec;
                                socket->lowest_layer().close(close_ec);
                                if (h_connection_error_ && !*connection_error_called) {
                                    h_connection_error_(
                                        boost::system::errc::make_error_code(
                                            boost::system::errc::stream_timeout
                                        ),
                                        ioc_con
                                    );
                                    *connection_error_called = true;
                                }
                            }
                        );
                    }
                );

                auto ps = socket.get();

                ps->next_layer().async_handshake(
                    tls::stream_base::server,
                    [
                        this,
                        socket = force_move(socket),
                        tim,
                        underlying_finished,
                        connection_error_called,
                        &ioc_con,
                        username
                    ]
                    (error_code ec) mutable {
                        if (ec) {
                            *underlying_finished = true;
                            tim->cancel();
                            do_accept();
                            return;
                        }
                        auto sb = std::make_shared<boost::asio::streambuf>();
                        auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                        auto ps = socket.get();
                        boost::beast::http::async_read(
                            ps->next_layer(),
                            *sb,
                            *request,
                            [
                                this,
                                socket = force_move(socket),
                                sb,
                                request,
                                tim,
                                underlying_finished,
                                connection_error_called,
                                &ioc_con,
                                username
                            ]
                            (error_code ec, std::size_t) mutable {
                                if (ec) {
                                    *underlying_finished = true;
                                    tim->cancel();
                                    if (h_connection_error_ && !*connection_error_called) {
                                        h_connection_error_(ec, ioc_con);
                                        *connection_error_called = true;
                                    }
                                    do_accept();
                                    return;
                                }
                                if (!boost::beast::websocket::is_upgrade(*request)) {
                                    *underlying_finished = true;
                                    tim->cancel();
                                    if (h_connection_error_ && !*connection_error_called) {
                                        h_connection_error_(
                                            boost::system::errc::make_error_code(
                                                boost::system::errc::protocol_error
                                            ),
                                            ioc_con
                                        );
                                        *connection_error_called = true;
                                    }
                                    do_accept();
                                    return;
                                }
                                auto ps = socket.get();

#if BOOST_BEAST_VERSION >= 248

                                auto it = request->find("Sec-WebSocket-Protocol");
                                if (it != request->end()) {
                                    ps->set_option(
                                        boost::beast::websocket::stream_base::decorator(
                                            [name = it->name(), value = it->value()] // name is enum, value is boost::string_view
                                            (boost::beast::websocket::response_type& res) {
                                                // This lambda is called before the scope out point *1
                                                res.set(name, value);
                                            }
                                        )
                                    );
                                }
                                ps->async_accept(
                                    *request,
                                    [
                                        this,
                                        socket = force_move(socket),
                                        tim,
                                        underlying_finished,
                                        connection_error_called,
                                        &ioc_con,
                                        username
                                    ]
                                    (error_code ec) mutable {
                                        *underlying_finished = true;
                                        tim->cancel();
                                        if (ec) {
                                            if (h_connection_error_ && !*connection_error_called) {
                                                h_connection_error_(ec, ioc_con);
                                                *connection_error_called = true;
                                            }
                                            do_accept();
                                            return;
                                        }
                                        auto sp = std::make_shared<endpoint_t>(ioc_con, force_move(socket), version_);
                                        sp->set_preauthed_user_name(*username);
                                        if (h_accept_) h_accept_(force_move(sp));
                                        do_accept();
                                    }
                                );

#else  // BOOST_BEAST_VERSION >= 248

                                ps->async_accept_ex(
                                    *request,
                                    [request]
                                    (boost::beast::websocket::response_type& m) {
                                        auto it = request->find("Sec-WebSocket-Protocol");
                                        if (it != request->end()) {
                                            m.insert(it->name(), it->value());
                                        }
                                    },
                                    [
                                        this,
                                        socket = force_move(socket),
                                        tim,
                                        underlying_finished,
                                        connection_error_called,
                                        &ioc_con,
                                        username
                                    ]
                                    (error_code ec) mutable {
                                        *underlying_finished = true;
                                        tim->cancel();
                                        if (ec) {
                                            if (h_connection_error_ && *connection_error_called) {
                                                h_connection_error_(ec, ioc_con);
                                                *connection_error_called = true;
                                            }
                                            do_accept();
                                            return;
                                        }
                                        // TODO: The use of force_move on this line of code causes
                                        // a static assertion that socket is a const object when
                                        // TLS is enabled, and WS is enabled, with Boost 1.70, and gcc 8.3.0
                                        auto sp = std::make_shared<endpoint_t>(ioc_con, socket, version_);
                                        sp->set_preauthed_user_name(*username);
                                        if (h_accept_) h_accept_(force_move(sp));
                                        do_accept();
                                    }
                                );

#endif // BOOST_BEAST_VERSION >= 248

                                // scope out point *1
                            }
                        );
                    }
                );
            }
        );
    }

private:
    verify_cb_t verify_cb_with_username_;

    as::ip::tcp::endpoint ep_;
    as::io_context& ioc_accept_;
    as::io_context* ioc_con_ = nullptr;
    std::function<as::io_context&()> ioc_con_getter_;
    optional<as::ip::tcp::acceptor> acceptor_;
    std::function<void(as::ip::tcp::acceptor&)> config_;
    bool close_request_{false};
    accept_handler h_accept_;
    connection_error_handler h_connection_error_;
    error_handler_with_ioc h_error_;
    tls::context ctx_;
    protocol_version version_ = protocol_version::undetermined;
    std::chrono::steady_clock::duration underlying_connect_timeout_ = std::chrono::seconds(10);
};

#endif // defined(MQTT_USE_TLS)

#endif // defined(MQTT_USE_WS)

} // namespace MQTT_NS

#endif // MQTT_SERVER_HPP

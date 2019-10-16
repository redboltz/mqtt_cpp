// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CLIENT_HPP)
#define MQTT_CLIENT_HPP

#include <mqtt/variant.hpp> // should be top to configure variant limit

#include <string>
#include <vector>
#include <functional>

#include <mqtt/namespace.hpp>
#include <mqtt/optional.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

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
namespace mi = boost::multi_index;

template <typename Socket, std::size_t PacketIdBytes = 2>
class client : public endpoint<std::mutex, std::lock_guard, PacketIdBytes> {
    using this_type = client<Socket, PacketIdBytes>;
    using base = endpoint<std::mutex, std::lock_guard, PacketIdBytes>;
protected:
    struct constructor_access{};
public:
    using async_handler_t = typename base::async_handler_t;

    /**
     * Constructor used by factory functions at the end of this file.
     */
    template<typename ... Args>
    client(constructor_access, Args && ... args)
     : client(std::forward<Args>(args)...)
    { }

    /**
     * @brief Create no tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
    make_client(as::io_context& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>>
    make_client_no_strand(as::io_context& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object.
     *  strand is controlled by ws_endpoint, not endpoint, so client has null_strand template argument.
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
    make_client_ws(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>>>>
    make_client_no_strand_ws(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
    make_tls_client(as::io_context& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
    make_tls_client_no_strand(as::io_context& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object.
     *  strand is controlled by ws_endpoint, not endpoint, so client has null_strand template argument.
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
    make_tls_client_ws(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
    make_tls_client_no_strand_ws(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Create no tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
    make_client_32(as::io_context& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
    make_client_no_strand_32(as::io_context& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object.
     *  strand is controlled by ws_endpoint, not endpoint, so client has null_strand template argument.
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
    make_client_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
    make_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
    make_tls_client_32(as::io_context& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
    make_tls_client_no_strand_32(as::io_context& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object.
     *  strand is controlled by ws_endpoint, not endpoint, so client has null_strand template argument.
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
    make_tls_client_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    friend std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
    make_tls_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Set client id.
     * @param id client id
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     * 3.1.3.1 Client Identifier
     */
    void set_client_id(std::string id) {
        client_id_ = force_move(id);
    }

    /**
     * @brief Set clean session.
     * @param cs clean session
     *
     * This function should be called before calling connect().<BR>
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349231<BR>
     * 3.1.2.4 Clean Session<BR>
     * After constructing a endpoint, the clean session is set to false.
     */
    void set_clean_session(bool cs) {
        base::clean_session_ = cs;
    }

    /**
     * @brief Set clean start.
     * @param cs clean start
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     * 3.1.2.4 Clean Start<BR>
     * After constructing a endpoint, the clean start is set to false.
     */
    void set_clean_start(bool cs) {
        set_clean_session(cs);
    }

    /**
     * @brief Set username.
     * @param name username
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     * 3.1.3.5 User Name
     */
    void set_user_name(std::string name) {
        user_name_ = force_move(name);
    }

    /**
     * @brief Set password.
     * @param password password
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     * 3.1.3.6 Password
     */
    void set_password(std::string password) {
        password_ = force_move(password);
    }

    /**
     * @brief Set will.
     * @param w will
     *
     * This function should be called before calling connect().<BR>
     * 'will' would be send when endpoint is disconnected without calling disconnect().
     */
    void set_will(will w) {
        will_ = force_move(w);
    }

#if defined(MQTT_USE_TLS)

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context& get_ssl_context() {
        static_assert(has_tls<std::decay_t<decltype(*this)>>::value, "Client is required to support TLS.");
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    as::ssl::context const& get_ssl_context() const {
        static_assert(has_tls<std::decay_t<decltype(*this)>>::value, "Client is required to support TLS.");
        return ctx_;
    }
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Set a keep alive second and a ping milli seconds.
     * @param keep_alive_sec keep alive seconds
     * @param ping_ms ping sending interval
     *
     * When a endpoint connects to a broker, the endpoint notifies keep_alive_sec to
     * the broker.
     * After connecting, the broker starts counting a timeout, and the endpoint starts
     * sending ping packets for each ping_ms.
     * When the broker receives a ping packet, timeout timer is reset.
     * If the broker doesn't receive a ping packet within keep_alive_sec, the endpoint
     * is disconnected.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc464635115<BR>
     * 3.1.2.10 Keep Alive
     */
    void set_keep_alive_sec_ping_ms(std::uint16_t keep_alive_sec, std::size_t ping_ms) {
        if (ping_duration_ms_ != 0 && base::connected() && ping_ms == 0) {
            tim_ping_.cancel();
        }
        keep_alive_sec_ = keep_alive_sec;
        ping_duration_ms_ = ping_ms;
    }

    /**
     * @brief Set a keep alive second and a ping milli seconds.
     * @param keep_alive_sec keep alive seconds
     *
     * Call set_keep_alive_sec_ping_ms(keep_alive_sec, keep_alive_sec * 1000 / 2)<BR>
     * ping_ms is set to a half of keep_alive_sec.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc464635115<BR>
     * 3.1.2.10 Keep Alive
     */
    void set_keep_alive_sec(std::uint16_t keep_alive_sec) {
        set_keep_alive_sec_ping_ms(keep_alive_sec, keep_alive_sec * 1000 / 2);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param func finish handler that is called when the session is finished
     */
    void connect(any session_life_keeper = any()) {
        connect(v5::properties{}, force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param func finish handler that is called when the session is finished
     */
    void connect(v5::properties props, any session_life_keeper = any()) {
        as::ip::tcp::resolver r(ioc_);
        auto eps = r.resolve(host_, port_);
        setup_socket(socket_);
        connect_impl(*socket_, eps.begin(), eps.end(), force_move(props), force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param func finish handler that is called when the session is finished
     */
    void connect(std::shared_ptr<Socket>&& socket, any session_life_keeper = any()) {
        connect(force_move(socket), v5::properties{}, force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param func finish handler that is called when the session is finished
     */
    void connect(std::shared_ptr<Socket>&& socket, v5::properties props, any session_life_keeper = any()) {
        as::ip::tcp::resolver r(ioc_);
        auto eps = r.resolve(host_, port_);
        socket_ = force_move(socket);
        base::socket_optional().emplace(socket_);
        connect_impl(*socket_, eps.begin(), eps.end(), force_move(props), force_move(session_life_keeper));
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param timeout after timeout elapsed, force_disconnect() is automatically called.
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     */
    void disconnect(
        boost::posix_time::time_duration const& timeout,
        v5::disconnect_reason_code reason_code = v5::disconnect_reason_code::normal_disconnection,
        v5::properties props = {}
    ) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            tim_close_.expires_from_now(timeout);
            tim_close_.async_wait(
                [wp = force_move(wp)](boost::system::error_code const& ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            sp->force_disconnect();
                        }
                    }
                }
            );
            base::disconnect(reason_code, force_move(props));
        }
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     */
    void disconnect(
        v5::disconnect_reason_code reason_code = v5::disconnect_reason_code::normal_disconnection,
        v5::properties props = {}
    ) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            base::disconnect(reason_code, force_move(props));
        }
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param timeout after timeout elapsed, force_disconnect() is automatically called.
     * @param func A callback function that is called when async operation will finish.
     */
    void async_disconnect(
        boost::posix_time::time_duration const& timeout,
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            tim_close_.expires_from_now(timeout);
            tim_close_.async_wait(
                [wp = force_move(wp)](boost::system::error_code const& ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            sp->force_disconnect();
                        }
                    }
                }
            );
            base::async_disconnect(force_move(func));
        }
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param timeout after timeout elapsed, force_disconnect() is automatically called.
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     * @param func A callback function that is called when async operation will finish.
     */
    void async_disconnect(
        boost::posix_time::time_duration const& timeout,
        v5::disconnect_reason_code reason_code,
        v5::properties props,
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            tim_close_.expires_from_now(timeout);
            tim_close_.async_wait(
                [wp = force_move(wp)](boost::system::error_code const& ec) {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            sp->force_disconnect();
                        }
                    }
                }
            );
            base::async_disconnect(reason_code, force_move(props), force_move(func));
        }
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param func A callback function that is called when async operation will finish.
     */
    void async_disconnect(
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            base::async_disconnect(force_move(func));
        }
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     * @param func A callback function that is called when async operation will finish.
     */
    void async_disconnect(
        v5::disconnect_reason_code reason_code,
        v5::properties props,
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        if (base::connected()) {
            base::async_disconnect(reason_code, force_move(props), force_move(func));
        }
    }

    /**
     * @brief Disconnect by endpoint
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    void force_disconnect() {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
        tim_close_.cancel();
        base::force_disconnect();
    }


    /**
     * @brief Set pingreq message sending mode
     * @param b If true then send pingreq asynchronously, otherwise send synchronously.
     */
    void set_async_pingreq(bool b) {
        async_pingreq_ = b;
    }

    std::shared_ptr<Socket> const& socket() const {
        return socket_;
    }

    std::shared_ptr<Socket>& socket() {
        return socket_;
    }

protected:
    client(as::io_context& ioc,
           std::string host,
           std::string port
#if defined(MQTT_USE_WS)
           ,
           std::string path = "/"
#endif // defined(MQTT_USE_WS)
           ,
           protocol_version version = protocol_version::v3_1_1,
           bool async_store_send = false
    )
        :base(version, async_store_send),
         ioc_(ioc),
         tim_ping_(ioc_),
         tim_close_(ioc_),
         host_(force_move(host)),
         port_(force_move(port))
#if defined(MQTT_USE_WS)
         ,
         path_(force_move(path))
#endif // defined(MQTT_USE_WS)
    {
#if defined(MQTT_USE_TLS)
        ctx_.set_verify_mode(as::ssl::verify_peer);
#endif // defined(MQTT_USE_TLS)
    }

private:
    template <typename Strand>
    void setup_socket(std::shared_ptr<tcp_endpoint<as::ip::tcp::socket, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_);
        base::socket_optional().emplace(socket);
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<ws_endpoint<as::ip::tcp::socket, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_);
        base::socket_optional().emplace(socket);
    }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_, ctx_);
        base::socket_optional().emplace(socket);
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_, ctx_);
        base::socket_optional().emplace(socket);
    }
#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    void start_session(v5::properties props, any session_life_keeper) {
        base::async_read_control_packet_type(force_move(session_life_keeper));
        // sync base::connect() refer to parameters only in the function.
        // So they can be passed as view.
        base::connect(
            buffer(string_view(client_id_)),
            ( user_name_ ? buffer(string_view(user_name_.value()))
                         : buffer() ),
            ( password_  ? buffer(string_view(password_.value()))
                         : buffer() ),
            will_,
            keep_alive_sec_,
            force_move(props)
        );
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper) {
        start_session(force_move(props), force_move(session_life_keeper));
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.async_handshake(
            host_,
            path_,
            [this, self = this->shared_from_this(), session_life_keeper = force_move(session_life_keeper), props = force_move(props)]
            (boost::system::error_code const& ec) mutable {
                if (base::handle_close_or_error(ec)) return;
                start_session(force_move(props), force_move(session_life_keeper));
            });
    }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.async_handshake(
            as::ssl::stream_base::client,
            [this, self = this->shared_from_this(), session_life_keeper = force_move(session_life_keeper), props = force_move(props)]
            (boost::system::error_code const& ec) mutable {
                if (base::handle_close_or_error(ec)) return;
                start_session(force_move(props), force_move(session_life_keeper));
            });
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.next_layer().async_handshake(
            as::ssl::stream_base::client,
            [this, self = this->shared_from_this(), session_life_keeper = force_move(session_life_keeper), &socket, props = force_move(props)]
            (boost::system::error_code const& ec) mutable {
                if (base::handle_close_or_error(ec)) return;
                socket.async_handshake(
                    host_,
                    path_,
                    [this, self, session_life_keeper = force_move(session_life_keeper), props = force_move(props)]
                    (boost::system::error_code const& ec) mutable {
                        if (base::handle_close_or_error(ec)) return;
                        start_session(force_move(props), force_move(session_life_keeper));
                    });
            });
    }
#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    template <typename Iterator>
    void connect_impl(Socket& socket, Iterator it, Iterator end, v5::properties props, any session_life_keeper) {
        as::async_connect(
            socket.lowest_layer(), it, end,
            [this, self = this->shared_from_this(), &socket, session_life_keeper = force_move(session_life_keeper), props = force_move(props)]
            (boost::system::error_code const& ec, Iterator) mutable {
                if (!ec) {
                    base::set_connect();
                    if (ping_duration_ms_ != 0) {
                        set_timer();
                    }
                }
                if (base::handle_close_or_error(ec)) return;
                handshake_socket(socket, force_move(props), force_move(session_life_keeper));
            });
    }

    void on_pre_send() override
    {
        if (ping_duration_ms_ != 0) {
            reset_timer();
        }
    }

    void handle_timer(boost::system::error_code const& ec) {
        if (!ec) {
            if (async_pingreq_) {
                base::async_pingreq();
            }
            else {
                base::pingreq();
            }
        }
    }

    void set_timer() {
        tim_ping_.expires_from_now(boost::posix_time::milliseconds(ping_duration_ms_));
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
        tim_ping_.async_wait(
            [wp = force_move(wp)](boost::system::error_code const& ec) {
                if (auto sp = wp.lock()) {
                    sp->handle_timer(ec);
                }
            }
        );
    }

    void reset_timer() {
        tim_ping_.cancel();
        set_timer();
    }

    void on_close() override {
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
    }

    void on_error(boost::system::error_code const& ec) override {
        (void)ec;
        if (ping_duration_ms_ != 0) tim_ping_.cancel();
    }

protected:
    // Ensure that only code that knows the *exact* type of an object
    // inheriting from this abstract base class can destruct it.
    // This avoids issues of the destructor not triggering destruction
    // of derived classes, and any member variables contained in them.
    // Note: Not virtual to avoid need for a vtable when possible.
    ~client() = default;

private:

#if defined(MQTT_USE_TLS)

    template <typename T>
    struct has_tls : std::false_type {
    };

    template <typename U>
    struct has_tls<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, U>>> : std::true_type {
    };

#if defined(MQTT_USE_WS)
    template <typename U>
    struct has_tls<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, U>>> : std::true_type {
    };
#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    std::shared_ptr<Socket> socket_;
    as::io_context& ioc_;
    as::deadline_timer tim_ping_;
    as::deadline_timer tim_close_;
    std::string host_;
    std::string port_;
    std::uint16_t keep_alive_sec_{0};
    std::size_t ping_duration_ms_{0};
    std::string client_id_;
    optional<will> will_;
    optional<std::string> user_name_;
    optional<std::string> password_;
    bool async_pingreq_ = false;
#if defined(MQTT_USE_TLS)
    as::ssl::context ctx_{as::ssl::context::tlsv12};
#endif // defined(MQTT_USE_TLS)
#if defined(MQTT_USE_WS)
    std::string path_;
#endif // defined(MQTT_USE_WS)
};

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
make_client(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
make_client(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>>
make_client_no_strand(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ip::tcp::socket, null_strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>>
make_client_no_strand(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_client_no_strand(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
make_client_ws(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>>>>
make_client_ws(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_client_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>>>>
make_client_no_strand_ws(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ip::tcp::socket, null_strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>>>>
make_client_no_strand_ws(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_client_no_strand_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
make_tls_client(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
make_tls_client(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
make_tls_client_no_strand(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
make_tls_client_no_strand(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_no_strand(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
make_tls_client_ws(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>>>>
make_tls_client_ws(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
make_tls_client_no_strand_ws(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>>
make_tls_client_no_strand_ws(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_no_strand_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)


// 32bit Packet Id (experimental)

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
make_client_32(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
make_client_32(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
make_client_no_strand_32(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
make_client_no_strand_32(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_client_no_strand_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
make_client_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, as::io_context::strand>, 4>>>
make_client_ws_32(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_client_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
make_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>>
make_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_client_no_strand_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
make_tls_client_32(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
make_tls_client_32(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
make_tls_client_no_strand_32(as::io_context& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
#if defined(MQTT_USE_WS)
        "/",
#endif // defined(MQTT_USE_WS)
        version
    );
}

inline std::shared_ptr<callable_overlay<client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
make_tls_client_no_strand_32(as::io_context& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_no_strand_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
make_tls_client_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_context::strand>, 4>>>
make_tls_client_ws_32(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
make_tls_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    using client_t = client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

inline std::shared_ptr<callable_overlay<client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>>
make_tls_client_no_strand_ws_32(as::io_context& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1) {
    return make_tls_client_no_strand_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

} // namespace MQTT_NS

#endif // MQTT_CLIENT_HPP

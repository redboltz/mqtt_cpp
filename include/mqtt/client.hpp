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
#include <type_traits>

#include <mqtt/namespace.hpp>
#include <mqtt/optional.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#include <mqtt/endpoint.hpp>
#include <mqtt/move.hpp>
#include <mqtt/constant.hpp>
#include <mqtt/callable_overlay.hpp>
#include <mqtt/strand.hpp>
#include <mqtt/null_strand.hpp>

namespace MQTT_NS {

namespace as = boost::asio;
namespace mi = boost::multi_index;

template <typename Socket, std::size_t PacketIdBytes = 2>
class client;

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)


// 32bit Packet Id (experimental)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

template <typename Socket, std::size_t PacketIdBytes>
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
    explicit client(constructor_access, Args && ... args)
     : client(std::forward<Args>(args)...)
    { }

    /**
     * @brief Create no tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    strand
                >
            >
        >
    >
    make_client(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >
            >
        >
    >
    make_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version);

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
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    strand
                >
            >
        >
    >
    make_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >
            >
        >
    >
    make_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >
            >
        >
    >
    make_tls_client(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >
            >
        >
    >
    make_tls_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version);

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
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >
            >
        >
    >
    make_tls_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >
            >
        >
    >
    make_tls_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Create no tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    strand
                >,
                4
            >
        >
    >
    make_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >,
                4
            >
        >
    >
    make_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

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
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    strand
                >,
                4
            >
        >
    >
    make_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >,
                4
            >
        >
    >
    make_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >,
                4
            >
        >
    >
    make_tls_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >,
                4
            >
        >
    >
    make_tls_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

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
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >,
                4
            >
        >
    >
    make_tls_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >,
                4
            >
        >
    >
    make_tls_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Set host.
     * @param host host to connect
     *
     * The host is used from the next connect() or async_connect() call.
     */
    void set_host(std::string host) {
        host_ = force_move(host);
    }

    /**
     * @brief Set port.
     * @param port port tp connect
     *
     * The port is used from the next connect() or async_connect() call.
     */
    void set_port(std::string port) {
        port_ = force_move(port);
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
        set_clean_start(cs);
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
        base::clean_start_ = cs;
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
     * @brief Clear username.
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     * 3.1.3.5 User Name
     */
    void set_user_name() {
        user_name_ = nullopt;
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
     * @brief Clear password.
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     * 3.1.3.6 Password
     */
    void set_password() {
        password_ = nullopt;
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

    /**
     * @brief Clear will.
     *
     * This function should be called before calling connect().<BR>
     */
    void set_will() {
        will_ = nullopt;
    }

    template<typename ... Args>
    void set_will(Args && ... args) {
        will_.emplace(std::forward<Args>(args)...);
    }

#if defined(MQTT_USE_TLS)

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context& get_ssl_context() {
        static_assert(has_tls<std::decay_t<decltype(*this)>>::value, "Client is required to support TLS.");
        return ctx_;
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    tls::context const& get_ssl_context() const {
        static_assert(has_tls<std::decay_t<decltype(*this)>>::value, "Client is required to support TLS.");
        return ctx_;
    }

#endif // defined(MQTT_USE_TLS)


    /**
     * @brief Set a keep alive second and a ping duration.
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
    void set_keep_alive_sec(std::uint16_t keep_alive_sec, std::chrono::steady_clock::duration ping) {
        if ((ping_duration_ != std::chrono::steady_clock::duration::zero()) && base::connected() && (ping == std::chrono::steady_clock::duration::zero())) {
            cancel_ping_timer();
        }
        keep_alive_sec_ = keep_alive_sec;
        ping_duration_ = force_move(ping);
    }

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
            set_keep_alive_sec(keep_alive_sec, std::chrono::milliseconds(ping_ms));
    }

    /**
     * @brief Set a keep alive second and a ping milli seconds.
     * @param keep_alive_sec keep alive seconds
     *
     * Call set_keep_alive_sec(keep_alive_sec, std::chrono::seconds(keep_alive_sec / 2))<BR>
     * ping_ms is set to a half of keep_alive_sec.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc464635115<BR>
     * 3.1.2.10 Keep Alive
     */
    void set_keep_alive_sec(std::uint16_t keep_alive_sec) {
        set_keep_alive_sec(keep_alive_sec, std::chrono::seconds(keep_alive_sec / 2));
    }


    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(any session_life_keeper = any()) {
        connect(v5::properties{}, force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param ec error code
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(boost::system::error_code& ec, any session_life_keeper = any()) {
        connect(v5::properties{}, ec, force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(v5::properties props, any session_life_keeper = any()) {
        setup_socket(socket_);
        connect_impl(force_move(props), force_move(session_life_keeper), false);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param ec error code
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(
        v5::properties props,
        boost::system::error_code& ec,
        any session_life_keeper = any()) {
        setup_socket(socket_);
        connect_impl(force_move(props), force_move(session_life_keeper), ec, false);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void connect(std::shared_ptr<Socket>&& socket, any session_life_keeper = any(), bool underlying_connected = false) {
        connect(force_move(socket), v5::properties{}, force_move(session_life_keeper), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param ec error code
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        boost::system::error_code& ec,
        any session_life_keeper = any(),
        bool underlying_connected = false) {
        connect(force_move(socket), v5::properties{}, ec, force_move(session_life_keeper), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        v5::properties props,
        any session_life_keeper = any(),
        bool underlying_connected = false) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        connect_impl(force_move(props), force_move(session_life_keeper), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param ec error code
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        v5::properties props,
        boost::system::error_code& ec,
        any session_life_keeper = any(),
        bool underlying_connected = false) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        connect_impl(force_move(props), force_move(session_life_keeper), ec, underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     */
    void async_connect() {
        async_connect(any(), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    template <typename T>
    // to avoid ambiguousness between any and async_handler_t
    std::enable_if_t<
        !std::is_convertible<T, async_handler_t>::value
    >
    async_connect(T session_life_keeper) {
        async_connect(force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param func finish handler that is called when the underlying connection process is finished
     */
    void async_connect(async_handler_t func) {
        async_connect(any(), force_move(func));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param func finish handler that is called when the underlying connection process is finished
     */
    void async_connect(any session_life_keeper, async_handler_t func) {
        async_connect(v5::properties{}, force_move(session_life_keeper), force_move(func));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    template <typename T>
    // to avoid ambiguousness between any and async_handler_t
    std::enable_if_t<
        !std::is_convertible<T, async_handler_t>::value
    >
    async_connect(v5::properties props, T session_life_keeper) {
        async_connect(force_move(props), force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param func finish handler that is called when the underlying connection process is finished
     */
    void async_connect(v5::properties props, async_handler_t func) {
        async_connect(force_move(props), any(), force_move(func));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param func finish handler that is called when the underlying connection process is finished
     */
    void async_connect(v5::properties props, any session_life_keeper, async_handler_t func) {
        setup_socket(socket_);
        async_connect_impl(force_move(props), force_move(session_life_keeper), force_move(func), false);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, bool underlying_connected = false) {
        async_connect(force_move(socket), v5::properties{}, any(), async_handler_t(), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    template <typename T>
    // to avoid ambiguousness between any and async_handler_t
    std::enable_if_t<
        !std::is_convertible<T, async_handler_t>::value
    >
    async_connect(std::shared_ptr<Socket>&& socket, T session_life_keeper, bool underlying_connected = false) {
        async_connect(force_move(socket), v5::properties{}, force_move(session_life_keeper), async_handler_t(), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param func finish handler that is called when the underlying connection process is finished
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, async_handler_t func, bool underlying_connected = false) {
        async_connect(force_move(socket), v5::properties{}, any(), force_move(func), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param func finish handler that is called when the underlying connection process is finished
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, any session_life_keeper, async_handler_t func, bool underlying_connected = false) {
        async_connect(force_move(socket), v5::properties{}, force_move(session_life_keeper), force_move(func), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, bool underlying_connected = false) {
        async_connect(force_move(socket), force_move(props), any(), async_handler_t(), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    template <typename T>
    // to avoid ambiguousness between any and async_handler_t
    std::enable_if_t<
        !std::is_convertible<T, async_handler_t>::value
    >
    async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, T session_life_keeper, bool underlying_connected = false) {
        async_connect(force_move(socket), force_move(props), force_move(session_life_keeper), async_handler_t(), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param func finish handler that is called when the underlying connection process is finished
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, async_handler_t func, bool underlying_connected = false) {
        async_connect(force_move(socket), force_move(props), any(), force_move(func), underlying_connected);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param func finish handler that is called when the underlying connection process is finished
     * @param underlying_connected If the given socket has aleady been connected, then true.
     *                             Connecting processes are skipped and send MQTT CONNECT packet.
     *                             If false then do all underlying connecting process and then
     *                             send MQTT CONNECT packet.
     */
    void async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, any session_life_keeper, async_handler_t func, bool underlying_connected = false) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        async_connect_impl(force_move(props), force_move(session_life_keeper), force_move(func), underlying_connected);
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
        std::chrono::steady_clock::duration timeout,
        v5::disconnect_reason_code reason_code = v5::disconnect_reason_code::normal_disconnection,
        v5::properties props = {}
    ) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            {
                std::lock_guard<std::mutex> g(mtx_tim_close_);
                tim_close_.expires_after(force_move(timeout));
                tim_close_.async_wait(
                    [wp = force_move(wp)](error_code ec) {
                        if (auto sp = wp.lock()) {
                            if (!ec) {
                                sp->socket()->post(
                                    [sp] {
                                        sp->force_disconnect();
                                    }
                                );
                            }
                        }
                    }
                );
            }
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
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
        std::chrono::steady_clock::duration timeout,
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            {
                std::lock_guard<std::mutex> g(mtx_tim_close_);
                tim_close_.expires_after(force_move(timeout));
                tim_close_.async_wait(
                    [wp = force_move(wp)](error_code ec) {
                        if (auto sp = wp.lock()) {
                            if (!ec) {
                                sp->socket()->post(
                                    [sp] {
                                        sp->force_disconnect();
                                    }
                                );
                            }
                        }
                    }
                );
            }
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
        std::chrono::steady_clock::duration timeout,
        v5::disconnect_reason_code reason_code,
        v5::properties props,
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
            {
                std::lock_guard<std::mutex> g(mtx_tim_close_);
                tim_close_.expires_after(force_move(timeout));
                tim_close_.async_wait(
                    [wp = force_move(wp)](error_code ec) {
                        if (auto sp = wp.lock()) {
                            if (!ec) {
                                sp->socket()->post(
                                    [sp] {
                                        sp->force_disconnect();
                                    }
                                );
                            }
                        }
                    }
                );
            }
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
        {
            std::lock_guard<std::mutex> g(mtx_tim_close_);
            tim_close_.cancel();
        }
        base::force_disconnect();
    }

    /**
     * @brief Disconnect by endpoint
     * @param func A callback function that is called when async operation will finish.
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    void async_force_disconnect(
        async_handler_t func = async_handler_t()) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
        {
            std::lock_guard<std::mutex> g(mtx_tim_close_);
            tim_close_.cancel();
        }
        base::async_force_disconnect(force_move(func));
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
           bool async_operation = false
    )
        :base(ioc, version, async_operation),
         ioc_(ioc),
         tim_ping_(ioc_),
         tim_close_(ioc_),
         host_(force_move(host)),
         port_(force_move(port)),
         async_operation_(async_operation)
#if defined(MQTT_USE_WS)
         ,
         path_(force_move(path))
#endif // defined(MQTT_USE_WS)
    {
#if defined(MQTT_USE_TLS)
        ctx_.set_verify_mode(tls::verify_peer);
#endif // defined(MQTT_USE_TLS)
        setup_socket(socket_);
    }

private:
    template <typename Strand>
    void setup_socket(std::shared_ptr<tcp_endpoint<as::ip::tcp::socket, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_);
        base::socket_sp_ref() = socket;
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<ws_endpoint<as::ip::tcp::socket, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_);
        base::socket_sp_ref() = socket;
    }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_, ctx_);
        base::socket_sp_ref() = socket;
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void setup_socket(std::shared_ptr<ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>>& socket) {
        socket = std::make_shared<Socket>(ioc_, ctx_);
        base::socket_sp_ref() = socket;
    }
#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    void start_session(v5::properties props, any session_life_keeper) {
        base::async_read_control_packet_type(force_move(session_life_keeper));
        // sync base::connect() refer to parameters only in the function.
        // So they can be passed as view.
        base::connect(
            buffer(string_view(base::get_client_id())),
            ( user_name_ ? buffer(string_view(user_name_.value()))
                         : optional<buffer>() ),
            ( password_  ? buffer(string_view(password_.value()))
                         : optional<buffer>() ),
            will_,
            keep_alive_sec_,
            force_move(props)
        );
    }

    void async_start_session(v5::properties props, any session_life_keeper, async_handler_t func) {
        base::async_read_control_packet_type(force_move(session_life_keeper));
        // sync base::connect() refer to parameters only in the function.
        // So they can be passed as view.
        base::async_connect(
            buffer(string_view(base::get_client_id())),
            ( user_name_ ? buffer(string_view(user_name_.value()))
                         : optional<buffer>() ),
            ( password_  ? buffer(string_view(password_.value()))
                         : optional<buffer>() ),
            will_,
            keep_alive_sec_,
            force_move(props),
            force_move(func)
        );
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper,
        bool /*underlying_connected*/) {
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec,
        bool /*underlying_connected*/) {
        start_session(force_move(props), force_move(session_life_keeper));
        ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

#if defined(MQTT_USE_WS)

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        bool underlying_connected) {
        if (!underlying_connected) {
            socket.handshake(host_, path_);
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec,
        bool underlying_connected) {
        if (!underlying_connected) {
            ec = boost::system::errc::make_error_code(boost::system::errc::success);
        }
        else {
            socket.handshake(host_, path_, ec);
            if (ec) return;
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        bool underlying_connected) {
        if (!underlying_connected) {
            socket.handshake(tls::stream_base::client);
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec,
        bool underlying_connected) {
        if (underlying_connected) {
            ec = boost::system::errc::make_error_code(boost::system::errc::success);
        }
        else {
            socket.handshake(tls::stream_base::client, ec);
            if (ec) return;
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

#if defined(MQTT_USE_WS)

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        bool underlying_connected) {
        if (!underlying_connected) {
            socket.next_layer().handshake(tls::stream_base::client);
            socket.handshake(host_, path_);
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec,
        bool underlying_connected) {
        if (underlying_connected) {
            ec = boost::system::errc::make_error_code(boost::system::errc::success);
        }
        else {
            socket.next_layer().handshake(tls::stream_base::client, ec);
            if (ec) return;
            socket.handshake(host_, path_, ec);
            if (ec) return;
        }
        start_session(force_move(props), force_move(session_life_keeper));
    }

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    template <typename Strand>
    void async_handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper,
        async_handler_t func,
        bool /*underlying_connected*/) {
        async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void async_handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        async_handler_t func,
        bool underlying_connected) {
        if (underlying_connected) {
            async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
            return;
        }
        socket.async_handshake(
            host_,
            path_,
            [
                this,
                self = this->shared_from_this(),
                session_life_keeper = force_move(session_life_keeper),
                props = force_move(props),
                func = force_move(func)
            ]
            (error_code ec) mutable {
                if (ec) {
                  if (func) func(ec);
                  return;
                }
                async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
            });
    }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

    template <typename Strand>
    void async_handshake_socket(
        tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        async_handler_t func,
        bool underlying_connected) {
        if (underlying_connected) {
            async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
            return;
        }
        socket.async_handshake(
            tls::stream_base::client,
            [
                this,
                self = this->shared_from_this(),
                session_life_keeper = force_move(session_life_keeper),
                props = force_move(props),
                func = force_move(func)
            ]
            (error_code ec) mutable {
                if (ec) {
                  if (func) func(ec);
                  return;
                }
                async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
            });
    }

#if defined(MQTT_USE_WS)
    template <typename Strand>
    void async_handshake_socket(
        ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        async_handler_t func,
        bool underlying_connected) {
        if (underlying_connected) {
            async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
            return;
        }
        socket.next_layer().async_handshake(
            tls::stream_base::client,
            [
                this,
                self = this->shared_from_this(),
                session_life_keeper = force_move(session_life_keeper),
                &socket,
                props = force_move(props),
                func = force_move(func)
            ]
            (error_code ec) mutable {
                if (ec) {
                    if (func) func(ec);
                    return;
                }
                socket.async_handshake(
                    host_,
                    path_,
                    [
                        this,
                        self,
                        session_life_keeper = force_move(session_life_keeper),
                        props = force_move(props),
                        func = force_move(func)
                    ]
                    (error_code ec) mutable {
                        if (ec) {
                            if (func) func(ec);
                            return;
                        }
                        async_start_session(force_move(props), force_move(session_life_keeper), force_move(func));
                    });
            });
    }
#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    void connect_impl(
        v5::properties props,
        any session_life_keeper,
        bool underlying_connected) {
        if (!underlying_connected) {
            as::ip::tcp::resolver r(ioc_);
            auto eps = r.resolve(host_, port_);
            as::connect(socket_->lowest_layer(), eps.begin(), eps.end());
        }
        base::set_connect();
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            set_ping_timer();
        }
        handshake_socket(*socket_, force_move(props), force_move(session_life_keeper), underlying_connected);
    }

    void connect_impl(
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec,
        bool underlying_connected) {
        if (!underlying_connected) {
            as::ip::tcp::resolver r(ioc_);
            auto eps = r.resolve(host_, port_, ec);
            if (ec) return;
            as::connect(socket_->lowest_layer(), eps.begin(), eps.end(), ec);
            if (ec) return;
        }
        base::set_connect();
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            set_ping_timer();
        }
        handshake_socket(*socket_, force_move(props), force_move(session_life_keeper), ec, underlying_connected);
    }

    void async_connect_impl(
        v5::properties props,
        any session_life_keeper,
        async_handler_t func,
        bool underlying_connected) {
        if (underlying_connected) {
            base::set_connect();
            if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
                set_ping_timer();
            }
            async_handshake_socket(*socket_, force_move(props), force_move(session_life_keeper), force_move(func), underlying_connected);
        }
        else {
            auto r = std::make_shared<as::ip::tcp::resolver>(ioc_);
            auto p = r.get();
            p->async_resolve(
                host_,
                port_,
                [
                    this,
                    self = this->shared_from_this(),
                    props = force_move(props),
                    session_life_keeper = force_move(session_life_keeper),
                    func = force_move(func),
                    r = force_move(r),
                    underlying_connected
                ]
                (
                    error_code ec,
                    as::ip::tcp::resolver::results_type eps
                ) mutable {
                    if (ec) {
                        if (func) func(ec);
                        return;
                    }
                    as::async_connect(
                        socket_->lowest_layer(), eps.begin(), eps.end(),
                        [
                            this,
                            self = force_move(self),
                            props = force_move(props),
                            session_life_keeper = force_move(session_life_keeper),
                            func = force_move(func),
                            underlying_connected
                        ]
                        (error_code ec, auto /* unused */) mutable {
                            if (ec) {
                                if (func) func(ec);
                                return;
                            }
                            base::set_connect();
                            if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
                                set_ping_timer();
                            }
                            async_handshake_socket(*socket_, force_move(props), force_move(session_life_keeper), force_move(func), underlying_connected);
                        }
                    );
                }
            );
        }
    }

protected:
    void on_pre_send() noexcept override {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            reset_ping_timer();
        }
    }

private:
    void handle_timer(error_code ec) {
        if (!ec) {
            if (async_operation_) {
                base::async_pingreq();
            }
            else {
                base::pingreq();
            }
        }
    }

    void set_ping_timer_no_lock() {
        tim_ping_.expires_after(ping_duration_);
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
        tim_ping_.async_wait(
            [wp = force_move(wp)](error_code ec) {
                if (auto sp = wp.lock()) {
                    sp->handle_timer(ec);
                }
            }
        );
    }

    void cancel_timer_no_lock() {
        tim_ping_.cancel();
    }


    void set_ping_timer() {
        std::lock_guard<std::mutex> g(mtx_tim_ping_);
        set_ping_timer_no_lock();
    }

    void cancel_ping_timer() {
        std::lock_guard<std::mutex> g(mtx_tim_ping_);
        cancel_timer_no_lock();
    }

    void reset_ping_timer() {
        std::lock_guard<std::mutex> g(mtx_tim_ping_);
        cancel_timer_no_lock();
        set_ping_timer_no_lock();
    }

protected:
    void on_close() noexcept override {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
    }

    void on_error(error_code ec) noexcept override {
        (void)ec;
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) cancel_ping_timer();
    }

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
    struct has_tls<client<tcp_endpoint<tls::stream<as::ip::tcp::socket>, U>>> : std::true_type {
    };

#if defined(MQTT_USE_WS)

    template <typename U>
    struct has_tls<client<ws_endpoint<tls::stream<as::ip::tcp::socket>, U>>> : std::true_type {
    };

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    std::shared_ptr<Socket> socket_;
    as::io_context& ioc_;
    std::mutex mtx_tim_ping_;
    as::steady_timer tim_ping_;
    std::mutex mtx_tim_close_;
    as::steady_timer tim_close_;
    std::string host_;
    std::string port_;
    std::uint16_t keep_alive_sec_{0};
    std::chrono::steady_clock::duration ping_duration_{std::chrono::steady_clock::duration::zero()};
    optional<will> will_;
    optional<std::string> user_name_;
    optional<std::string> password_;
    bool async_operation_ = false;
#if defined(MQTT_USE_TLS)
    tls::context ctx_{tls::context::tlsv12};
#endif // defined(MQTT_USE_TLS)
#if defined(MQTT_USE_WS)
    std::string path_;
#endif // defined(MQTT_USE_WS)
};

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            as::ip::tcp::socket,
            strand
        >
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            as::ip::tcp::socket,
            null_strand
        >
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client_no_strand(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            as::ip::tcp::socket,
            strand
        >
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            as::ip::tcp::socket,
            null_strand
        >
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client_no_strand(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client_ws(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            as::ip::tcp::socket,
            strand
        >,
        4
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            as::ip::tcp::socket,
            null_strand
        >,
        4
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client_no_strand_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            as::ip::tcp::socket,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_client_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            as::ip::tcp::socket,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >,
        4
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >,
        4
    >;
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

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client_no_strand_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

#if defined(MQTT_USE_WS)

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_client_ws_32(
        ioc,
        force_move(host),
        std::to_string(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using client_t = client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<client_t>>(
        client_t::constructor_access(),
        ioc,
        force_move(host),
        force_move(port),
        force_move(path),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
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

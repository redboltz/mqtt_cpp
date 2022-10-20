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
#include <mqtt/is_invocable.hpp>

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
            tim_ping_.cancel();
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
        connect_impl(force_move(props), force_move(session_life_keeper));
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
        connect_impl(force_move(props), force_move(session_life_keeper), ec);
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(std::shared_ptr<Socket>&& socket, any session_life_keeper = any()) {
        connect(force_move(socket), v5::properties{}, force_move(session_life_keeper));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param ec error code
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        boost::system::error_code& ec,
        any session_life_keeper = any()) {
        connect(force_move(socket), v5::properties{}, ec, force_move(session_life_keeper));
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
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        v5::properties props,
        any session_life_keeper = any()) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        connect_impl(force_move(props), force_move(session_life_keeper));
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
     */
    void connect(
        std::shared_ptr<Socket>&& socket,
        v5::properties props,
        boost::system::error_code& ec,
        any session_life_keeper = any()) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        connect_impl(force_move(props), force_move(session_life_keeper), ec);
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
    template <
        typename T,
        typename std::enable_if_t<
            !is_invocable<T, error_code>::value
        >* = nullptr
    >
    void async_connect(T session_life_keeper) {
        async_connect(force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(CompletionToken&& token) {
        return async_connect(any(), std::forward<CompletionToken>(token));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(any session_life_keeper, CompletionToken&& token) {
        return
            async_connect(
                v5::properties{},
                force_move(session_life_keeper),
                std::forward<CompletionToken>(token)
            );
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    template <
        typename T,
        typename std::enable_if_t<
            !is_invocable<T, error_code>::value
        >* = nullptr
    >
    void async_connect(v5::properties props, T session_life_keeper) {
        async_connect(force_move(props), force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(v5::properties props, CompletionToken&& token) {
        return async_connect(force_move(props), any(), std::forward<CompletionToken>(token));
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(v5::properties props, any session_life_keeper, CompletionToken&& token) {
        setup_socket(socket_);
        return
            as::async_compose<
                CompletionToken,
                void(error_code)
            >(
                async_connect_impl{
                    *this,
                    force_move(props),
                    force_move(session_life_keeper)
                },
                token
            );
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     */
    void async_connect(std::shared_ptr<Socket>&& socket) {
        async_connect(force_move(socket), v5::properties{}, any(), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     */
    template <
        typename T,
        typename std::enable_if_t<
            !is_invocable<T, error_code>::value
        >* = nullptr
    >
    void async_connect(std::shared_ptr<Socket>&& socket, T session_life_keeper) {
        async_connect(force_move(socket), v5::properties{}, force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(std::shared_ptr<Socket>&& socket, CompletionToken&& token) {
        return
            async_connect(
                force_move(socket),
                v5::properties{}, any(),
                std::forward<CompletionToken>(token)
            );
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param session_life_keeper the passed object lifetime will be kept during the session.
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(std::shared_ptr<Socket>&& socket, any session_life_keeper, CompletionToken&& token) {
        return
            async_connect(
                force_move(socket),
                v5::properties{},
                force_move(session_life_keeper),
                std::forward<CompletionToken>(token)
            );
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     */
    void async_connect(std::shared_ptr<Socket>&& socket, v5::properties props) {
        async_connect(force_move(socket), force_move(props), any(), async_handler_t());
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
     */
    template <
        typename T,
        typename std::enable_if_t<
            !is_invocable<T, error_code>::value
        >* = nullptr
    >
    void async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, T session_life_keeper) {
        async_connect(force_move(socket), force_move(props), force_move(session_life_keeper), async_handler_t());
    }

    /**
     * @brief Connect to a broker
     * Before calling connect(), call set_xxx member functions to configure the connection.
     * @param socket The library uses the socket instead of internal generation.
     *               You can configure the socket prior to connect.
     * @param props properties
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param token CompletionToken that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(std::shared_ptr<Socket>&& socket, v5::properties props, CompletionToken&& token) {
        return
            async_connect(
                force_move(socket),
                force_move(props),
                any(),
                std::forward<CompletionToken>(token)
            );
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
     * @param token CompletionTokeny that is called when the underlying connection process is finished
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_connect(
        std::shared_ptr<Socket>&& socket,
        v5::properties props,
        any session_life_keeper,
        CompletionToken&& token) {
        socket_ = force_move(socket);
        base::socket_sp_ref() = socket_;
        return
            as::async_compose<
                CompletionToken,
                void(error_code)
            >(
                async_connect_impl{
                    *this,
                    force_move(props),
                    force_move(session_life_keeper)
                },
                token
            );
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        if (base::connected()) {
            std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
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
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
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
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     * @param token CompletionToken that is called when async operation will finish.
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_disconnect(
        std::chrono::steady_clock::duration timeout,
        v5::disconnect_reason_code reason_code,
        v5::properties props,
        CompletionToken&& token = async_handler_t{}
    ) {

        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
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
        return
            base::async_disconnect(
                reason_code,
                force_move(props),
                std::forward<CompletionToken>(token)
            );
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
     * @param token CompletionToken that is called when async operation will finish.
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_disconnect(
        v5::disconnect_reason_code reason_code,
        v5::properties props,
        CompletionToken&& token = async_handler_t{}
    ) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        return
            base::async_disconnect(
                reason_code,
                force_move(props),
                std::forward<CompletionToken>(token)
            );
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param timeout after timeout elapsed, force_disconnect() is automatically called.
     * @param token CompletionToken that is called when async operation will finish.
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_disconnect(
        std::chrono::steady_clock::duration timeout,
        CompletionToken&& token = async_handler_t{}
    ) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
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
        return
            base::async_disconnect(
                std::forward<CompletionToken>(token)
            );
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param token CompletionToken that is called when async operation will finish.
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_disconnect(
        CompletionToken&& token = async_handler_t{}
    ) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        return base::async_disconnect(token);
    }

    /**
     * @brief Disconnect by endpoint
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    void force_disconnect() {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        tim_close_.cancel();
        base::force_disconnect();
    }

    /**
     * @brief Disconnect by endpoint
     * @param token CompletionToken that is called when async operation will finish.
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    template <
        typename CompletionToken = async_handler_t,
        typename std::enable_if_t<
            is_invocable<CompletionToken, error_code>::value
        >* = nullptr
    >
    auto async_force_disconnect(
        CompletionToken&& token = async_handler_t{}
    ) {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
        tim_close_.cancel();
        return base::async_force_disconnect(token);
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

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper) {
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<as::ip::tcp::socket, Strand>&,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec) {
        start_session(force_move(props), force_move(session_life_keeper));
        ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

#if defined(MQTT_USE_WS)

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.handshake(host_, path_);
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<as::ip::tcp::socket, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec) {
        socket.handshake(host_, path_, ec);
        if (ec) return;
        start_session(force_move(props), force_move(session_life_keeper));
    }

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.handshake(tls::stream_base::client);
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec) {
        socket.handshake(tls::stream_base::client, ec);
        if (ec) return;
        start_session(force_move(props), force_move(session_life_keeper));
    }

#if defined(MQTT_USE_WS)

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper) {
        socket.next_layer().handshake(tls::stream_base::client);
        socket.handshake(host_, path_);
        start_session(force_move(props), force_move(session_life_keeper));
    }

    template <typename Strand>
    void handshake_socket(
        ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec) {
        socket.next_layer().handshake(tls::stream_base::client, ec);
        if (ec) return;
        socket.handshake(host_, path_, ec);
        if (ec) return;
        start_session(force_move(props), force_move(session_life_keeper));
    }

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

    void connect_impl(
        v5::properties props,
        any session_life_keeper) {
        as::ip::tcp::resolver r(ioc_);
        auto eps = r.resolve(host_, port_);
        as::connect(socket_->lowest_layer(), eps.begin(), eps.end());
        base::set_connect();
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            set_timer();
        }
        handshake_socket(*socket_, force_move(props), force_move(session_life_keeper));
    }

    void connect_impl(
        v5::properties props,
        any session_life_keeper,
        boost::system::error_code& ec) {
        as::ip::tcp::resolver r(ioc_);
        auto eps = r.resolve(host_, port_, ec);
        if (ec) return;
        as::connect(socket_->lowest_layer(), eps.begin(), eps.end(), ec);
        if (ec) return;
        base::set_connect();
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            set_timer();
        }
        handshake_socket(*socket_, force_move(props), force_move(session_life_keeper), ec);
    }

    template <typename... Params>
    auto base_async_connect(Params&&... params) {
        return base::async_connect(std::forward<Params>(params)...);
    }

    struct async_connect_impl {
        this_type& cl;
        v5::properties props = {};
        any session_life_keeper;
        as::ip::tcp::resolver resolver{cl.ioc_};
        enum {
            initiate,
            resolve,
            connect,
#if defined(MQTT_USE_TLS)
            tls_handshake,
#if defined(MQTT_USE_WS)
            tls_ws_handshake_tls,
            tls_ws_handshake_ws,
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)
            ws_handshake,
#endif // defined(MQTT_USE_WS)
            start_session,
            complete
        } state = initiate;

        template <typename Self>
        void operator()(
            Self& self
        ) {
            BOOST_ASSERT(state == initiate);
            state = resolve;
            auto& a_cl{cl};
            auto& a_resolver{resolver};
            a_resolver.async_resolve(
                a_cl.host_,
                a_cl.port_,
                force_move(self)
            );
        }

        template <typename Self>
        void operator()(
            Self& self,
            error_code ec
        ) {
            if (ec) {
                self.complete(ec);
                return;
            }
            switch (state) {
#if defined(MQTT_USE_WS)
            case ws_handshake:
                async_start_session(force_move(self));
                break;
#endif // defined(MQTT_USE_WS)
#if defined(MQTT_USE_TLS)
            case tls_handshake:
                async_start_session(force_move(self));
                break;
#if defined(MQTT_USE_WS)
            case tls_ws_handshake_tls: {
                state = tls_ws_handshake_ws;
                auto& socket = *cl.socket_;
                async_ws_handshake_socket(
                    socket,
                    force_move(self)
                );
            } break;
            case tls_ws_handshake_ws:
                async_start_session(force_move(self));
                break;
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)
            case start_session:
                async_start_session(force_move(self));
                break;
            case complete:
                self.complete(ec);
                break;
            default:
                BOOST_ASSERT(false);
                break;
            }
        }

        template <typename Self>
        void operator()(
            Self& self,
            error_code ec,
            as::ip::tcp::resolver::results_type eps
        ) {
            BOOST_ASSERT(state == resolve);
            if (ec) {
                self.complete(ec);
            }
            else {
                state = connect;
                auto& a_cl{cl};
                as::async_connect(
                    a_cl.socket_->lowest_layer(), eps.begin(), eps.end(), force_move(self)
                );
            }
        }

        template <typename Self, typename Iterator>
        void operator()(
            Self& self,
            error_code ec,
            Iterator
        ) {
            BOOST_ASSERT(state == connect);
            if (ec) {
                self.complete(ec);
            }
            else {
                cl.set_connect();
                if (cl.ping_duration_ != std::chrono::steady_clock::duration::zero()) {
                    cl.set_timer();
                }
                auto& socket = *cl.socket_;
                async_handshake_socket(
                    socket,
                    force_move(self)
                );
            }
        }

        template <typename Self, typename Strand>
        void async_handshake_socket(
            tcp_endpoint<as::ip::tcp::socket, Strand>&,
            Self&& self) {
            async_start_session(force_move(self));
        }

#if defined(MQTT_USE_WS)

        template <typename Self, typename Strand>
        void async_handshake_socket(
            ws_endpoint<as::ip::tcp::socket, Strand>& socket,
            Self&& self) {
            state = ws_handshake;
            auto& a_cl{cl};
            socket.async_handshake(
                a_cl.host_,
                a_cl.path_,
                force_move(self)
            );
        }

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

        template <typename Self, typename Strand>
        void async_handshake_socket(
            tcp_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
            Self&& self) {
            state = tls_handshake;
            socket.async_handshake(
                tls::stream_base::client,
                force_move(self)
            );
        }

#if defined(MQTT_USE_WS)

        template <typename Self, typename Strand>
        void async_handshake_socket(
            ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
            Self&& self) {
            state = tls_ws_handshake_tls;
            socket.next_layer().async_handshake(
                tls::stream_base::client,
                force_move(self)
            );
        }

        template <typename Self, typename Strand>
        void async_ws_handshake_socket(
            ws_endpoint<tls::stream<as::ip::tcp::socket>, Strand>& socket,
            Self&& self) {
            state = tls_ws_handshake_ws;
            auto& a_cl{cl};
            socket.async_handshake(
                a_cl.host_,
                a_cl.path_,
                force_move(self)
            );
        }

        template <typename Self, typename Ignore>
        void async_ws_handshake_socket(Ignore&, Self&&) {
            BOOST_ASSERT(false);
        }


#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)

        template <typename Self>
        void async_start_session(Self&& self) {
            state = complete;
            auto& a_cl{cl};
            auto a_props = force_move(props);
            a_cl.async_read_control_packet_type(force_move(session_life_keeper));
            // sync base::connect() refer to parameters only in the function.
            // So they can be passed as view.
            a_cl.base_async_connect(
                buffer(string_view(static_cast<base const&>(a_cl).get_client_id())),
                (a_cl.user_name_ ? buffer(string_view(a_cl.user_name_.value()))
                                 : optional<buffer>() ),
                (a_cl.password_  ? buffer(string_view(a_cl.password_.value()))
                                 : optional<buffer>() ),
                a_cl.will_,
                a_cl.keep_alive_sec_,
                force_move(a_props),
                force_move(self)
            );
        }
    };

protected:
    void on_pre_send() noexcept override {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) {
            reset_timer();
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

    void set_timer() {
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

    void reset_timer() {
        tim_ping_.cancel();
        set_timer();
    }

protected:
    void on_close() noexcept override {
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
    }

    void on_error(error_code ec) noexcept override {
        (void)ec;
        if (ping_duration_ != std::chrono::steady_clock::duration::zero()) tim_ping_.cancel();
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
    as::steady_timer tim_ping_;
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

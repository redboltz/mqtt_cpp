// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SYNC_CLIENT_HPP)
#define MQTT_SYNC_CLIENT_HPP

#include <mqtt/namespace.hpp>
#include <mqtt/client.hpp>
#include <mqtt/move.hpp>
#include <mqtt/callable_overlay.hpp>

namespace MQTT_NS {

template <typename Socket, std::size_t PacketIdBytes = 2>
class sync_client;

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)


// 32bit Packet Id (experimental)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version = protocol_version::v3_1_1);

#if defined(MQTT_USE_WS)

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

template <typename Ioc>
std::shared_ptr<
    callable_overlay<
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path = "/", protocol_version version = protocol_version::v3_1_1);

#endif // defined(MQTT_USE_WS)

#endif // defined(MQTT_USE_TLS)


template <typename Socket, std::size_t PacketIdBytes>
class sync_client : public client<Socket, PacketIdBytes> {
    using this_type = sync_client<Socket, PacketIdBytes>;
    using base = client<Socket, PacketIdBytes>;
    using constructor_access = typename base::constructor_access;
public:

    /**
     * Constructor used by factory functions at the end of this file.
     */
    template<typename ... Args>
    explicit sync_client(constructor_access, Args && ... args)
     : sync_client(std::forward<Args>(args)...)
    { }

    /**
     * @brief Create no tls sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    strand
                >
            >
        >
    >
    make_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >
            >
        >
    >
    make_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so sync_client has null_strand template argument.
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    strand
                >
            >
        >
    >
    make_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >
            >
        >
    >
    make_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >
            >
        >
    >
    make_tls_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >
            >
        >
    >
    make_tls_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so sync_client has null_strand template argument.
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >
            >
        >
    >
    make_tls_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >
            >
        >
    >
    make_tls_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Create no tls sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    strand
                >,
                4
            >
        >
    >
    make_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create no tls sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >,
                4
            >
        >
    >
    make_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so sync_client has null_strand template argument.
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    strand
                >,
                4
            >
        >
    >
    make_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    as::ip::tcp::socket,
                    null_strand
                >,
                4
            >
        >
    >
    make_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
    /**
     * @brief Create tls sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >,
                4
            >
        >
    >
    make_tls_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

    /**
     * @brief Create tls sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                tcp_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >,
                4
            >
        >
    >
    make_tls_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket sync_client with strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so sync_client has null_strand template argument.
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    strand
                >,
                4
            >
        >
    >
    make_tls_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);

    /**
     * @brief Create no tls websocket sync_client without strand.
     * @param ioc io_context object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return sync_client object
     */
    template <typename Ioc>
    friend std::shared_ptr<
        callable_overlay<
            sync_client<
                ws_endpoint<
                    tls::stream<as::ip::tcp::socket>,
                    null_strand
                >,
                4
            >
        >
    >
    make_tls_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version);
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_USE_TLS)

    /**
     * @brief Set auto publish response mode.
     * @param b set value
     * @param sync auto publish ressponse send synchronous
     *
     * When set auto publish response mode to true, puback, pubrec, pubrel,and pub comp automatically send.<BR>
     */
    void set_auto_pub_response(bool b = true) {
        base::set_auto_pub_response(b);
    }

    void async_connect() = delete;
    void async_disconnect() = delete;
    void async_force_disconnect() = delete;

    void async_publish() = delete;
    void async_subscribe() = delete;
    void async_unsubscribe() = delete;
    void async_pingresp() = delete;
    void async_connack() = delete;
    void async_puback() = delete;
    void async_pubrec() = delete;
    void async_pubrel() = delete;
    void async_pubcomp() = delete;
    void async_suback() = delete;
    void async_unsuback() = delete;

protected:
    // Ensure that only code that knows the *exact* type of an object
    // inheriting from this abstract base class can destruct it.
    // This avoids issues of the destructor not triggering destruction
    // of derived classes, and any member variables contained in them.
    // Note: Not virtual to avoid need for a vtable when possible.
    ~sync_client() = default;

    sync_client(
        as::io_context& ioc,
        std::string host,
        std::string port
#if defined(MQTT_USE_WS)
        ,
        std::string path = "/"
#endif // defined(MQTT_USE_WS)
        ,
        protocol_version version = protocol_version::v3_1_1
    ):base(ioc, force_move(host), force_move(port)
#if defined(MQTT_USE_WS)
           , force_move(path)
#endif // defined(MQTT_USE_WS)
           ,
           version
    ) {
        set_auto_pub_response();
    }
};

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            as::ip::tcp::socket,
            strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
                    as::ip::tcp::socket,
                    null_strand
        >>;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_no_strand(
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            as::ip::tcp::socket,
            strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >
        >
    >
>
make_sync_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_ws(
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            as::ip::tcp::socket,
            null_strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >
        >
    >
>
make_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_no_strand_ws(
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_no_strand(
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >
        >
    >
>
make_tls_sync_client_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_ws(
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >
        >
    >
>
make_tls_sync_client_no_strand_ws(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_no_strand_ws(
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
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            as::ip::tcp::socket,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            as::ip::tcp::socket,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_no_strand_32(
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            as::ip::tcp::socket,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                strand
            >,
            4
        >
    >
>
make_sync_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_ws_32(
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            as::ip::tcp::socket,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                as::ip::tcp::socket,
                null_strand
            >,
            4
        >
    >
>
make_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_sync_client_no_strand_ws_32(
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_32(
        ioc,
        force_move(host),
        std::to_string(port),
        version
    );
}

template <typename Ioc>
inline std::shared_ptr<
    callable_overlay<
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_32(Ioc& ioc, std::string host, std::string port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        tcp_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            tcp_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_32(Ioc& ioc, std::string host, std::uint16_t port, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_no_strand_32(
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                strand
            >,
            4
        >
    >
>
make_tls_sync_client_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_ws_32(
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::string port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    using sync_client_t = sync_client<
        ws_endpoint<
            tls::stream<as::ip::tcp::socket>,
            null_strand
        >,
        4
    >;
    return std::make_shared<callable_overlay<sync_client_t>>(
        sync_client_t::constructor_access(),
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
        sync_client<
            ws_endpoint<
                tls::stream<as::ip::tcp::socket>,
                null_strand
            >,
            4
        >
    >
>
make_tls_sync_client_no_strand_ws_32(Ioc& ioc, std::string host, std::uint16_t port, std::string path, protocol_version version) {
    static_assert(std::is_same<Ioc, as::io_context>::value, "The type of ioc must be boost::asio::io_context");
    return make_tls_sync_client_no_strand_ws_32(
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

#endif // MQTT_SYNC_CLIENT_HPP

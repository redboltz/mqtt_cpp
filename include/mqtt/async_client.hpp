// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ASYNC_CLIENT_HPP)
#define MQTT_ASYNC_CLIENT_HPP

#include <mqtt/client.hpp>

namespace mqtt {

template <typename Socket, std::size_t PacketIdBytes = 2>
class async_client : public client<Socket, PacketIdBytes> {
    using this_type = async_client<Socket, PacketIdBytes>;
    using base = client<Socket, PacketIdBytes>;

public:
    /**
     * @brief Create no tls async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
    make_async_client(as::io_service& ios, std::string host, std::string port);

    /**
     * @brief Create no tls async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>
    make_async_client_no_strand(as::io_service& ios, std::string host, std::string port);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so async_client has null_strand template argument.
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
    make_async_client_ws(as::io_service& ios, std::string host, std::string port, std::string path);

    /**
     * @brief Create no tls websocket async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>>>
    make_async_client_no_strand_ws(as::io_service& ios, std::string host, std::string port, std::string path);
#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)
    /**
     * @brief Create tls async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
    make_tls_async_client(as::io_service& ios, std::string host, std::string port);

    /**
     * @brief Create tls async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
    make_tls_async_client_no_strand(as::io_service& ios, std::string host, std::string port);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so async_client has null_strand template argument.
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
    make_tls_async_client_ws(as::io_service& ios, std::string host, std::string port, std::string path);

    /**
     * @brief Create no tls websocket async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
    make_tls_async_client_no_strand_ws(as::io_service& ios, std::string host, std::string port, std::string path);
#endif // defined(MQTT_USE_WS)
#endif // !defined(MQTT_NO_TLS)

    /**
     * @brief Create no tls async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
    make_async_client_32(as::io_service& ios, std::string host, std::string port);

    /**
     * @brief Create no tls async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>
    make_async_client_no_strand_32(as::io_service& ios, std::string host, std::string port);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so async_client has null_strand template argument.
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
    make_async_client_ws_32(as::io_service& ios, std::string host, std::string port, std::string path);

    /**
     * @brief Create no tls websocket async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>
    make_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::string port, std::string path);
#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)
    /**
     * @brief Create tls async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
    make_tls_async_client_32(as::io_service& ios, std::string host, std::string port);

    /**
     * @brief Create tls async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @return async_client object
     */
    friend std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
    make_tls_async_client_no_strand_32(as::io_service& ios, std::string host, std::string port);

#if defined(MQTT_USE_WS)
    /**
     * @brief Create no tls websocket async_client with strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object.
     *  strand is controlled by ws_endpoint, not endpoint, so async_client has null_strand template argument.
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
    make_tls_async_client_ws_32(as::io_service& ios, std::string host, std::string port, std::string path);

    /**
     * @brief Create no tls websocket async_client without strand.
     * @param ios io_service object.
     * @param host hostname
     * @param port port number
     * @param path path string
     * @return async_client object
     */
    friend std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
    make_tls_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::string port, std::string path);
#endif // defined(MQTT_USE_WS)
#endif // !defined(MQTT_NO_TLS)


    /**
     * @brief Set auto publish response mode.
     * @param b set value
     * @param async auto publish ressponse send asynchronous
     *
     * When set auto publish response mode to true, puback, pubrec, pubrel,and pub comp automatically send.<BR>
     */
    void set_auto_pub_response(bool b = true) {
        base::set_auto_pub_response(b, true);
    }

    void disconnect() = delete;
    void publish_at_most_once() = delete;
    void publish_at_least_once() = delete;
    void publish_exactly_once() = delete;

    void publish() = delete;
    void subscribe() = delete;
    void unsubscribe() = delete;
    void publish_dup() = delete;
    void acquired_publish_at_least_once() = delete;
    void acquired_publish_exactly_once() = delete;
    void acquired_publish() = delete;
    void acquired_publish_dup() = delete;
    void acquired_subscribe() = delete;
    void acquired_unsubscribe() = delete;
    void acquired_pingreq() = delete;
    void pingresp() = delete;
    void connack() = delete;
    void puback() = delete;
    void pubrec() = delete;
    void pubrel() = delete;
    void pubcomp() = delete;
    void suback() = delete;
    void unsuback() = delete;

protected:

    async_client(as::io_service& ios,
               std::string host,
               std::string port,
               bool tls
#if defined(MQTT_USE_WS)
               ,
               std::string path = "/"
#endif // defined(MQTT_USE_WS)
    ):base(ios, std::move(host), std::move(port), tls
#if defined(MQTT_USE_WS)
           , std::move(path)
#endif // defined(MQTT_USE_WS)
    ) {
        set_auto_pub_response();
    }
};

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
make_async_client(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
make_async_client(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_async_client(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>
make_async_client_no_strand(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>>>
make_async_client_no_strand(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_async_client_no_strand(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
make_async_client_ws(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>>>
make_async_client_ws(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_async_client_ws(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>>>
make_async_client_no_strand_ws(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ip::tcp::socket, null_strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ip::tcp::socket, null_strand>>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>>>
make_async_client_no_strand_ws(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_async_client_no_strand_ws(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
make_tls_async_client(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>
        (std::ref(ios), std::move(host), std::move(port), true);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
make_tls_async_client(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_tls_async_client(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
make_tls_async_client_no_strand(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
make_tls_async_client_no_strand(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_tls_async_client_no_strand(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
make_tls_async_client_ws(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>>>
make_tls_async_client_ws(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_tls_async_client_ws(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
make_tls_async_client_no_strand_ws(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>>>
make_tls_async_client_no_strand_ws(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_tls_async_client_no_strand_ws(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

#endif // defined(MQTT_USE_WS)

#endif // !defined(MQTT_NO_TLS)


// 32bit Packet Id (experimental)

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
make_async_client_32(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
make_async_client_32(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_async_client_32(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>
make_async_client_no_strand_32(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ip::tcp::socket, null_strand>, 4>>
make_async_client_no_strand_32(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_async_client_no_strand_32(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
make_async_client_ws_32(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, as::io_service::strand>, 4>>
make_async_client_ws_32(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_async_client_ws_32(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>
make_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), false, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ip::tcp::socket, null_strand>, 4>>
make_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_async_client_no_strand_ws_32(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
make_tls_async_client_32(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>
        (std::ref(ios), std::move(host), std::move(port), true);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
make_tls_async_client_32(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_tls_async_client_32(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
make_tls_async_client_no_strand_32(as::io_service& ios, std::string host, std::string port) {
    struct impl : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls)
        : async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>(ios, std::move(host), std::move(port), tls) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true);
}

inline std::shared_ptr<async_client<tcp_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
make_tls_async_client_no_strand_32(as::io_service& ios, std::string host, std::uint16_t port) {
    return make_tls_async_client_no_strand_32(ios, std::move(host), boost::lexical_cast<std::string>(port));
}

#if defined(MQTT_USE_WS)

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
make_tls_async_client_ws_32(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, as::io_service::strand>, 4>>
make_tls_async_client_ws_32(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_tls_async_client_ws_32(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
make_tls_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::string port, std::string path = "/") {
    struct impl : async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4> {
        impl(as::io_service& ios,
             std::string host,
             std::string port,
             bool tls,
             std::string path)
            :
            async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>
            (ios, std::move(host), std::move(port), tls, std::move(path)) {}
    };
    return std::make_shared<impl>(std::ref(ios), std::move(host), std::move(port), true, std::move(path));
}

inline std::shared_ptr<async_client<ws_endpoint<as::ssl::stream<as::ip::tcp::socket>, null_strand>, 4>>
make_tls_async_client_no_strand_ws_32(as::io_service& ios, std::string host, std::uint16_t port, std::string path = "/") {
    return make_tls_async_client_no_strand_ws_32(ios, std::move(host), boost::lexical_cast<std::string>(port), std::move(path));
}

#endif // defined(MQTT_USE_WS)

#endif // !defined(MQTT_NO_TLS)

} // namespace mqtt

#endif // MQTT_ASYNC_CLIENT_HPP

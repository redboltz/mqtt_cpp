// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TLS_IMPLEMENTATION_HPP)
#define MQTT_TLS_IMPLEMENTATION_HPP

#define MQTT_TLS_OPENSSL 1
#define MQTT_TLS_GNUTLS  2

#if defined(MQTT_USE_TLS)
#if MQTT_USE_TLS == 0
#define MQTT_USE_TLS MQTT_TLS_OPENSSL
#endif // MQTT_USE_TLS == 0
#endif // defined(MQTT_USE_TLS)

#if MQTT_USE_TLS == MQTT_TLS_OPENSSL
#include <boost/asio/ssl.hpp>
#elif MQTT_USE_TLS == MQTT_TLS_GNUTLS
#include <boost/asio/gnutls.hpp>
#endif // MQTT_USE_TLS == MQTT_TLS_OPENSSL

#if MQTT_USE_TLS == MQTT_TLS_OPENSSL
namespace tls = boost::asio::ssl;
#elif MQTT_USE_TLS == MQTT_TLS_GNUTLS
namespace tls = boost::asio::gnutls;
#endif // MQTT_USE_TLS == MQTT_TLS_OPENSSL

namespace MQTT_NS {

inline constexpr bool is_tls_short_read(int error_val)
{

#if MQTT_USE_TLS == MQTT_TLS_OPENSSL
#if defined(SSL_R_SHORT_READ)
    return ERR_GET_REASON(error_val) == SSL_R_SHORT_READ;
#else  // defined(SSL_R_SHORT_READ)
    return ERR_GET_REASON(error_val) == tls::error::stream_truncated;
#endif // defined(SSL_R_SHORT_READ)
#elif MQTT_USE_TLS == MQTT_TLS_GNUTLS
    return error_val == tls::error::stream_truncated;
#endif // MQTT_USE_TLS == MQTT_TLS_OPENSSL

    return false;
}

} // namespace MQTT_NS

#endif // MQTT_TLS_IMPLEMENTATION_HPP

// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TLS_IMPLEMENTATION_HPP)
#define MQTT_TLS_IMPLEMENTATION_HPP

#if defined(MQTT_USE_TLS)
#if defined(MQTT_USE_GNU_TLS)
#include <boost/asio/gnutls.hpp>
// The following is used in 'endpoint.hpp' and is not defined by default for Gnu TLS.
# define ERR_GET_REASON(l)       (int)( (l)         & 0xFFFL)
#else
#include <boost/asio/ssl.hpp>
#endif // defined(MQTT_USE_GNU_TLS)
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_TLS)
#if defined(MQTT_USE_GNU_TLS)
namespace tls = boost::asio::gnutls;
#else
namespace tls = boost::asio::ssl;
#endif // defined(MQTT_USE_GNU_TLS)
#endif // defined(MQTT_USE_TLS)

namespace MQTT_NS {

inline constexpr bool is_tls_short_read(int error_val)
{
#if defined(MQTT_USE_TLS)
#if defined(MQTT_USE_GNU_TLS)
    return error_val == tls::error::stream_truncated;
#else
#if defined(SSL_R_SHORT_READ)
    return ERR_GET_REASON(error_val) == SSL_R_SHORT_READ;
#else  // defined(SSL_R_SHORT_READ)
    return ERR_GET_REASON(error_val) == tls::error::stream_truncated;
#endif // defined(SSL_R_SHORT_READ)
#endif // defined(MQTT_USE_GNU_TLS)
#endif // defined(MQTT_USE_TLS)
}

} // namespace MQTT_NS

#endif // MQTT_tls_implementation_HPP

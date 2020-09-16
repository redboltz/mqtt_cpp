// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_CTX_INIT_HPP)
#define MQTT_TEST_CTX_INIT_HPP

#if defined(MQTT_USE_TLS)

#include <boost/asio.hpp>

#if !defined(MQTT_USE_GNU_TLS)
    #include <boost/asio/ssl.hpp>
#else
    #include <boost/asio/gnutls.hpp>
    #include <gnutls/gnutls.h>
#endif // !defined(MQTT_USE_GNU_TLS)

#include "test_settings.hpp"

#if defined(MQTT_USE_TLS)
#if defined(MQTT_USE_GNU_TLS)
    namespace boost
    {
        namespace asio
        {
            namespace ssl = boost::asio::gnutls;
        }
    }
#endif // defined(MQTT_USE_GNU_TLS)
#endif // defined(MQTT_USE_TLS)

struct ctx_init {
    ctx_init() : ctx(as::ssl::context::tlsv12) {
        ctx.set_options(
            as::ssl::context::default_workarounds |
            as::ssl::context::single_dh_use);
        std::string path = boost::unit_test::framework::master_test_suite().argv[0];
        std::size_t pos = path.find_last_of("/\\");
        std::string base = (pos == std::string::npos) ? "" : path.substr(0, pos + 1);
        ctx.use_certificate_file(base + "server.crt.pem", as::ssl::context::pem);
        ctx.use_private_key_file(base + "server.key.pem", as::ssl::context::pem);
    }
    as::ssl::context ctx;
};

#endif // defined(MQTT_USE_TLS)

#endif // MQTT_TEST_CTX_INIT_HPP

// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_COMBI_TEST_HPP)
#define MQTT_TEST_COMBI_TEST_HPP

#include "test_settings.hpp"
#include "test_broker.hpp"
#include "test_server_no_tls.hpp"
#if !defined(MQTT_NO_TLS)
#include "test_server_tls.hpp"
#endif // !defined(MQTT_NO_TLS)

#if defined(MQTT_USE_WS)
#include "test_server_no_tls_ws.hpp"
#if !defined(MQTT_NO_TLS)
#include "test_server_tls_ws.hpp"
#endif // !defined(MQTT_NO_TLS)
#endif // defined(MQTT_USE_WS)

#include <mqtt/client.hpp>

template <typename Test>
inline void do_combi_test(Test const& test) {
    {
        boost::asio::io_service ios;
        test_broker b(ios);
        test_server_no_tls s(ios, b);
        auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
        test(ios, c, s);
    }
#if !defined(MQTT_NO_TLS)
    {
        boost::asio::io_service ios;
        test_broker b(ios);
        test_server_tls s(ios, b);
        auto c = mqtt::make_tls_client(ios, broker_url, broker_tls_port);
        std::string path = boost::unit_test::framework::master_test_suite().argv[0];
        std::size_t pos = path.find_last_of("/\\");
        std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);
        c->set_ca_cert_file(base + "cacert.pem");
        test(ios, c, s);
    }
#endif // !defined(MQTT_NO_TLS)
#if defined(MQTT_USE_WS)
    {
        boost::asio::io_service ios;
        test_broker b(ios);
        test_server_no_tls_ws s(ios, b);
        auto c = mqtt::make_client_ws(ios, broker_url, broker_notls_ws_port);
        test(ios, c, s);
    }
#if !defined(MQTT_NO_TLS)
    {
        boost::asio::io_service ios;
        test_broker b(ios);
        test_server_tls_ws s(ios, b);
        auto c = mqtt::make_tls_client_ws(ios, broker_url, broker_tls_ws_port);
        std::string path = boost::unit_test::framework::master_test_suite().argv[0];
        std::size_t pos = path.find_last_of("/\\");
        std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);
        c->set_ca_cert_file(base + "cacert.pem");
        test(ios, c, s);
    }
#endif // !defined(MQTT_NO_TLS)
#endif // defined(MQTT_USE_WS)
}

#endif // MQTT_TEST_COMBI_TEST_HPP

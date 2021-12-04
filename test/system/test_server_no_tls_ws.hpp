// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SERVER_NO_TLS_WS_HPP)
#define MQTT_TEST_SERVER_NO_TLS_WS_HPP

#if defined(MQTT_USE_WS)
#include <mqtt_server_cpp.hpp>

#include <mqtt/broker/broker.hpp>
#include "test_settings.hpp"

namespace mi = boost::multi_index;
namespace as = boost::asio;

class test_server_no_tls_ws {
public:
    test_server_no_tls_ws(as::io_context& ioc, MQTT_NS::broker::broker_t& b, uint16_t port = broker_notls_ws_port)
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), port
            ),
            ioc,
            ioc,
            [](auto& acceptor) {
                acceptor.set_option(as::ip::tcp::acceptor::reuse_address(true));
            }
        ), b_(b) {
        server_.set_error_handler(
            [](MQTT_NS::error_code /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](std::shared_ptr<MQTT_NS::server_ws<MQTT_NS::strand, MQTT_NS::null_mutex>::endpoint_t> spep) {
                b_.handle_accept(MQTT_NS::force_move(spep));
            }
        );

        server_.listen();
    }

    MQTT_NS::broker::broker_t& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

private:
    MQTT_NS::server_ws<MQTT_NS::strand, MQTT_NS::null_mutex> server_;
    MQTT_NS::broker::broker_t& b_;
};

#endif // defined(MQTT_USE_WS)

#endif // MQTT_TEST_SERVER_NO_TLS_WS_HPP

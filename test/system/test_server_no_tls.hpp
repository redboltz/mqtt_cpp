// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SERVER_NO_TLS_HPP)
#define MQTT_TEST_SERVER_NO_TLS_HPP

#include <mqtt_server_cpp.hpp>

#include <mqtt/broker/broker.hpp>

#include "test_settings.hpp"

namespace mi = boost::multi_index;
namespace as = boost::asio;

using con_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;

class test_server_no_tls {
public:
    test_server_no_tls(as::io_context& ioc, MQTT_NS::broker::broker_t& b)
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), broker_notls_port
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
            [&](con_sp_t spep) {
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
    MQTT_NS::server<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#endif // MQTT_TEST_SERVER_NO_TLS_HPP

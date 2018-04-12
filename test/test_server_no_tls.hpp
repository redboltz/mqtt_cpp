// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SERVER_NO_TLS_HPP)
#define MQTT_TEST_SERVER_NO_TLS_HPP

#include <mqtt_server_cpp.hpp>
#include "test_settings.hpp"
#include "test_broker.hpp"

namespace mi = boost::multi_index;
namespace as = boost::asio;


class test_server_no_tls {
public:
    test_server_no_tls(as::io_service& ios, test_broker& b)
        : server_(as::ip::tcp::endpoint(as::ip::tcp::v4(), broker_notls_port), ios), b_(b) {
        server_.set_error_handler(
            [](boost::system::error_code const& /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](mqtt::server<>::endpoint_t& ep) {
                b_.handle_accept(ep);
            }
        );

        server_.listen();
    }

    test_broker& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

private:
    mqtt::server<> server_;
    test_broker& b_;
};

#endif // MQTT_TEST_SERVER_NO_TLS_HPP

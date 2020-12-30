// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SERVER_TLS_HPP)
#define MQTT_TEST_SERVER_TLS_HPP

#if defined(MQTT_USE_TLS)
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <mqtt_server_cpp.hpp>

namespace mi = boost::multi_index;
namespace as = boost::asio;

class test_server_tls {
public:
    test_server_tls(as::io_context& ioc, boost::asio::ssl::context&& ctx, MQTT_NS::broker::broker_t& b, uint16_t port = broker_tls_port)
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), port
            ),
            MQTT_NS::force_move(ctx),
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
            [&](std::shared_ptr<MQTT_NS::server_tls<>::endpoint_t> spep) {
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
    MQTT_NS::server_tls<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#endif

#endif // MQTT_TEST_SERVER_TLS_HPP

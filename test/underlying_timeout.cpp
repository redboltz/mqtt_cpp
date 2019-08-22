// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include <mqtt_server_cpp.hpp>
#include "test_settings.hpp"
#include "test_ctx_init.hpp"
#include "checker.hpp"

BOOST_AUTO_TEST_SUITE(test_underlying_timeout)

namespace as = boost::asio;

BOOST_AUTO_TEST_CASE( dummy ) {
}

#if defined(MQTT_USE_WS)

BOOST_AUTO_TEST_CASE( connect_ws_upg ) {
    as::io_service ios;

    // server
    mqtt::server_ws<> server(
        as::ip::tcp::endpoint(
            as::ip::tcp::v4(),
            broker_notls_ws_port),
        ios);

    server.set_accept_handler(
        [&](mqtt::server_ws<>::endpoint_t& /*ep*/) {
            BOOST_TEST(false);
        }
    );

    server.set_underlying_connect_timeout(boost::posix_time::seconds(1));
    server.listen();

    // client
    as::ip::tcp::resolver r(ios);
#if BOOST_VERSION < 106600
    as::ip::tcp::resolver::query q(broker_url, std::to_string(broker_notls_ws_port));
    auto it = r.resolve(q);
    as::ip::tcp::resolver::iterator end;
#else  // BOOST_VERSION < 106600
    auto eps = r.resolve(broker_url, std::to_string(broker_notls_ws_port));
    auto it = eps.begin();
    auto end = eps.end();
#endif // BOOST_VERSION < 106600

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);

    boost::beast::websocket::stream<as::ip::tcp::socket> socket(ios);

    char buf;

    as::async_connect(
        socket.lowest_layer(), it, end,
        [&]
        (boost::system::error_code const& ec, as::ip::tcp::resolver::iterator) {
            if (ec) {
                std::cout << ec.message() << std::endl;
            }
            BOOST_TEST(!ec);
            // intentionally don't call async_handshake (WS)
            as::async_read(
                socket,
                as::buffer(&buf, 1),
                [&]
                (boost::system::error_code const& ec,
                 std::size_t /*bytes_transferred*/){
                    BOOST_TEST(ec);
                    server.close();
                }
            );
        }
    );

    ios.run();
}

#if !defined(MQTT_NO_TLS)

BOOST_AUTO_TEST_CASE( connect_tls_ws_ashs ) {
    as::io_service ios;

    // server
    ctx_init ci;
    mqtt::server_tls_ws<> server(
        as::ip::tcp::endpoint(
            as::ip::tcp::v4(),
            broker_tls_ws_port),
        std::move(ci.ctx),
        ios);

    server.set_accept_handler(
        [&](mqtt::server_tls_ws<>::endpoint_t& /*ep*/) {
            BOOST_TEST(false);
        }
    );

    server.set_underlying_connect_timeout(boost::posix_time::seconds(1));
    server.listen();

    // client
    as::ip::tcp::resolver r(ios);
#if BOOST_VERSION < 106600
    as::ip::tcp::resolver::query q(broker_url, std::to_string(broker_tls_ws_port));
    auto it = r.resolve(q);
    as::ip::tcp::resolver::iterator end;
#else  // BOOST_VERSION < 106600
    auto eps = r.resolve(broker_url, std::to_string(broker_tls_ws_port));
    auto it = eps.begin();
    auto end = eps.end();
#endif // BOOST_VERSION < 106600

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);

    as::ssl::context ctx {as::ssl::context::tlsv12};
    ctx.load_verify_file(base + "cacert.pem");
    ctx.set_verify_mode(as::ssl::verify_peer);
    boost::beast::websocket::stream<as::ssl::stream<as::ip::tcp::socket>> socket(ios, ctx);

    char buf;

    as::async_connect(
        socket.lowest_layer(), it, end,
        [&]
        (boost::system::error_code const& ec, as::ip::tcp::resolver::iterator) {
            if (ec) {
                std::cout << ec.message() << std::endl;
            }
            BOOST_TEST(!ec);
            // intentionally don't call async_handshake (TLS)
            as::async_read(
                socket,
                as::buffer(&buf, 1),
                [&]
                (boost::system::error_code const& ec,
                 std::size_t /*bytes_transferred*/){
                    BOOST_TEST(ec);
                    server.close();
                }
            );
        }
    );

    ios.run();
}

BOOST_AUTO_TEST_CASE( connect_tls_ws_upg ) {
    as::io_service ios;

    // server
    ctx_init ci;
    mqtt::server_tls_ws<> server(
        as::ip::tcp::endpoint(
            as::ip::tcp::v4(),
            broker_tls_ws_port),
        std::move(ci.ctx),
        ios);

    server.set_accept_handler(
        [&](mqtt::server_tls_ws<>::endpoint_t& /*ep*/) {
            BOOST_TEST(false);
        }
    );

    server.set_underlying_connect_timeout(boost::posix_time::seconds(1));
    server.listen();

    // client
    as::ip::tcp::resolver r(ios);
#if BOOST_VERSION < 106600
    as::ip::tcp::resolver::query q(broker_url, std::to_string(broker_tls_ws_port));
    auto it = r.resolve(q);
    as::ip::tcp::resolver::iterator end;
#else  // BOOST_VERSION < 106600
    auto eps = r.resolve(broker_url, std::to_string(broker_tls_ws_port));
    auto it = eps.begin();
    auto end = eps.end();
#endif // BOOST_VERSION < 106600

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);

    as::ssl::context ctx {as::ssl::context::tlsv12};
    ctx.load_verify_file(base + "cacert.pem");
    ctx.set_verify_mode(as::ssl::verify_peer);
    boost::beast::websocket::stream<as::ssl::stream<as::ip::tcp::socket>> socket(ios, ctx);

    char buf;

    as::async_connect(
        socket.lowest_layer(), it, end,
        [&]
        (boost::system::error_code const& ec, as::ip::tcp::resolver::iterator) {
            if (ec) {
                std::cout << ec.message() << std::endl;
            }
            BOOST_TEST(!ec);

            socket.next_layer().async_handshake(
                as::ssl::stream_base::client,
                [&]
                (boost::system::error_code const& ec) {
                    if (ec) {
                        std::cout << ec.message() << std::endl;
                    }
                    BOOST_TEST(!ec);
                    // intentionally don't call async_handshake (WS)
                    as::async_read(
                        socket,
                        as::buffer(&buf, 1),
                        [&]
                        (boost::system::error_code const& ec,
                         std::size_t /*bytes_transferred*/){
                            BOOST_TEST(ec);
                            server.close();
                        }
                    );
                }
            );
        }
    );

    ios.run();
}

#endif // !defined(MQTT_NO_TLS)

#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)

BOOST_AUTO_TEST_CASE( connect_tls_ashs ) {
    as::io_service ios;

    // server
    ctx_init ci;
    mqtt::server_tls<> server(
        as::ip::tcp::endpoint(
            as::ip::tcp::v4(),
            broker_tls_port),
        std::move(ci.ctx),
        ios);

    server.set_accept_handler(
        [&](mqtt::server_tls<>::endpoint_t& /*ep*/) {
            BOOST_TEST(false);
        }
    );

    server.set_underlying_connect_timeout(boost::posix_time::seconds(1));
    server.listen();

    // client
    as::ip::tcp::resolver r(ios);
#if BOOST_VERSION < 106600
    as::ip::tcp::resolver::query q(broker_url, std::to_string(broker_tls_port));
    auto it = r.resolve(q);
    as::ip::tcp::resolver::iterator end;
#else  // BOOST_VERSION < 106600
    auto eps = r.resolve(broker_url, std::to_string(broker_tls_port));
    auto it = eps.begin();
    auto end = eps.end();
#endif // BOOST_VERSION < 106600

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "" : path.substr(0, pos + 1);

    as::ssl::context ctx {as::ssl::context::tlsv12};
    ctx.load_verify_file(base + "cacert.pem");
    ctx.set_verify_mode(as::ssl::verify_peer);
    as::ssl::stream<as::ip::tcp::socket> socket(ios, ctx);

    char buf;

    as::async_connect(
        socket.lowest_layer(), it, end,
        [&]
        (boost::system::error_code const& ec, as::ip::tcp::resolver::iterator) {
            if (ec) {
                std::cout << ec.message() << std::endl;
            }
            BOOST_TEST(!ec);
            // intentionally don't call async_handshake (TLS)
            as::async_read(
                socket,
                as::buffer(&buf, 1),
                [&]
                (boost::system::error_code const& ec,
                 std::size_t /*bytes_transferred*/){
                    BOOST_TEST(ec);
                    server.close();
                }
            );
        }
    );

    ios.run();
}

#endif // !defined(MQTT_NO_TLS)

BOOST_AUTO_TEST_SUITE_END()

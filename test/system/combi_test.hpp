// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_COMBI_TEST_HPP)
#define MQTT_TEST_COMBI_TEST_HPP

#include <thread>
#include <future>

#include <mqtt/broker/broker.hpp>

#include "test_settings.hpp"
#include "test_server_no_tls.hpp"
#if defined(MQTT_USE_TLS)
#include "test_server_tls.hpp"
#include "test_ctx_init.hpp"
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)
#include "test_server_no_tls_ws.hpp"
#if defined(MQTT_USE_TLS)
#include "test_server_tls_ws.hpp"
#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)

#include <mqtt/sync_client.hpp>
#include <mqtt/async_client.hpp>

struct sync_type {};
struct async_type {};

template <typename ClientCreator, typename Test>
inline void do_test(
    ClientCreator const& cc,
    Test const& test,
    MQTT_NS::optional<MQTT_NS::protocol_version> v = MQTT_NS::nullopt) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    boost::asio::io_context ioc;
    auto c =
        [&] {
            if (v) {
                return cc(ioc, broker_url, broker_notls_port, v.value());
            }
            else {
                return cc(ioc, broker_url, broker_notls_port);
            }
        } ();
    test(
        ioc,
        c,
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                    b.clear_all_sessions();
                    b.clear_all_retained_topics();
                }
            );
        },
        b
    );
    th.join();
}

#if defined(MQTT_USE_TLS)

template <typename ClientCreator, typename Test>
inline void do_tls_test(
    ClientCreator const& cc,
    Test const& test,
    MQTT_NS::optional<MQTT_NS::protocol_version> v = MQTT_NS::nullopt) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
    MQTT_NS::optional<test_server_tls> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, test_ctx_init(), b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    boost::asio::io_context ioc;
    auto c =
        [&] {
            if (v) {
                return cc(ioc, broker_url, broker_tls_port, v.value());
            }
            else {
                return cc(ioc, broker_url, broker_tls_port);
            }
        } ();
    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? "" : path.substr(0, pos + 1);
    c->get_ssl_context().load_verify_file(base + "cacert.pem");
    test(
        ioc,
        c,
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                    b.clear_all_sessions();
                    b.clear_all_retained_topics();
                }
            );
        },
        b
    );
    th.join();
}

#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)

template <typename ClientCreator, typename Test>
inline void do_ws_test(
    ClientCreator const& cc,
    Test const& test,
    MQTT_NS::optional<MQTT_NS::protocol_version> v = MQTT_NS::nullopt) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
    MQTT_NS::optional<test_server_no_tls_ws> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    boost::asio::io_context ioc;
    auto c =
        [&] {
            if (v) {
                return cc(ioc, broker_url, broker_notls_ws_port, "/", v.value());
            }
            else {
                return cc(ioc, broker_url, broker_notls_ws_port);
            }
        } ();
    test(
        ioc,
        c,
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                    b.clear_all_sessions();
                    b.clear_all_retained_topics();
                }
            );
        },
        b
    );
    th.join();
}

#if defined(MQTT_USE_TLS)

template <typename ClientCreator, typename Test>
inline void do_tls_ws_test(
    ClientCreator const& cc,
    Test const& test,
    MQTT_NS::optional<MQTT_NS::protocol_version> v = MQTT_NS::nullopt) {
    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t b(iocb);
    MQTT_NS::optional<test_server_tls_ws> s;
    std::promise<void> p;
    auto f = p.get_future();
    std::thread th(
        [&] {
            s.emplace(iocb, test_ctx_init(), b);
            p.set_value();
            iocb.run();
        }
    );
    f.wait();
    boost::asio::io_context ioc;
    auto c =
        [&] {
            if (v) {
                return cc(ioc, broker_url, broker_tls_ws_port, "/", v.value());
            }
            else {
                return cc(ioc, broker_url, broker_tls_ws_port);
            }
        } ();
    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? "" : path.substr(0, pos + 1);
    c->get_ssl_context().load_verify_file(base + "cacert.pem");
    test(
        ioc,
        c,
        [&] {
            as::post(
                iocb,
                [&] {
                    s->close();
                    b.clear_all_sessions();
                    b.clear_all_retained_topics();
                }
            );
        },
        b
    );
    th.join();
}

#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)


template <typename Test>
inline void do_combi_test(Test const& test) {
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#if defined(MQTT_USE_WS)
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)
}

template <typename Test>
inline void do_combi_test_sync(Test const& test) {
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_sync_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_sync_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_sync_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_sync_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#if defined(MQTT_USE_WS)
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_sync_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_sync_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_sync_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_sync_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)
}

template <typename Test>
inline void do_combi_test_async(Test const& test) {
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_async_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_test(
        [&](auto&&... args) { return MQTT_NS::make_async_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_async_client(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_async_client(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#if defined(MQTT_USE_WS)
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_async_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_async_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#if defined(MQTT_USE_TLS)
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_async_client_ws(std::forward<decltype(args)>(args)...); },
        test
    );
    do_tls_ws_test(
        [&](auto&&... args) { return MQTT_NS::make_tls_async_client_ws(std::forward<decltype(args)>(args)...); },
        test,
        MQTT_NS::protocol_version::v5
    );
#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)
}

#endif // MQTT_TEST_COMBI_TEST_HPP

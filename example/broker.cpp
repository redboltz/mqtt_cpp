// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/config.hpp>
#include "../test/system/test_server_no_tls.hpp"
#include "../test/system/test_server_no_tls_ws.hpp"
#include "../test/system/test_server_tls.hpp"
#include "../test/system/test_server_tls_ws.hpp"
#include <mqtt/setup_log.hpp>
#include <mqtt/broker/broker.hpp>
#include <boost/program_options.hpp>

#include <fstream>

#if defined(MQTT_USE_TLS)
boost::asio::ssl::context init_ctx()
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);
    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::single_dh_use);
    return ctx;
}

template<typename Server>
void reload_ctx(Server& server, boost::asio::steady_timer& reload_timer,
                std::string const& certificate_filename,
                std::string const& key_filename,
                unsigned int certificate_reload_interval,
                char const* name, bool first_load = true)
{
    MQTT_LOG("mqtt_broker", info) << "Reloading certificates for server " << name;

    if (certificate_reload_interval > 0) {
        reload_timer.expires_after(std::chrono::hours(certificate_reload_interval));
        reload_timer.async_wait(
            [&server, &reload_timer, certificate_filename, key_filename, certificate_reload_interval, name]
            (boost::system::error_code const& e) {

            BOOST_ASSERT(!e || e == boost::asio::error::operation_aborted);

            if (!e) {
                reload_ctx(server, reload_timer, certificate_filename, key_filename, certificate_reload_interval, name, false);
            }
        });
    }

    auto context = init_ctx();

    boost::system::error_code ec;
    context.use_certificate_chain_file(certificate_filename, ec);
    if (ec) {
        auto message = "Failed to load certificate file: " + ec.message();
        if (first_load) {
            throw std::runtime_error(message);
        }

        MQTT_LOG("mqtt_broker", warning) << message;
        return;
    }

    context.use_private_key_file(key_filename, boost::asio::ssl::context::pem, ec);
    if (ec) {
        auto message = "Failed to load private key file: " + ec.message();
        if (first_load) {
            throw std::runtime_error(message);
        }

        MQTT_LOG("mqtt_broker", warning) << message;
        return;
    }

    server.get_ssl_context() = std::move(context);
}

template<typename Server>
void load_ctx(Server& server, boost::asio::steady_timer& reload_timer, boost::program_options::variables_map const& vm, char const* name)
{
    if (vm.count("certificate") == 0 && vm.count("private_key") == 0) {
        throw std::runtime_error("TLS requested but certificate and/or private_key not specified");
    }

    reload_ctx(server, reload_timer,
           vm["certificate"].as<std::string>(),
           vm["private_key"].as<std::string>(),
           vm["certificate_reload_interval"].as<unsigned int>(),
           name, true);
}
#endif // defined(MQTT_USE_TLS)

void run_broker(boost::program_options::variables_map const& vm)
{
    try {
        boost::asio::io_context ioc;
        MQTT_NS::broker::broker_t b(ioc);

        MQTT_NS::optional<test_server_no_tls> s;
        if (vm.count("tcp.port")) {
            s.emplace(ioc, b, vm["tcp.port"].as<std::uint16_t>());
        }

#if defined(MQTT_USE_WS)
        MQTT_NS::optional<test_server_no_tls_ws> s_ws;
        if (vm.count("ws.port")) {
            s_ws.emplace(ioc, b, vm["ws.port"].as<std::uint16_t>());
        }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        MQTT_NS::optional<test_server_tls> s_tls;
        MQTT_NS::optional<boost::asio::steady_timer> s_lts_timer;

        if (vm.count("tls.port")) {
            s_tls.emplace(ioc, init_ctx(), b, vm["tls.port"].as<std::uint16_t>());
            s_lts_timer.emplace(ioc);
            load_ctx(s_tls.value(), s_lts_timer.value(), vm, "TLS");
        }
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
        MQTT_NS::optional<test_server_tls_ws> s_tls_ws;
        MQTT_NS::optional<boost::asio::steady_timer> s_tls_ws_timer;

        if (vm.count("wss.port")) {
            s_tls_ws.emplace(ioc, init_ctx(), b, vm["wss.port"].as<std::uint16_t>());
            s_tls_ws_timer.emplace(ioc);
            load_ctx(s_tls_ws.value(), s_tls_ws_timer.value(), vm, "WSS");
        }
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)

        auto threads =
            [&] () -> std::size_t {
                if (vm.count("threads")) {
                    return vm["threads"].as<std::size_t>();
                }
                return 1;
            } ();
        if (threads == 0) {
            MQTT_LOG("mqtt_broker", error) << "threads should be greater than 0";
            return;
        }
        std::vector<std::thread> ts;
        ts.reserve(threads);
        for (std::size_t i = 0; i != threads; ++i) {
            ts.emplace_back(
                [&ioc] {
                    ioc.run();
                }
            );
        }
        for (auto& t : ts) t.join();
    } catch(std::exception &e) {
        MQTT_LOG("mqtt_broker", error) << e.what();
    }
}

int main(int argc, char **argv) {
    try {
        boost::program_options::options_description desc;

        boost::program_options::options_description general_desc("General options");
        general_desc.add_options()
            ("help", "produce help message")
            ("cfg", boost::program_options::value<std::string>()->default_value("broker.conf"), "Load configuration file")
            ("threads", boost::program_options::value<std::size_t>()->default_value(1), "Number of worker threads")
#if defined(MQTT_USE_LOG)
            ("verbose", boost::program_options::value<unsigned int>()->default_value(1), "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace")
#endif // defined(MQTT_USE_LOG)
            ("certificate", boost::program_options::value<std::string>(), "Certificate file for TLS connections")
            ("private_key", boost::program_options::value<std::string>(), "Private key file for TLS connections")
            ("certificate_reload_interval", boost::program_options::value<unsigned int>()->default_value(0), "Reload interval for the certificate and private key files (hours)\n 0 - Disabled")
            ;

        boost::program_options::options_description notls_desc("TCP Server options");
        notls_desc.add_options()
            ("tcp.port", boost::program_options::value<std::uint16_t>(), "default port (TCP)")
        ;
        desc.add(general_desc).add(notls_desc);

#if defined(MQTT_USE_WS)
        boost::program_options::options_description ws_desc("TCP websocket Server options");
        ws_desc.add_options()
            ("ws.port", boost::program_options::value<std::uint16_t>(), "default port (TCP)")
        ;

        desc.add(ws_desc);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        boost::program_options::options_description tls_desc("TLS Server options");
        tls_desc.add_options()
            ("tls.port", boost::program_options::value<std::uint16_t>(), "default port (TLS)")
        ;

        desc.add(tls_desc);
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS) && defined(MQTT_USE_TLS)
        boost::program_options::options_description tlsws_desc("TLS Websocket Server options");
        tlsws_desc.add_options()
            ("wss.port", boost::program_options::value<std::uint16_t>(), "default port (TLS)")
        ;
        desc.add(tlsws_desc);
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);

        std::string config_file = vm["cfg"].as<std::string>();
        if (!config_file.empty()) {
            std::ifstream input(vm["cfg"].as<std::string>());
            if (input.good()) {
                boost::program_options::store(boost::program_options::parse_config_file(input, desc), vm);
            } else
            {
                std::cerr << "Configuration file '" << config_file << "' not found,  broker doesn't use configuration file." << std::endl;
            }
        }

        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

#if defined(MQTT_USE_LOG)
        switch (vm["verbose"].as<unsigned int>()) {
        case 5:
            MQTT_NS::setup_log(MQTT_NS::severity_level::trace);
            break;
        case 4:
            MQTT_NS::setup_log(MQTT_NS::severity_level::debug);
            break;
        case 3:
            MQTT_NS::setup_log(MQTT_NS::severity_level::info);
            break;
        case 2:
            MQTT_NS::setup_log(MQTT_NS::severity_level::warning);
            break;
        default:
            MQTT_NS::setup_log(MQTT_NS::severity_level::error);
            break;
        case 0:
            MQTT_NS::setup_log(MQTT_NS::severity_level::fatal);
            break;
        }
#else
        MQTT_NS::setup_log();
#endif

        run_broker(vm);
    } catch(std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

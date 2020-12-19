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
boost::asio::ssl::context init_ctx(boost::program_options::variables_map const& vm)
{
    if (vm.count("certificate") == 0 && vm.count("private_key") == 0) {
        throw std::runtime_error("TLS requested but certificate and/or private_key not specified");
    }

    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);
    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::single_dh_use);
    ctx.use_certificate_file(vm["certificate"].as<std::string>(), boost::asio::ssl::context::pem);
    ctx.use_private_key_file(vm["private_key"].as<std::string>(), boost::asio::ssl::context::pem);
    return ctx;
}
#endif // defined(MQTT_USE_TLS)

void run_broker(boost::program_options::variables_map const& vm)
{
    try {
        boost::asio::io_context ioc;
        MQTT_NS::broker::broker_t b(ioc);

        std::unique_ptr<test_server_no_tls> s;
        if (vm.count("tcp.port")) {
            s = std::make_unique<test_server_no_tls>(ioc, b, vm["tcp.port"].as<uint16_t>());
        }

#if defined(MQTT_USE_WS)
        std::unique_ptr<test_server_no_tls_ws> s_ws;
        if (vm.count("ws.port")) {
            s_ws = std::make_unique<test_server_no_tls_ws>(ioc, b, vm["ws.port"].as<uint16_t>());
        }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        std::unique_ptr<test_server_tls> s_tls;
        if (vm.count("tls.port")) {
            s_tls = std::make_unique<test_server_tls>(ioc, init_ctx(vm), b, vm["tls.port"].as<uint16_t>());
        }
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
        std::unique_ptr<test_server_tls_ws> s_tls_ws;
        if (vm.count("wss.port")) {
            s_tls_ws = std::make_unique<test_server_tls_ws>(ioc, init_ctx(vm), b, vm["wss.port"].as<uint16_t>());
        }
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)


        ioc.run();
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
#if defined(MQTT_USE_LOG)
            ("verbose", boost::program_options::value<unsigned int>()->default_value(1), "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace")
#endif // defined(MQTT_USE_LOG)
            ("certificate", boost::program_options::value<std::string>(), "Certificate file for TLS connections")
            ("private_key", boost::program_options::value<std::string>(), "Private key file for TLS connections")
        ;

        boost::program_options::options_description notls_desc("TCP Server options");
        notls_desc.add_options()
            ("tcp.port", boost::program_options::value<uint16_t>(), "default port (TCP)")
        ;
        desc.add(general_desc).add(notls_desc);

#if defined(MQTT_USE_WS)
        boost::program_options::options_description ws_desc("TCP websocket Server options");
        ws_desc.add_options()
            ("ws.port", boost::program_options::value<uint16_t>(), "default port (TCP)")
        ;

        desc.add(ws_desc);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        boost::program_options::options_description tls_desc("TLS Server options");
        tls_desc.add_options()
            ("tls.port", boost::program_options::value<uint16_t>(), "default port (TLS)")
        ;

        desc.add(tls_desc);
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS) && defined(MQTT_USE_TLS)
        boost::program_options::options_description tlsws_desc("TLS Websocket Server options");
        tlsws_desc.add_options()
            ("wss.port", boost::program_options::value<uint16_t>(), "default port (TLS)")
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
            MQTT_NS::setup_log(mqtt::severity_level::trace);
            break;
        case 4:
            MQTT_NS::setup_log(mqtt::severity_level::debug);
            break;
        case 3:
            MQTT_NS::setup_log(mqtt::severity_level::info);
            break;
        case 2:
            MQTT_NS::setup_log(mqtt::severity_level::warning);
            break;
        default:
            MQTT_NS::setup_log(mqtt::severity_level::error);
            break;
        case 0:
            MQTT_NS::setup_log(mqtt::severity_level::fatal);
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

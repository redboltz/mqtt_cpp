// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/config.hpp>
#include "../test/system/test_server_no_tls.hpp"
#include <mqtt/setup_log.hpp>
#include <mqtt/broker/broker.hpp>
#include <boost/program_options.hpp>

void run_broker(boost::program_options::variables_map &vm)
{
    try {
        boost::asio::io_context ioc;
        MQTT_NS::broker::broker_t b(ioc);
        test_server_no_tls s(ioc, b, vm["notls.port"].as<uint16_t>());
        ioc.run();
    } catch(std::exception &e) {
        MQTT_LOG("mqtt_broker", error) << e.what();
    }
}

int main(int argc, char **argv) {
    try {
        boost::program_options::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
#ifdef MQTT_USE_LOG
            ("verbose", boost::program_options::value<unsigned int>()->default_value(1), "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace")
#endif
            ("notls.port", boost::program_options::value<uint16_t>()->default_value(broker_notls_port), "default port (TCP)")
        ;

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

#ifdef MQTT_USE_LOG
        switch(vm["verbose"].as<unsigned int>()) {
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

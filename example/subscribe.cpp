// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <map>

#include <boost/lexical_cast.hpp>

#include <mqtt_client_cpp.hpp>
#include <mqtt/optional.hpp>

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cout << argv[0] << " host port cacert_file sub(0/1)" << std::endl;
        return -1;
    }

    boost::asio::io_service ios;

    std::string host = argv[1];
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[2]);
    std::string cacert = argv[3];

    std::size_t sub = boost::lexical_cast<std::size_t>(argv[4]);

    int count = 0;
    // Create TLS client
    auto c = mqtt::make_tls_client(ios, host, port);

    auto disconnect = [&] {
        //if (++count == 5) c->disconnect();
    };

    // Setup client
    c->set_client_id("subscribe");
    c->set_clean_session(false);
    c->set_ca_cert_file(cacert);

    // Setup handlers
    c->set_connack_handler(
        [&c, sub]
        (bool sp, std::uint8_t connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: "
                      << mqtt::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == mqtt::connect_return_code::accepted) {
                if (sub == 1) {
                    c->subscribe("mqtt_client_cpp/topic2_1", mqtt::qos::at_least_once);
                }
                c->async_publish("topic", "payload", 1, false);

            }
            return true;
        });

    c->set_publish_handler(
        [&]
        (std::uint8_t header,
         boost::optional<std::uint16_t> packet_id,
         std::string topic_name,
         std::string contents){
            std::cout << "publish received. "
                      << "dup: " << std::boolalpha << mqtt::publish::is_dup(header)
                      << " pos: " << mqtt::qos::to_str(mqtt::publish::get_qos(header))
                      << " retain: " << mqtt::publish::is_retain(header) << std::endl;
            if (packet_id)
                std::cout << "packet_id: " << *packet_id << std::endl;
            std::cout << "topic_name: " << topic_name << " contents: " << contents << std::endl;
            return true;
        });

    // Connect
    c->connect();

    ios.run();
}

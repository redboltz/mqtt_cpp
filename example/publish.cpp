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

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << argv[0] << " host port cacert_file" << std::endl;
        return -1;
    }

    boost::asio::io_service ios;

    std::string host = argv[1];
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[2]);
    std::string cacert = argv[3];

    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    int count = 0;
    // Create TLS client
    auto c = mqtt::make_tls_client(ios, host, port);

    auto disconnect = [&] {
        //if (++count == 5) c->disconnect();
    };

    // Setup client
    c->set_client_id("publish");
    c->set_clean_session(true);
    c->set_ca_cert_file(cacert);

    // Setup handlers
    c->set_connack_handler(
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, std::uint8_t connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: "
                      << mqtt::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == mqtt::connect_return_code::accepted) {
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
            }
            return true;
        });

    // Connect
    c->connect();

    ios.run();
}

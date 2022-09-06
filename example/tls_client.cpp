// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <map>

#include <mqtt_client_cpp.hpp>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << argv[0] << " host port cacert_file" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;

    std::string host = argv[1];
    auto port = argv[2];
    std::string cacert = argv[3];

    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    int count = 0;
    // Create TLS client
    auto c = MQTT_NS::make_tls_async_client(ioc, host, port);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);
    c->get_ssl_context().load_verify_file(cacert);

    // Setup handlers
    c->set_connack_handler(
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: "
                      << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;
            return true;
        });
    c->set_close_handler(
        []
        (){
            std::cout << "closed." << std::endl;
        });
    c->set_error_handler(
        []
        (MQTT_NS::error_code ec){
            std::cout << "error: " << ec.message() << std::endl;
        });

    // Connect
    c->async_connect(
        // [optional] checking underlying layer completion code
        []
        (MQTT_NS::error_code ec){
            std::cout << "async_connect callback: " << ec.message() << std::endl;
        }
    );

    ioc.run();
}

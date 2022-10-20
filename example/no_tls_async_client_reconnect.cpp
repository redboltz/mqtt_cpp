// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <map>

#include <mqtt_client_cpp.hpp>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << argv[0] << " host port" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;

    // Create no TLS client
    auto c = MQTT_NS::make_async_client(ioc, argv[1], argv[2]);

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);

    boost::asio::steady_timer tim_connect_wait_limit(ioc);
    boost::asio::steady_timer tim_reconnect_delay(ioc);

    std::function<void()> connect;
    std::function<void()> reconnect;

    connect =
        [&] {
            std::cout << "async_connect" << std::endl;
            c->async_connect(
                [&]
                (MQTT_NS::error_code ec){
                    std::cout << "async_connect callback: " << ec.message() << std::endl;
                    if (ec) reconnect();
                }
            );

            std::cout << "tim_connect_wait_limit set" << std::endl;
            tim_connect_wait_limit.expires_after(std::chrono::seconds(3));
            tim_connect_wait_limit.async_wait(
                [&](boost::system::error_code ec) {
                    std::cout << "tim_connect_wait_limit callback: " << ec.message() << std::endl;
                    if (!ec) {
                        c->async_force_disconnect(
                            [&](boost::system::error_code ec) {
                                std::cout << "async_force_disconnect callback: " << ec.message() << std::endl;
                            }
                        );
                    }
                }
            );
        };
    reconnect =
        [&] {
            std::cout << "tim_reconnect_delay set" << std::endl;
            tim_reconnect_delay.expires_after(std::chrono::seconds(3));
            tim_reconnect_delay.async_wait(
                [&](boost::system::error_code ec) {
                    std::cout << "tim_reconnect_delay callback: " << ec.message() << std::endl;
                    if (!ec) connect();
                }
            );
        };

    // Setup handlers
    c->set_connack_handler(
        [&]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "  Session Present: " << std::boolalpha << sp << std::endl;
            std::cout << "  Connack Return Code: "
                      << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                tim_connect_wait_limit.cancel();
            }
            else {
                reconnect();
            }
        });
    c->set_close_handler(
        [&]
        (){
            std::cout << "closed." << std::endl;
            reconnect();
        });
    c->set_error_handler(
        [&]
        (MQTT_NS::error_code ec){
            std::cout << "error: " << ec.message() << std::endl;
            reconnect();
        });

    connect();
    ioc.run();
}

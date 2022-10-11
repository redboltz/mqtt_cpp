// Copyright Wouter van Kleunen 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <mqtt_client_cpp.hpp>

// This example shows the client reconnecting to the broker
//
// The client connects to the server and published a total of 100 messages,
// if the connection was lost a new connection will be established.
//
// Important: please note that messages are only republished to the broker if
// the broker still has an active session for this client. If it does not,
// the client will start with a new session and not resend offline stored messages
//

template <typename C>
void reconnect_client(boost::asio::steady_timer& timer, C& c)
{
    std::cout << "Start reconnect timer" << std::endl;

    // Set an expiry time relative to now.
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait([&timer, &c](const boost::system::error_code& error) {
        if (error != boost::asio::error::operation_aborted) {
            std::cout << "Reconnect now !!" << std::endl;

            // Connect
            c->async_connect(
                // [optional] checking underlying layer completion code
                [&timer, &c]
                (MQTT_NS::error_code ec){
                    std::cout << "async_connect callback: " << ec.message() << std::endl;
                    if (ec && ec != boost::asio::error::operation_aborted) {
                        reconnect_client(timer, c);
                    }
                }
            );
        }
    });
}

template <typename C>
void publish_message(boost::asio::steady_timer& timer, C& c, unsigned int& packet_counter)
{
    // Publish a message every 5 seconds
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait([&timer, &c, &packet_counter](boost::system::error_code const& error) {
        if (error != boost::asio::error::operation_aborted) {
            c->async_publish(
                MQTT_NS::allocate_buffer("mqtt_client_cpp/topic1"),
                MQTT_NS::allocate_buffer("packet #" + std::to_string(packet_counter)),
                MQTT_NS::qos::exactly_once,
                // [optional] checking async_publish completion code
                []
                (MQTT_NS::error_code ec){
                    std::cout << "async_publish callback: " << ec.message() << std::endl;
                }
            );

            publish_message(timer, c, packet_counter);
        }
    });
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << argv[0] << " hostname port" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;

    boost::asio::steady_timer publish_timer(ioc);
    boost::asio::steady_timer reconnect_timer(ioc);
    unsigned int packet_counter = 1;

    auto c = MQTT_NS::make_async_client(ioc, argv[1], argv[2]);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    auto disconnect = [&]() {
        publish_timer.cancel();
        reconnect_timer.cancel();
        c->async_disconnect(
            // [optional] checking async_disconnect completion code
            []
            (MQTT_NS::error_code ec){
                std::cout << "async_disconnect callback: " << ec.message() << std::endl;
            }
        );
    };

    c->set_client_id("reconnect_client");
    c->set_clean_session(false);

    // Setup handlers
    c->set_connack_handler(
        [&]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: " << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;

            c->async_subscribe(
                "mqtt_client_cpp/topic1",
                MQTT_NS::qos::exactly_once,
                // [optional] checking async_subscribe completion code
                []
                (MQTT_NS::error_code ec){
                    std::cout << "async_subscribe callback: " << ec.message() << std::endl;
                }
                );

            publish_message(publish_timer, c, packet_counter);
        });
    c->set_close_handler(
        []
        (){
            std::cout << "closed." << std::endl;
        });

    c->set_error_handler(
        [&]
        (MQTT_NS::error_code ec){
            std::cout << "error: " << ec.message() << std::endl;
            reconnect_client(reconnect_timer, c);
        });

    c->set_puback_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "puback received. packet_id: " << packet_id << std::endl;
        });
    c->set_pubrec_handler(
        []
        (packet_id_t packet_id){
            std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
        });
    c->set_pubcomp_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;

            if (packet_counter == 100) {
                disconnect();
            }

            packet_counter += 1;
        });
    c->set_suback_handler(
        [&]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results){
            std::cout << "suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : results) {
                std::cout << "[client] subscribe result: " << e << std::endl;
            }
        });
    c->set_publish_handler(
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic_name,
         MQTT_NS::buffer contents){
            std::cout << "publish received."
                      << " dup: "    << pubopts.get_dup()
                      << " qos: "    << pubopts.get_qos()
                      << " retain: " << pubopts.get_retain() << std::endl;
            if (packet_id)
                std::cout << "packet_id: " << *packet_id << std::endl;
            std::cout << "topic_name: " << topic_name << std::endl;
            std::cout << "contents: " << contents << std::endl;

        });

    // Connect
    c->async_connect(
        // Initial connect should succeed, otherwise we shutdown
        [&]
        (MQTT_NS::error_code ec) {
            std::cout << "async_connect callback: " << ec.message() << std::endl;
        }
    );

    ioc.run();

    return 0;
}

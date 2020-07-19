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

    boost::asio::io_context ioc;

    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    int count = 0;
    // Create no TLS client
    auto c = MQTT_NS::make_async_client(ioc, argv[1], argv[2]);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    auto disconnect = [&] {
        if (++count == 5) {
            c->async_disconnect(
                // [optional] checking async_disconnect completion code
                []
                (MQTT_NS::error_code ec){
                    std::cout << "async_disconnect callback: " << ec.message() << std::endl;
                }
            );
        }
    };

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);

    // Setup handlers
    c->set_connack_handler(
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: "
                      << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                pid_sub1 = c->acquire_unique_packet_id();
                c->async_subscribe(
                    pid_sub1,
                    "mqtt_client_cpp/topic1",
                    MQTT_NS::qos::at_most_once,
                    // [optional] checking async_subscribe completion code
                    []
                    (MQTT_NS::error_code ec){
                        std::cout << "async_subscribe callback: " << ec.message() << std::endl;
                    }
                );
                pid_sub2 = c->acquire_unique_packet_id();
                c->async_subscribe(
                    pid_sub2,
                    std::vector<std::tuple<std::string, MQTT_NS::subscribe_options>>
                    {
                        { "mqtt_client_cpp/topic2_1", MQTT_NS::qos::at_least_once },
                        { "mqtt_client_cpp/topic2_2", MQTT_NS::qos::exactly_once }
                    },
                    // [optional] checking async_subscribe completion code
                    []
                    (MQTT_NS::error_code ec){
                        std::cout << "async_subscribe callback: " << ec.message() << std::endl;
                    }

                );
            }
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
    c->set_puback_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "puback received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_pubrec_handler(
        []
        (packet_id_t packet_id){
            std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
            return true;
        });
    c->set_pubcomp_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_suback_handler(
        [&]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results){
            std::cout << "suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : results) {
                std::cout << "[client] subscribe result: " << e << std::endl;
            }
            if (packet_id == pid_sub1) {
                c->async_publish(
                    "mqtt_client_cpp/topic1",
                    "test1",
                    MQTT_NS::qos::at_most_once,
                    // [optional] checking async_publish completion code
                    []
                    (MQTT_NS::error_code ec){
                        std::cout << "async_publish callback: " << ec.message() << std::endl;
                    }
                );
            }
            else if (packet_id == pid_sub2) {
                c->async_publish(
                    "mqtt_client_cpp/topic2_1",
                    "test2_1",
                    MQTT_NS::qos::at_least_once,
                    // [optional] checking async_publish completion code
                    []
                    (MQTT_NS::error_code ec){
                        std::cout << "async_publish callback: " << ec.message() << std::endl;
                    }
                );
                c->async_publish(
                    "mqtt_client_cpp/topic2_2",
                    "test2_2",
                    MQTT_NS::qos::exactly_once,
                    // [optional] checking async_publish completion code
                    []
                    (MQTT_NS::error_code ec){
                        std::cout << "async_publish callback: " << ec.message() << std::endl;
                    }
                );
            }
            return true;
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
            disconnect();
            return true;
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

// Copyright Takatoshi Kondo 2017
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

    boost::asio::io_context ioc;

    std::string host = argv[1];
    auto port = boost::lexical_cast<std::uint16_t>(argv[2]);
    std::string cacert = argv[3];

    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    int count = 0;
    // Create TLS client
    auto c = MQTT_NS::make_tls_sync_client_ws(ioc, host, port);

    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);
    c->get_ssl_context().load_verify_file(cacert);

    // Setup handlers
    c->set_connack_handler(
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "Connack handler called" << std::endl;
            std::cout << "Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "Connack Return Code: "
                      << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                pid_sub1 = c->subscribe("mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once);
                pid_sub2 = c->subscribe(
                    {
                        { "mqtt_client_cpp/topic2_1", MQTT_NS::qos::at_least_once },
                        { "mqtt_client_cpp/topic2_2", MQTT_NS::qos::exactly_once }
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
        (boost::system::error_code const& ec){
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
        [&]
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
                c->publish("mqtt_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once);
            }
            else if (packet_id == pid_sub2) {
                c->publish("mqtt_client_cpp/topic2_1", "test2_1", MQTT_NS::qos::at_least_once);
                c->publish("mqtt_client_cpp/topic2_2", "test2_2", MQTT_NS::qos::exactly_once);
            }
            return true;
        });
    c->set_publish_handler(
        [&]
        (bool is_dup,
         MQTT_NS::qos qos_value,
         bool is_retain,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic_name,
         MQTT_NS::buffer contents){
            std::cout << "publish received. "
                      << "dup: " << std::boolalpha << is_dup
                      << " qos: " << qos_value
                      << " retain: " << is_retain << std::endl;
            if (packet_id)
                std::cout << "packet_id: " << *packet_id << std::endl;
            std::cout << "topic_name: " << topic_name << std::endl;
            std::cout << "contents: " << contents << std::endl;
            disconnect();
            return true;
        });

    // Connect
    c->connect();

    ioc.run();
}

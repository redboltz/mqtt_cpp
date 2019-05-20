// Copyright Takatoshi Kondo 2019
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

    boost::asio::io_service ios;

    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    int count = 0;
    // Create no TLS client
    // You can set the protocol_version to connect. If you don't set it, v3_1_1 is used.
    auto c = mqtt::make_sync_client(ios, argv[1], argv[2], mqtt::protocol_version::v5);
    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);

    // Setup handlers
    c->set_v5_connack_handler( // use v5 handler
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout << "[client] Connack handler called" << std::endl;
            std::cout << "[client] Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "[client] Connect Reason Code: "
                      << static_cast<int>(reason_code) << std::endl;
            if (reason_code == mqtt::v5::reason_code::success) {
                pid_sub1 = c->subscribe("mqtt_client_cpp/topic1", mqtt::qos::at_most_once);
                pid_sub2 = c->subscribe("mqtt_client_cpp/topic2_1", mqtt::qos::at_least_once,
                                       "mqtt_client_cpp/topic2_2", mqtt::qos::exactly_once);
            }
            return true;
        });
    c->set_close_handler( // this handler doesn't depend on MQTT protocol version
        []
        (){
            std::cout << "[client] closed." << std::endl;
        });
    c->set_error_handler( // this handler doesn't depend on MQTT protocol version
        []
        (boost::system::error_code const& ec){
            std::cout << "[client] error: " << ec.message() << std::endl;
        });
    c->set_v5_puback_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout <<
                "[client] puback received. packet_id: " << packet_id <<
                " reason_code: " << static_cast<int>(reason_code) << std::endl;
            disconnect();
            return true;
        });
    c->set_v5_pubrec_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout <<
                "[client] pubrec received. packet_id: " << packet_id <<
                " reason_code: " << static_cast<int>(reason_code) << std::endl;
            return true;
        });
    c->set_v5_pubcomp_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout <<
                "[client] pubcomp received. packet_id: " << packet_id <<
                " reason_code: " << static_cast<int>(reason_code) << std::endl;
            disconnect();
            return true;
        });
    c->set_v5_suback_handler( // use v5 handler
        [&]
        (packet_id_t packet_id,
         std::vector<std::uint8_t> reasons,
         std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout << "[client] suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : reasons) {
                switch (e) {
                case mqtt::v5::reason_code::granted_qos_0:
                    std::cout << "[client] subscribe success: qos0" << std::endl;
                    break;
                case mqtt::v5::reason_code::granted_qos_1:
                    std::cout << "[client] subscribe success: qos1" << std::endl;
                    break;
                case mqtt::v5::reason_code::granted_qos_2:
                    std::cout << "[client] subscribe success: qos2" << std::endl;
                    break;
                default:
                    std::cout << "[client] subscribe failed: reason_code = " << static_cast<int>(e) << std::endl;
                    break;
                }
            }
            if (packet_id == pid_sub1) {
                c->publish_at_most_once("mqtt_client_cpp/topic1", "test1");
            }
            else if (packet_id == pid_sub2) {
                c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
                c->publish_exactly_once("mqtt_client_cpp/topic2_2", "test2_2");
            }
            return true;
        });
    c->set_v5_publish_handler( // use v5 handler
        [&]
        (std::uint8_t header,
         mqtt::optional<packet_id_t> packet_id,
         std::string topic_name,
         std::string contents,
         std::vector<mqtt::v5::property_variant> /*props*/){
            std::cout << "[client] publish received. "
                      << "dup: " << std::boolalpha << mqtt::publish::is_dup(header)
                      << " pos: " << mqtt::qos::to_str(mqtt::publish::get_qos(header))
                      << " retain: " << mqtt::publish::is_retain(header) << std::endl;
            if (packet_id)
                std::cout << "[client] packet_id: " << *packet_id << std::endl;
            std::cout << "[client] topic_name: " << topic_name << std::endl;
            std::cout << "[client] contents: " << contents << std::endl;
            disconnect();
            return true;
        });

    // Connect
    c->connect();

    ios.run();
}

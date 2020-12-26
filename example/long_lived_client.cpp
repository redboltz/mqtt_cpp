// Copyright Wouter van Kleunen 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <regex>

#include <mqtt_client_cpp.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// This example shows the client reconnecting to the broker
//
// The client connects to the server and published a total of 100 messages,
// if the connection was lost a new connection will be established.
//
// Important: please note that messages are only republished to the broker if
// the broker still has an active session for this client. If it does not,
// the client will start with a new session and not resend offline stored messages
//
// It is possible to connect to the server using (mqtt, mqtts, ws or wss) as follows:
// long_lived mqtt://example.com
// long_lived mqtts://example.com
// long_lived ws://example.com
// long_lived wss://example.com
//
// Server certificate is validated using the file cacert.pem


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
void publish_message(boost::asio::steady_timer& timer, C& c, int packet_counter)
{
    // Publish a message every 5 seconds
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait([&timer, &c, packet_counter](boost::system::error_code const& error) {
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

            publish_message(timer, c, packet_counter + 1);
        }
    });
}

template <typename C>
void setup_client(C& c, boost::asio::steady_timer& publish_timer, boost::asio::steady_timer& reconnect_timer)
{
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

    // Setup client
    boost::uuids::name_generator_sha1 gen(boost::uuids::ns::dns());
    boost::uuids::uuid mqtt_uuid = gen("mqtt.org");

    c->set_client_id(boost::lexical_cast<std::string>(mqtt_uuid));
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

            publish_message(publish_timer, c, 1);
            return true;
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
            return true;
        });
    c->set_suback_handler(
        [&]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results){
            std::cout << "suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : results) {
                std::cout << "[client] subscribe result: " << e << std::endl;
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

            if (contents == "packet #100") {
                disconnect();
            }

            return true;
        });

    // Connect
    c->async_connect(
        // Initial connect should succeed, otherwise we shutdown
        [&]
        (MQTT_NS::error_code ec) {
            std::cout << "async_connect callback: " << ec.message() << std::endl;
        }
    );
}

template <typename C>
void setup_tls_client(C& c, boost::asio::steady_timer& publish_timer, boost::asio::steady_timer& reconnect_timer)
{
    auto cacert = "cacert.pem";

    c->get_ssl_context().load_verify_file(cacert);
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    SSL_CTX_set_keylog_callback(
        c->get_ssl_context().native_handle(),
        [](SSL const*, char const* line) {
            std::cout << line << std::endl;
        }
        );
#endif // OPENSSL_VERSION_NUMBER >= 0x10101000L

    setup_client(c, publish_timer, reconnect_timer);
}

void show_help(char **argv)
{
    std::cout << argv[0] << " uri" << std::endl;
    std::cout << "Example URI: " << std::endl;
    std::cout << "  mqtt://example.com" << std::endl;
    std::cout << "  mqtt://example.com:12345" << std::endl;
    std::cout << "  mqtts://example.com" << std::endl;
    std::cout << "  ws://example.com" << std::endl;
    std::cout << "  wss://example.com" << std::endl;
}

std::map<std::string, uint16_t> default_ports =
    {
        { "mqtt", 1883},
        { "mqtts", 8883},
        { "ws", 10080},
        { "wss", 10443}
};


int main(int argc, char** argv) {
    if (argc != 2) {
        show_help(argv);
        return -1;
    }

    MQTT_NS::setup_log();

    // Parse URI using regex
    constexpr std::size_t expected_match_size = 8;
    std::regex uri_regex{
        "((mqtt|mqtt|ws|ws)(s)?)(://)([a-zA-Z0-9\\-.]+)(:([0-9]+))?"
    };

    std::string uri(argv[1]);
    std::smatch match;
    if (!std::regex_match(uri, match, uri_regex) || match.size() != expected_match_size) {
        show_help(argv);
        return -1;
    }

    std::string protocol = match[1];
    std::string hostname = match[5];
    std::string port_str = match[7];

    auto port_iter = default_ports.find(protocol);
    if (port_iter == default_ports.end()) {
        std::cout << "Invalid protocol specified: " << protocol << std::endl;
        return -1;
    }

    uint16_t port = (port_str.empty() ? port_iter->second : boost::lexical_cast<uint16_t>(port_str));

    boost::asio::io_context ioc;

    boost::asio::steady_timer publish_timer(ioc);
    boost::asio::steady_timer reconnect_timer(ioc);

    if (protocol == "mqtt") {
        auto c = MQTT_NS::make_async_client(ioc, hostname, port);
        setup_client(c, publish_timer, reconnect_timer);
        ioc.run();
    }

    if (protocol == "mqtts") {
        auto c = MQTT_NS::make_tls_async_client(ioc, hostname, port);
        setup_tls_client(c, publish_timer, reconnect_timer);
        ioc.run();
    }

    if (protocol == "ws") {
        auto c = MQTT_NS::make_async_client_ws(ioc, hostname, port);
        setup_client(c, publish_timer, reconnect_timer);
        ioc.run();
    }

    if (protocol == "wss") {
        auto c = MQTT_NS::make_tls_async_client_ws(ioc, hostname, port);
        setup_tls_client(c, publish_timer, reconnect_timer);
        ioc.run();
    }

    return 0;
}

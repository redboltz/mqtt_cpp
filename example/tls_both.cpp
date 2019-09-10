// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// no_tls client and server

#include <iostream>
#include <iomanip>
#include <map>

#include <mqtt_client_cpp.hpp>

#include <boost/lexical_cast.hpp>

template <typename Client, typename Disconnect>
void client_proc(
    Client& c,
    std::uint16_t& pid_sub1,
    std::uint16_t& pid_sub2,
    Disconnect const& disconnect) {

    using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;

    // Setup client
    c->set_client_id("cid1");
    c->set_clean_session(true);

    // Setup handlers
    c->set_connack_handler(
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, MQTT_NS::connect_return_code connack_return_code){
            std::cout << "[client] Connack handler called" << std::endl;
            std::cout << "[client] Clean Session: " << std::boolalpha << sp << std::endl;
            std::cout << "[client] Connack Return Code: "
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
            std::cout << "[client] closed." << std::endl;
        });
    c->set_error_handler(
        []
        (boost::system::error_code const& ec){
            std::cout << "[client] error: " << ec.message() << std::endl;
        });
    c->set_puback_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "[client] puback received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_pubrec_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "[client] pubrec received. packet_id: " << packet_id << std::endl;
            return true;
        });
    c->set_pubcomp_handler(
        [&]
        (packet_id_t packet_id){
            std::cout << "[client] pubcomp received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_suback_handler(
        [&]
        (packet_id_t packet_id, std::vector<MQTT_NS::optional<MQTT_NS::suback_reason_code>> results){
            std::cout << "[client] suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : results) {
                if (e) {
                    std::cout << "[client] subscribe success: " << static_cast<MQTT_NS::qos>(*e) << std::endl;
                }
                else {
                    std::cout << "[client] subscribe failed" << std::endl;
                }
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
        (std::uint8_t header,
         MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::buffer topic_name,
         MQTT_NS::buffer contents){
            std::cout << "[client] publish received. "
                      << "dup: " << std::boolalpha << MQTT_NS::publish::is_dup(header)
                      << " qos: " << MQTT_NS::publish::get_qos(header)
                      << " retain: " << MQTT_NS::publish::is_retain(header) << std::endl;
            if (packet_id)
                std::cout << "[client] packet_id: " << *packet_id << std::endl;
            std::cout << "[client] topic_name: " << topic_name << std::endl;
            std::cout << "[client] contents: " << contents << std::endl;
            disconnect();
            return true;
        });

    // Connect
    c->connect();
}

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <mqtt_server_cpp.hpp>

namespace mi = boost::multi_index;

using con_t = MQTT_NS::server_tls<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;
using con_wp_t = std::weak_ptr<con_t>;

struct sub_con {
    sub_con(MQTT_NS::buffer topic, con_sp_t con, MQTT_NS::qos qos_value)
        :topic(std::move(topic)), con(std::move(con)), qos_value(qos_value) {}
    MQTT_NS::buffer topic;
    con_sp_t con;
    MQTT_NS::qos qos_value;
};

struct tag_topic {};
struct tag_con {};

using mi_sub_con = mi::multi_index_container<
    sub_con,
    mi::indexed_by<
        mi::ordered_non_unique<
            mi::tag<tag_topic>,
            BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
        >,
        mi::ordered_non_unique<
            mi::tag<tag_con>,
            BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
        >
    >
>;


inline void close_proc(std::set<con_sp_t>& cons, mi_sub_con& subs, con_sp_t const& con) {
    cons.erase(con);

    auto& idx = subs.get<tag_con>();
    auto r = idx.equal_range(con);
    idx.erase(r.first, r.second);
}

template <typename Server>
void server_proc(Server& s, std::set<con_sp_t>& connections, mi_sub_con& subs) {
    s.set_error_handler(
        [](boost::system::error_code const& ec) {
            std::cout << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler(
        [&](con_sp_t spep) {
            using packet_id_t = con_sp_t::element_type::packet_id_t;
            std::cout << "[server]accept" << std::endl;
            spep->start_session(
                [&, spep] // keeping ep's lifetime as sp until session finished
                (boost::system::error_code const& ec) {
                    std::cout << "[server]session end: " << ec.message() << std::endl;
                    s.close();
                }
            );

            // set connection (lower than MQTT) level handlers
            spep->set_close_handler(
                [&, spep]
                (){
                    std::cout << "[server]closed." << std::endl;
                    close_proc(connections, subs, spep);
                });
            spep->set_error_handler(
                [&, spep]
                (boost::system::error_code const& ec){
                    std::cout << "[server]error: " << ec.message() << std::endl;
                    close_proc(connections, subs, spep);
                });

            // set MQTT level handlers
            spep->set_connect_handler(
                [&, spep]
                (MQTT_NS::buffer client_id,
                 MQTT_NS::optional<MQTT_NS::buffer> username,
                 MQTT_NS::optional<MQTT_NS::buffer> password,
                 MQTT_NS::optional<MQTT_NS::will>,
                 bool clean_session,
                 std::uint16_t keep_alive) {
                    using namespace MQTT_NS::literals;
                    std::cout << "[server]client_id    : " << client_id << std::endl;
                    std::cout << "[server]username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    std::cout << "[server]password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    std::cout << "[server]clean_session: " << std::boolalpha << clean_session << std::endl;
                    std::cout << "[server]keep_alive   : " << keep_alive << std::endl;
                    connections.insert(spep);
                    spep->connack(false, MQTT_NS::connect_return_code::accepted);
                    return true;
                }
            );
            spep->set_disconnect_handler(
                [&, spep]
                (){
                    std::cout << "[server]disconnect received." << std::endl;
                    close_proc(connections, subs, spep);
                });
            spep->set_puback_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "[server]puback received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubrec_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "[server]pubrec received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubrel_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "[server]pubrel received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubcomp_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "[server]pubcomp received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_publish_handler(
                [&]
                (std::uint8_t header,
                 MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents){
                    MQTT_NS::qos qos_value = MQTT_NS::publish::get_qos(header);
                    bool retain = MQTT_NS::publish::is_retain(header);
                    std::cout << "[server]publish received."
                              << " dup: " << std::boolalpha << MQTT_NS::publish::is_dup(header)
                              << " qos: " << qos_value
                              << " retain: " << retain << std::endl;
                    if (packet_id)
                        std::cout << "[server]packet_id: " << *packet_id << std::endl;
                    std::cout << "[server]topic_name: " << topic_name << std::endl;
                    std::cout << "[server]contents: " << contents << std::endl;
                    auto const& idx = subs.get<tag_topic>();
                    auto r = idx.equal_range(topic_name);
                    for (; r.first != r.second; ++r.first) {
                        r.first->con->publish(
                            boost::asio::buffer(topic_name),
                            boost::asio::buffer(contents),
                            std::make_pair(topic_name, contents),
                            std::min(r.first->qos_value, qos_value),
                            retain
                        );
                    }
                    return true;
                });
            spep->set_subscribe_handler(
                [&, spep]
                (packet_id_t packet_id,
                 std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries) {
                    std::cout << "[server]subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<MQTT_NS::suback_reason_code> res;
                    res.reserve(entries.size());
                    for (auto const& e : entries) {
                        MQTT_NS::buffer topic = std::get<0>(e);
                        MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                        std::cout << "[server]topic: " << topic  << " qos: " << qos_value << std::endl;
                        res.emplace_back(static_cast<MQTT_NS::suback_reason_code>(qos_value));
                        subs.emplace(std::move(topic), spep, qos_value);
                    }
                    spep->suback(packet_id, res);
                    return true;
                }
            );
            spep->set_unsubscribe_handler(
                [&, spep]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::buffer> topics) {
                    std::cout << "[server]unsubscribe received. packet_id: " << packet_id << std::endl;
                    for (auto const& topic : topics) {
                        subs.erase(topic);
                    }
                    spep;
                    return true;
                }
            );
        }
    );

    s.listen();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << " port" << std::endl;
        return -1;
    }

    std::string path = argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? "./" : path.substr(0, pos + 1);
    boost::asio::io_context ioc;
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);

    // server
    boost::asio::ssl::context  ctx(boost::asio::ssl::context::tlsv12);
    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::single_dh_use);
    ctx.use_certificate_file(base + "server.crt.pem", boost::asio::ssl::context::pem);
    ctx.use_private_key_file(base + "server.key.pem", boost::asio::ssl::context::pem);

    auto s = MQTT_NS::server_tls<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            port
        ),
        std::move(ctx),
        ioc
    );
    std::set<con_sp_t> connections;
    mi_sub_con subs;
    server_proc(s, connections, subs);


    // client
    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    auto c = MQTT_NS::make_tls_sync_client(ioc, "localhost", port);
    c->set_ca_cert_file(base + "cacert.pem");

    int count = 0;
    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };
    client_proc(c, pid_sub1, pid_sub2, disconnect);


    ioc.run();
}

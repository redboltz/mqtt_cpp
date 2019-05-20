// Copyright Takatoshi Kondo 2019
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
}

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <mqtt_server_cpp.hpp>

namespace mi = boost::multi_index;

using con_t = mqtt::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;

struct sub_con {
    sub_con(std::string const& topic, con_sp_t const& con, std::uint8_t qos)
        :topic(topic), con(con), qos(qos) {}
    std::string topic;
    con_sp_t con;
    std::uint8_t qos;
};

struct tag_topic {};
struct tag_con {};

using mi_sub_con = mi::multi_index_container<
    sub_con,
    mi::indexed_by<
        mi::ordered_non_unique<
            mi::tag<tag_topic>,
            BOOST_MULTI_INDEX_MEMBER(sub_con, std::string, topic)
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
    s.set_error_handler( // this handler doesn't depend on MQTT protocol version
        [](boost::system::error_code const& ec) {
            std::cout << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler( // this handler doesn't depend on MQTT protocol version
        [&](con_t& ep) {
            using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
            std::cout << "[server] accept" << std::endl;
            auto sp = ep.shared_from_this();
            ep.start_session(
                [&, sp] // keeping ep's lifetime as sp until session finished
                (boost::system::error_code const& ec) {
                    std::cout << "[server] session end: " << ec.message() << std::endl;
                    s.close();
                }
            );

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler( // this handler doesn't depend on MQTT protocol version
                [&]
                (){
                    std::cout << "[server] closed." << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });
            ep.set_error_handler( // this handler doesn't depend on MQTT protocol version
                [&]
                (boost::system::error_code const& ec){
                    std::cout << "[server] error: " << ec.message() << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });

            // set MQTT level handlers
            ep.set_v5_connect_handler( // use v5 handler
                [&]
                (std::string const& client_id,
                 mqtt::optional<std::string> const& username,
                 mqtt::optional<std::string> const& password,
                 mqtt::optional<mqtt::will>,
                 bool clean_session,
                 std::uint16_t keep_alive,
                 std::vector<mqtt::v5::property_variant> /*props*/){
                    std::cout << "[server] client_id    : " << client_id << std::endl;
                    std::cout << "[server] username     : " << (username ? username.value() : "none") << std::endl;
                    std::cout << "[server] password     : " << (password ? password.value() : "none") << std::endl;
                    std::cout << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
                    std::cout << "[server] keep_alive   : " << keep_alive << std::endl;
                    connections.insert(ep.shared_from_this());
                    ep.connack(false, mqtt::connect_return_code::accepted);
                    return true;
                }
            );
            ep.set_v5_disconnect_handler( // use v5 handler
                [&]
                (std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/) {
                    std::cout <<
                        "[server] disconnect received." <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });
            ep.set_v5_puback_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
                    std::cout <<
                        "[server] puback received. packet_id: " << packet_id <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    return true;
                });
            ep.set_v5_pubrec_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
                    std::cout <<
                        "[server] pubrec received. packet_id: " << packet_id <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    return true;
                });
            ep.set_v5_pubrel_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
                    std::cout <<
                        "[server] pubrel received. packet_id: " << packet_id <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    return true;
                });
            ep.set_v5_pubcomp_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, std::uint8_t reason_code, std::vector<mqtt::v5::property_variant> /*props*/){
                    std::cout <<
                        "[server] pubcomp received. packet_id: " << packet_id <<
                        " reason_code: " << static_cast<int>(reason_code) << std::endl;
                    return true;
                });
            ep.set_v5_publish_handler( // use v5 handler
                [&]
                (std::uint8_t header,
                 mqtt::optional<packet_id_t> packet_id,
                 std::string topic_name,
                 std::string contents,
                 std::vector<mqtt::v5::property_variant> /*props*/){
                    std::uint8_t qos = mqtt::publish::get_qos(header);
                    bool retain = mqtt::publish::is_retain(header);
                    std::cout << "[server] publish received."
                              << " dup: " << std::boolalpha << mqtt::publish::is_dup(header)
                              << " qos: " << mqtt::qos::to_str(qos)
                              << " retain: " << retain << std::endl;
                    if (packet_id)
                        std::cout << "[server] packet_id: " << *packet_id << std::endl;
                    std::cout << "[server] topic_name: " << topic_name << std::endl;
                    std::cout << "[server] contents: " << contents << std::endl;
                    auto const& idx = subs.get<tag_topic>();
                    auto r = idx.equal_range(topic_name);
                    for (; r.first != r.second; ++r.first) {
                        r.first->con->publish(
                            topic_name,
                            contents,
                            std::min(r.first->qos, qos),
                            retain
                        );
                    }
                    return true;
                });
            ep.set_v5_subscribe_handler( // use v5 handler
                [&]
                (packet_id_t packet_id,
                 std::vector<std::tuple<std::string, std::uint8_t>> entries,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    std::cout << "[server] subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<std::uint8_t> res;
                    res.reserve(entries.size());
                    for (auto const& e : entries) {
                        std::string const& topic = std::get<0>(e);
                        std::uint8_t qos = std::get<1>(e);
                        std::cout << "[server] topic: " << topic  << " qos: " << static_cast<int>(qos) << std::endl;
                        res.emplace_back(qos);
                        subs.emplace(topic, ep.shared_from_this(), qos);
                    }
                    ep.suback(packet_id, res);
                    return true;
                }
            );
            ep.set_v5_unsubscribe_handler( // use v5 handler
                [&]
                (packet_id_t packet_id,
                 std::vector<std::string> topics,
                 std::vector<mqtt::v5::property_variant> /*props*/) {
                    std::cout << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
                    for (auto const& topic : topics) {
                        subs.erase(topic);
                    }
                    ep.unsuback(packet_id);
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

    boost::asio::io_service ios;
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);

    // server
    auto s = mqtt::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        ios
    );

    // You can set a specific protocol_version if you want to limit accepting version.
    // Otherwise, all protocols are accepted.
    s.set_protocol_version(mqtt::protocol_version::v5);

    std::set<con_sp_t> connections;
    mi_sub_con subs;
    server_proc(s, connections, subs);


    // client
    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    // You can set the protocol_version to connect. If you don't set it, v3_1_1 is used.
    auto c = mqtt::make_sync_client(ios, "localhost", port, mqtt::protocol_version::v5);

    int count = 0;
    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };
    client_proc(c, pid_sub1, pid_sub2, disconnect);


    ios.run();
}

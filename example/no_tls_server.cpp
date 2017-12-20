// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <set>

#include <boost/lexical_cast.hpp>
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

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << " port" << std::endl;
        return -1;
    }
    boost::asio::io_service ios;

    auto s = mqtt::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        ios
    );

    s.set_error_handler(
        [](boost::system::error_code const& ec) {
            std::cout << "error: " << ec.message() << std::endl;
        }
    );

    std::set<con_sp_t> connections;
    mi_sub_con subs;

    s.set_accept_handler(
        [&](con_t& ep) {
            std::cout << "accept" << std::endl;
            auto sp = ep.shared_from_this();
            ep.start_session(
                [sp] // keeping ep's lifetime as sp until session finished
                (boost::system::error_code const& ec) {
                    std::cout << "session end: " << ec.message() << std::endl;
                }
            );

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler(
                [&]
                (){
                    std::cout << "closed." << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });
            ep.set_error_handler(
                [&]
                (boost::system::error_code const& ec){
                    std::cout << "error: " << ec.message() << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });

            // set MQTT level handlers
            ep.set_connect_handler(
                [&]
                (std::string const& client_id,
                 boost::optional<std::string> const& username,
                 boost::optional<std::string> const& password,
                 boost::optional<mqtt::will>,
                 bool clean_session,
                 std::uint16_t keep_alive) {
                    std::cout << "client_id    : " << client_id << std::endl;
                    std::cout << "username     : " << (username ? username.get() : "none") << std::endl;
                    std::cout << "password     : " << (password ? password.get() : "none") << std::endl;
                    std::cout << "clean_session: " << std::boolalpha << clean_session << std::endl;
                    std::cout << "keep_alive   : " << keep_alive << std::endl;
                    connections.insert(ep.shared_from_this());
                    ep.connack(false, mqtt::connect_return_code::accepted);
                    return true;
                }
            );
            ep.set_disconnect_handler(
                [&]
                (){
                    std::cout << "disconnect received." << std::endl;
                    close_proc(connections, subs, ep.shared_from_this());
                });
            ep.set_puback_handler(
                [&]
                (std::uint16_t packet_id){
                    std::cout << "puback received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubrec_handler(
                [&]
                (std::uint16_t packet_id){
                    std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubrel_handler(
                [&]
                (std::uint16_t packet_id){
                    std::cout << "pubrel received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubcomp_handler(
                [&]
                (std::uint16_t packet_id){
                    std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_publish_handler(
                [&]
                (std::uint8_t header,
                 boost::optional<std::uint16_t> packet_id,
                 std::string topic_name,
                 std::string contents){
                    std::uint8_t qos = mqtt::publish::get_qos(header);
                    bool retain = mqtt::publish::is_retain(header);
                    std::cout << "publish received."
                              << " dup: " << std::boolalpha << mqtt::publish::is_dup(header)
                              << " qos: " << mqtt::qos::to_str(qos)
                              << " retain: " << retain << std::endl;
                    if (packet_id)
                        std::cout << "packet_id: " << *packet_id << std::endl;
                    std::cout << "topic_name: " << topic_name << std::endl;
                    std::cout << "contents: " << contents << std::endl;
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
            ep.set_subscribe_handler(
                [&]
                (std::uint16_t packet_id,
                 std::vector<std::tuple<std::string, std::uint8_t>> entries) {
                    std::cout << "subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<std::uint8_t> res;
                    res.reserve(entries.size());
                    for (auto const& e : entries) {
                        std::string const& topic = std::get<0>(e);
                        std::uint8_t qos = std::get<1>(e);
                        std::cout << "topic: " << topic  << " qos: " << static_cast<int>(qos) << std::endl;
                        res.emplace_back(qos);
                        subs.emplace(topic, ep.shared_from_this(), qos);
                    }
                    ep.suback(packet_id, res);
                    return true;
                }
            );
            ep.set_unsubscribe_handler(
                [&]
                (std::uint16_t packet_id,
                 std::vector<std::string> topics) {
                    std::cout << "unsubscribe received. packet_id: " << packet_id << std::endl;
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

    ios.run();
}

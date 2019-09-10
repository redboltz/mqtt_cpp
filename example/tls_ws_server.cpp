// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <set>

#include <mqtt_server_cpp.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace mi = boost::multi_index;

using con_t = MQTT_NS::server_tls_ws<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;

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

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << argv[0] << " port server_crt server_key" << std::endl;
        return -1;
    }
    boost::asio::io_context ioc;

    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);
    std::string cert = argv[2];
    std::string key = argv[3];

    boost::asio::ssl::context  ctx(boost::asio::ssl::context::tlsv12);
    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::single_dh_use);
    ctx.use_certificate_file(cert, boost::asio::ssl::context::pem);
    ctx.use_private_key_file(key, boost::asio::ssl::context::pem);

    auto s = MQTT_NS::server_tls_ws<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            port
        ),
        std::move(ctx),
        ioc
    );

    s.set_error_handler(
        [](boost::system::error_code const& ec) {
            std::cout << "error: " << ec.message() << std::endl;
        }
    );

    std::set<con_sp_t> connections;
    mi_sub_con subs;

    s.set_accept_handler(
        [&](con_sp_t spep) {
            using packet_id_t = con_sp_t::element_type::packet_id_t;
            std::cout << "accept" << std::endl;
            spep->start_session(
                [spep] // keeping ep's lifetime as sp until session finished
                (boost::system::error_code const& ec) {
                    std::cout << "session end: " << ec.message() << std::endl;
                }
            );

            // set connection (lower than MQTT) level handlers
            spep->set_close_handler(
                [&, spep]
                (){
                    std::cout << "closed." << std::endl;
                    close_proc(connections, subs, spep);
                });
            spep->set_error_handler(
                [&, spep]
                (boost::system::error_code const& ec){
                    std::cout << "error: " << ec.message() << std::endl;
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
                    std::cout << "client_id    : " << client_id << std::endl;
                    std::cout << "username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    std::cout << "password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    std::cout << "clean_session: " << std::boolalpha << clean_session << std::endl;
                    std::cout << "keep_alive   : " << keep_alive << std::endl;
                    connections.insert(spep);
                    spep->connack(false, MQTT_NS::connect_return_code::accepted);
                    return true;
                }
            );
            spep->set_disconnect_handler(
                [&, spep]
                (){
                    std::cout << "disconnect received." << std::endl;
                    close_proc(connections, subs, spep);
                });
            spep->set_puback_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "puback received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubrec_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubrel_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "pubrel received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            spep->set_pubcomp_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;
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
                    std::cout << "publish received."
                              << " dup: " << std::boolalpha << MQTT_NS::publish::is_dup(header)
                              << " qos: " << qos_value
                              << " retain: " << retain << std::endl;
                    if (packet_id)
                        std::cout << "packet_id: " << *packet_id << std::endl;
                    std::cout << "topic_name: " << topic_name << std::endl;
                    std::cout << "contents: " << contents << std::endl;
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
                    std::cout << "subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<MQTT_NS::suback_reason_code> res;
                    res.reserve(entries.size());
                    for (auto const& e : entries) {
                        MQTT_NS::buffer topic = std::get<0>(e);
                        MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                        std::cout << "topic: " << topic  << " qos: " << qos_value << std::endl;
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
                    std::cout << "unsubscribe received. packet_id: " << packet_id << std::endl;
                    for (auto const& topic : topics) {
                        subs.erase(topic);
                    }
                    spep->unsuback(packet_id);
                    return true;
                }
            );
        }
    );

    s.listen();

    ioc.run();
}

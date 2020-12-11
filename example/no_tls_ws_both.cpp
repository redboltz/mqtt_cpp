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

#include "locked_cout.hpp"

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
            locked_cout() << "[client] Connack handler called" << std::endl;
            locked_cout() << "[client] Session Present: " << std::boolalpha << sp << std::endl;
            locked_cout() << "[client] Connack Return Code: "
                      << MQTT_NS::connect_return_code_to_str(connack_return_code) << std::endl;
            if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                pid_sub1 = c->subscribe("mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once);
                pid_sub2 = c->subscribe(
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
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
            locked_cout() << "[client] closed." << std::endl;
        });
    c->set_error_handler(
        []
        (MQTT_NS::error_code ec){
            locked_cout() << "[client] error: " << ec.message() << std::endl;
        });
    c->set_puback_handler(
        [&]
        (packet_id_t packet_id){
            locked_cout() << "[client] puback received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_pubrec_handler(
        [&]
        (packet_id_t packet_id){
            locked_cout() << "[client] pubrec received. packet_id: " << packet_id << std::endl;
            return true;
        });
    c->set_pubcomp_handler(
        [&]
        (packet_id_t packet_id){
            locked_cout() << "[client] pubcomp received. packet_id: " << packet_id << std::endl;
            disconnect();
            return true;
        });
    c->set_suback_handler(
        [&]
        (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results){
            locked_cout() << "[client] suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : results) {
                locked_cout() << "[client] subscribe result: " << e << std::endl;
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
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic_name,
         MQTT_NS::buffer contents){
            locked_cout() << "[client] publish received. "
                          << " dup: "    << pubopts.get_dup()
                          << " qos: "    << pubopts.get_qos()
                          << " retain: " << pubopts.get_retain() << std::endl;
            if (packet_id)
                locked_cout() << "[client] packet_id: " << *packet_id << std::endl;
            locked_cout() << "[client] topic_name: " << topic_name << std::endl;
            locked_cout() << "[client] contents: " << contents << std::endl;
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

using con_t = MQTT_NS::server_ws<>::endpoint_t;
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

template <typename Server>
void server_proc(Server& s, std::set<con_sp_t>& connections, mi_sub_con& subs) {
    s.set_error_handler(
        [](MQTT_NS::error_code ec) {
            locked_cout() << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler(
        [&s, &connections, &subs](con_sp_t spep) {
            auto& ep = *spep;
            std::weak_ptr<con_t> wp(spep);

            using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
            locked_cout() << "[server] accept" << std::endl;
            // For server close if ep is closed.
            auto g = MQTT_NS::shared_scope_guard(
                [&s] {
                    locked_cout() << "[server] session end" << std::endl;
                    s.close();
                }
            );
            // Pass spep to keep lifetime.
            // It makes sure wp.lock() never return nullptr in the handlers below
            // including close_handler and error_handler.
            ep.start_session(std::make_tuple(std::move(spep), std::move(g)));

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler(
                [&connections, &subs, wp]
                (){
                    locked_cout() << "[server] closed." << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });
            ep.set_error_handler(
                [&connections, &subs, wp]
                (MQTT_NS::error_code ec){
                    locked_cout() << "[server] error: " << ec.message() << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });

            // set MQTT level handlers
            ep.set_connect_handler(
                [&connections, wp]
                (MQTT_NS::buffer client_id,
                 MQTT_NS::optional<MQTT_NS::buffer> username,
                 MQTT_NS::optional<MQTT_NS::buffer> password,
                 MQTT_NS::optional<MQTT_NS::will>,
                 bool clean_session,
                 std::uint16_t keep_alive) {
                    using namespace MQTT_NS::literals;
                    locked_cout() << "[server] client_id    : " << client_id << std::endl;
                    locked_cout() << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
                    locked_cout() << "[server] keep_alive   : " << keep_alive << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.insert(sp);
                    sp->connack(false, MQTT_NS::connect_return_code::accepted);
                    return true;
                }
            );
            ep.set_disconnect_handler(
                [&connections, &subs, wp]
                (){
                    locked_cout() << "[server] disconnect received." << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });
            ep.set_puback_handler(
                []
                (packet_id_t packet_id){
                    locked_cout() << "[server] puback received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubrec_handler(
                []
                (packet_id_t packet_id){
                    locked_cout() << "[server] pubrec received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubrel_handler(
                []
                (packet_id_t packet_id){
                    locked_cout() << "[server] pubrel received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_pubcomp_handler(
                []
                (packet_id_t packet_id){
                    locked_cout() << "[server] pubcomp received. packet_id: " << packet_id << std::endl;
                    return true;
                });
            ep.set_publish_handler(
                [&subs]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents){
                    locked_cout() << "[server] publish received."
                                  << " dup: "    << pubopts.get_dup()
                                  << " qos: "    << pubopts.get_qos()
                                  << " retain: " << pubopts.get_retain() << std::endl;
                    if (packet_id)
                        locked_cout() << "[server] packet_id: " << *packet_id << std::endl;
                    locked_cout() << "[server] topic_name: " << topic_name << std::endl;
                    locked_cout() << "[server] contents: " << contents << std::endl;
                    auto const& idx = subs.get<tag_topic>();
                    auto r = idx.equal_range(topic_name);
                    for (; r.first != r.second; ++r.first) {
                        r.first->con->publish(
                            topic_name,
                            contents,
                            std::min(r.first->qos_value, pubopts.get_qos())
                        );
                    }
                    return true;
                });
            ep.set_subscribe_handler(
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::subscribe_entry> entries) {
                    locked_cout() << "[server]subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<MQTT_NS::suback_return_code> res;
                    res.reserve(entries.size());
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    for (auto const& e : entries) {
                        locked_cout() << "[server] topic_filter: " << e.topic_filter  << " qos: " << e.subopts.get_qos() << std::endl;
                        res.emplace_back(MQTT_NS::qos_to_suback_return_code(e.subopts.get_qos()));
                        subs.emplace(std::move(e.topic_filter), sp, e.subopts.get_qos());
                    }
                    sp->suback(packet_id, res);
                    return true;
                }
            );
            ep.set_unsubscribe_handler(
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::unsubscribe_entry> entries) {
                    locked_cout() << "[server]unsubscribe received. packet_id: " << packet_id << std::endl;
                    for (auto const& e : entries) {
                        subs.erase(e.topic_filter);
                    }
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    sp->unsuback(packet_id);
                    return true;
                }
            );
        }
    );

    s.listen();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        locked_cout() << argv[0] << " port" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;
    std::uint16_t port = boost::lexical_cast<std::uint16_t>(argv[1]);

    // server
    boost::asio::io_context iocs;
    auto s = MQTT_NS::server_ws<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            port
        ),
        iocs
    );
    std::set<con_sp_t> connections;
    mi_sub_con subs;
    std::thread th(
        [&] {
            server_proc(s, connections, subs);
            iocs.run();
        }
    );


    // client
    std::uint16_t pid_sub1;
    std::uint16_t pid_sub2;

    auto c = MQTT_NS::make_sync_client_ws(ioc, "localhost", port);

    int count = 0;
    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };
    client_proc(c, pid_sub1, pid_sub2, disconnect);

    ioc.run();
    th.join();
}

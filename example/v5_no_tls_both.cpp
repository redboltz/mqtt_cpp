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
    c->set_clean_start(true);

    // Setup handlers
    c->set_v5_connack_handler( // use v5 handler
        [&c, &pid_sub1, &pid_sub2]
        (bool sp, MQTT_NS::v5::connect_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
            locked_cout() << "[client] Connack handler called" << std::endl;
            locked_cout() << "[client] Session Present: " << std::boolalpha << sp << std::endl;
            locked_cout() << "[client] Connect Reason Code: " << reason_code << std::endl;
            if (reason_code == MQTT_NS::v5::connect_reason_code::success) {
                pid_sub1 = c->subscribe("mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once);
                pid_sub2 = c->subscribe(
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>
                    {
                        { "mqtt_client_cpp/topic2_1", MQTT_NS::qos::at_least_once },
                        { "mqtt_client_cpp/topic2_2", MQTT_NS::qos::exactly_once }
                    }
                );
            }
        });
    c->set_close_handler( // this handler doesn't depend on MQTT protocol version
        []
        (){
            locked_cout() << "[client] closed." << std::endl;
        });
    c->set_error_handler( // this handler doesn't depend on MQTT protocol version
        []
        (MQTT_NS::error_code ec){
            locked_cout() << "[client] error: " << ec.message() << std::endl;
        });
    c->set_v5_puback_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
            locked_cout() <<
                "[client] puback received. packet_id: " << packet_id <<
                " reason_code: " << reason_code << std::endl;
            disconnect();
        });
    c->set_v5_pubrec_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
            locked_cout() <<
                "[client] pubrec received. packet_id: " << packet_id <<
                " reason_code: " << reason_code << std::endl;
        });
    c->set_v5_pubcomp_handler( // use v5 handler
        [&]
        (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
            locked_cout() <<
                "[client] pubcomp received. packet_id: " << packet_id <<
                " reason_code: " << reason_code << std::endl;
            disconnect();
        });
    c->set_v5_suback_handler( // use v5 handler
        [&]
        (packet_id_t packet_id,
         std::vector<MQTT_NS::v5::suback_reason_code> reasons,
         MQTT_NS::v5::properties /*props*/){
            locked_cout() << "[client] suback received. packet_id: " << packet_id << std::endl;
            for (auto const& e : reasons) {
                switch (e) {
                case MQTT_NS::v5::suback_reason_code::granted_qos_0:
                    locked_cout() << "[client] subscribe success: qos0" << std::endl;
                    break;
                case MQTT_NS::v5::suback_reason_code::granted_qos_1:
                    locked_cout() << "[client] subscribe success: qos1" << std::endl;
                    break;
                case MQTT_NS::v5::suback_reason_code::granted_qos_2:
                    locked_cout() << "[client] subscribe success: qos2" << std::endl;
                    break;
                default:
                    locked_cout() << "[client] subscribe failed: reason_code = " << static_cast<int>(e) << std::endl;
                    break;
                }
            }
            if (packet_id == pid_sub1) {
                c->publish("mqtt_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once);
            }
            else if (packet_id == pid_sub2) {
                c->publish("mqtt_client_cpp/topic2_1", "test2_1", MQTT_NS::qos::at_least_once);
                c->publish("mqtt_client_cpp/topic2_2", "test2_2", MQTT_NS::qos::exactly_once);
            }
        });
    c->set_v5_publish_handler( // use v5 handler
        [&]
        (MQTT_NS::optional<packet_id_t> packet_id,
         MQTT_NS::publish_options pubopts,
         MQTT_NS::buffer topic_name,
         MQTT_NS::buffer contents,
         MQTT_NS::v5::properties /*props*/){
            locked_cout() << "[client] publish received. "
                          << " dup: "    << pubopts.get_dup()
                          << " qos: "    << pubopts.get_qos()
                          << " retain: " << pubopts.get_retain() << std::endl;
            if (packet_id)
                locked_cout() << "[client] packet_id: " << *packet_id << std::endl;
            locked_cout() << "[client] topic_name: " << topic_name << std::endl;
            locked_cout() << "[client] contents: " << contents << std::endl;
            disconnect();
        });

    // Connect
    c->connect();
}

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <mqtt_server_cpp.hpp>

namespace mi = boost::multi_index;

using con_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;

struct sub_con {
    sub_con(MQTT_NS::buffer topic, con_sp_t con, MQTT_NS::qos qos_value, MQTT_NS::rap rap_value)
        :topic(std::move(topic)), con(std::move(con)), qos_value(qos_value), rap_value(rap_value) {}
    MQTT_NS::buffer topic;
    con_sp_t con;
    MQTT_NS::qos qos_value;
    MQTT_NS::rap rap_value;
};

struct tag_topic {};
struct tag_con {};
struct tag_con_topic {};

using mi_sub_con = mi::multi_index_container<
    sub_con,
    mi::indexed_by<
        mi::ordered_unique<
            mi::tag<tag_con_topic>,
            mi::composite_key<
                sub_con,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con),
                BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
            >
        >,
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
    s.set_error_handler( // this handler doesn't depend on MQTT protocol version
        [](MQTT_NS::error_code ec) {
            locked_cout() << "[server] error: " << ec.message() << std::endl;
        }
    );
    s.set_accept_handler( // this handler doesn't depend on MQTT protocol version
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
            ep.set_v5_connect_handler( // use v5 handler
                [&connections, wp]
                (MQTT_NS::buffer client_id,
                 MQTT_NS::optional<MQTT_NS::buffer> const& username,
                 MQTT_NS::optional<MQTT_NS::buffer> const& password,
                 MQTT_NS::optional<MQTT_NS::will>,
                 bool clean_start,
                 std::uint16_t keep_alive,
                 MQTT_NS::v5::properties /*props*/){
                    using namespace MQTT_NS::literals;
                    locked_cout() << "[server] client_id    : " << client_id << std::endl;
                    locked_cout() << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    locked_cout() << "[server] clean_start  : " << std::boolalpha << clean_start << std::endl;
                    locked_cout() << "[server] keep_alive   : " << keep_alive << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.insert(sp);
                    sp->connack(false, MQTT_NS::v5::connect_reason_code::success);
                }
            );
            ep.set_v5_disconnect_handler( // use v5 handler
                [&connections, &subs, wp]
                (MQTT_NS::v5::disconnect_reason_code reason_code, MQTT_NS::v5::properties /*props*/) {
                    locked_cout() <<
                        "[server] disconnect received." <<
                        " reason_code: " << reason_code << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });
            ep.set_v5_puback_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    locked_cout() <<
                        "[server] puback received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                });
            ep.set_v5_pubrec_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    locked_cout() <<
                        "[server] pubrec received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                });
            ep.set_v5_pubrel_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubrel_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    locked_cout() <<
                        "[server] pubrel received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                });
            ep.set_v5_pubcomp_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    locked_cout() <<
                        "[server] pubcomp received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                });
            ep.set_v5_publish_handler( // use v5 handler
                [&subs]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties props){
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
                        auto retain =
                            [&] {
                                if (r.first->rap_value == MQTT_NS::rap::retain) {
                                    return pubopts.get_retain();
                                }
                                return MQTT_NS::retain::no;
                            } ();
                        r.first->con->publish(
                            topic_name,
                            contents,
                            std::min(r.first->qos_value, pubopts.get_qos()) | retain,
                            std::move(props)
                        );
                    }
                });
            ep.set_v5_subscribe_handler( // use v5 handler
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::subscribe_entry> entries,
                 MQTT_NS::v5::properties /*props*/) {
                    locked_cout() << "[server] subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<MQTT_NS::v5::suback_reason_code> res;
                    res.reserve(entries.size());
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    for (auto const& e : entries) {
                        locked_cout() << "[server] topic_filter: " << e.topic_filter
                                      << " qos: " << e.subopts.get_qos()
                                      << " rap: " << e.subopts.get_rap()
                                      << std::endl;
                        res.emplace_back(MQTT_NS::v5::qos_to_suback_reason_code(e.subopts.get_qos()));
                        subs.emplace(std::move(e.topic_filter), sp, e.subopts.get_qos(), e.subopts.get_rap());
                    }
                    sp->suback(packet_id, res);
                }
            );
            ep.set_v5_unsubscribe_handler( // use v5 handler
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::unsubscribe_entry> entries,
                 MQTT_NS::v5::properties /*props*/) {
                    locked_cout() << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
                    auto sp = wp.lock();
                    for (auto const& e : entries) {
                        auto it = subs.find(std::make_tuple(sp, e.topic_filter));
                        if (it != subs.end()) {
                            subs.erase(it);
                        }
                    }
                    BOOST_ASSERT(sp);
                    sp->unsuback(packet_id);
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
    auto s = MQTT_NS::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        iocs
    );

    // You can set a specific protocol_version if you want to limit accepting version.
    // Otherwise, all protocols are accepted.
    s.set_protocol_version(MQTT_NS::protocol_version::v5);

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

    // You can set the protocol_version to connect. If you don't set it, v3_1_1 is used.
    auto c = MQTT_NS::make_sync_client(ioc, "localhost", port, MQTT_NS::protocol_version::v5);

    int count = 0;
    auto disconnect = [&] {
        if (++count == 5) c->disconnect();
    };
    client_proc(c, pid_sub1, pid_sub2, disconnect);

    ioc.run();
    th.join();
}

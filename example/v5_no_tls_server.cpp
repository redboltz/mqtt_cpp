// Copyright Takatoshi Kondo 2019
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
    if (argc != 2) {
        std::cout << argv[0] << " port" << std::endl;
        return -1;
    }

    MQTT_NS::setup_log();

    boost::asio::io_context ioc;

    auto s = MQTT_NS::server<>(
        boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(),
            boost::lexical_cast<std::uint16_t>(argv[1])
        ),
        ioc
    );

    s.set_error_handler(
        [](MQTT_NS::error_code ec) {
            std::cout << "error: " << ec.message() << std::endl;
        }
    );

    std::set<con_sp_t> connections;
    mi_sub_con subs;

    s.set_accept_handler( // this handler doesn't depend on MQTT protocol version
        [&connections, &subs](con_sp_t spep) {
            auto& ep = *spep;
            std::weak_ptr<con_t> wp(spep);

            using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
            std::cout << "[server] accept" << std::endl;
            // Pass spep to keep lifetime.
            // It makes sure wp.lock() never return nullptr in the handlers below
            // including close_handler and error_handler.
            ep.start_session(std::move(spep));

            // set connection (lower than MQTT) level handlers
            ep.set_close_handler(
                [&connections, &subs, wp]
                (){
                    std::cout << "[server] closed." << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });
            ep.set_error_handler(
                [&connections, &subs, wp]
                (MQTT_NS::error_code ec){
                    std::cout << "[server] error: " << ec.message() << std::endl;
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
                    std::cout << "[server] client_id    : " << client_id << std::endl;
                    std::cout << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                    std::cout << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                    std::cout << "[server] clean_start  : " << std::boolalpha << clean_start << std::endl;
                    std::cout << "[server] keep_alive   : " << keep_alive << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    connections.insert(sp);
                    sp->connack(false, MQTT_NS::v5::connect_reason_code::success);
                    return true;
                }
            );
            ep.set_v5_disconnect_handler( // use v5 handler
                [&connections, &subs, wp]
                (MQTT_NS::v5::disconnect_reason_code reason_code, MQTT_NS::v5::properties /*props*/) {
                    std::cout <<
                        "[server] disconnect received." <<
                        " reason_code: " << reason_code << std::endl;
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    close_proc(connections, subs, sp);
                });
            ep.set_v5_puback_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    std::cout <<
                        "[server] puback received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                    return true;
                });
            ep.set_v5_pubrec_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    std::cout <<
                        "[server] pubrec received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                    return true;
                });
            ep.set_v5_pubrel_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubrel_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    std::cout <<
                        "[server] pubrel received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                    return true;
                });
            ep.set_v5_pubcomp_handler( // use v5 handler
                []
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code reason_code, MQTT_NS::v5::properties /*props*/){
                    std::cout <<
                        "[server] pubcomp received. packet_id: " << packet_id <<
                        " reason_code: " << reason_code << std::endl;
                    return true;
                });
            ep.set_v5_publish_handler( // use v5 handler
                [&subs]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties props){
                    std::cout << "[server] publish received."
                              << " dup: "    << pubopts.get_dup()
                              << " qos: "    << pubopts.get_qos()
                              << " retain: " << pubopts.get_retain() << std::endl;
                    if (packet_id)
                        std::cout << "[server] packet_id: " << *packet_id << std::endl;
                    std::cout << "[server] topic_name: " << topic_name << std::endl;
                    std::cout << "[server] contents: " << contents << std::endl;
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
                    return true;
                });
            ep.set_v5_subscribe_handler( // use v5 handler
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries,
                 MQTT_NS::v5::properties /*props*/) {
                    std::cout << "[server] subscribe received. packet_id: " << packet_id << std::endl;
                    std::vector<MQTT_NS::v5::suback_reason_code> res;
                    res.reserve(entries.size());
                    auto sp = wp.lock();
                    BOOST_ASSERT(sp);
                    for (auto const& e : entries) {
                        MQTT_NS::buffer topic = std::get<0>(e);
                        MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                        MQTT_NS::rap rap_value = std::get<1>(e).get_rap();
                        std::cout << "[server] topic: " << topic
                                  << " qos: " << qos_value
                                  << " rap: " << rap_value
                                  << std::endl;
                        res.emplace_back(MQTT_NS::v5::qos_to_suback_reason_code(qos_value));
                        subs.emplace(std::move(topic), sp, qos_value, rap_value);
                    }
                    sp->suback(packet_id, res);
                    return true;
                }
            );
            ep.set_v5_unsubscribe_handler( // use v5 handler
                [&subs, wp]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::buffer> topics,
                 MQTT_NS::v5::properties /*props*/) {
                    std::cout << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
                    for (auto const& topic : topics) {
                        subs.erase(topic);
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

    ioc.run();
}

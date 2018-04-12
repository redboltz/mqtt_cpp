// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_BROKER_HPP)
#define MQTT_TEST_BROKER_HPP

#include <iostream>
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt_server_cpp.hpp>

#include "test_settings.hpp"
#include "visitor_util.hpp"


namespace mi = boost::multi_index;
namespace as = boost::asio;

using con_sp_t = boost::variant<
    std::shared_ptr<mqtt::server<>::endpoint_t>
#if !defined(MQTT_NO_TLS)
    ,
    std::shared_ptr<mqtt::server_tls<>::endpoint_t>
#endif // !defined(MQTT_NO_TLS)
#if defined(MQTT_USE_WS)
    ,
    std::shared_ptr<mqtt::server_ws<>::endpoint_t>
#if !defined(MQTT_NO_TLS)
    ,
    std::shared_ptr<mqtt::server_tls_ws<>::endpoint_t>
#endif // !defined(MQTT_NO_TLS)
#endif // defined(MQTT_USE_WS)
>;

class test_broker {
public:
    test_broker(as::io_service& ios)
        :ios_(ios),
         tim_disconnect_(ios_)
    {}

    // [begin] for test setting
    void set_disconnect_delay(boost::posix_time::time_duration const& delay) {
        delay_disconnect_ = delay;
    }
    // [end] for test setting

    template <typename Endpoint>
    void handle_accept(Endpoint& ep) {
        auto sp = ep.shared_from_this();
        ep.start_session(
            [sp] // keeping ep's lifetime as sp until session finished
            (boost::system::error_code const& /*ec*/) {
            }
        );

        // set connection (lower than MQTT) level handlers
        ep.set_close_handler(
            [&]
            (){
                close_proc(ep, true);
            });
        ep.set_error_handler(
            [&]
            (boost::system::error_code const& /*ec*/){
                close_proc(ep, true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [&]
            (std::string const& client_id,
             boost::optional<std::string> const& /*username*/,
             boost::optional<std::string> const& /*password*/,
             boost::optional<mqtt::will> will,
             bool clean_session,
             std::uint16_t /*keep_alive*/) {
                if (client_id.empty() && !clean_session) {
                    ep.connack(false, mqtt::connect_return_code::identifier_rejected);
                    return false;
                }
                try_connect(clean_session, ep.shared_from_this(), client_id, std::move(will));
                return true;
            }
        );
        ep.set_disconnect_handler(
            [&]
            (){
                if (delay_disconnect_) {
                    std::weak_ptr<Endpoint> wp(ep.shared_from_this());
                    tim_disconnect_.expires_from_now(delay_disconnect_.get());
                    tim_disconnect_.async_wait(
                        [&, wp](boost::system::error_code const& ec) {
                            if (auto sp = wp.lock()) {
                                if (!ec) {
                                    close_proc(*sp, false);
                                }
                            }
                        }
                    );
                }
                else {
                    close_proc(ep, false);
                }
            });
        ep.set_puback_handler(
            [&]
            (std::uint16_t /*packet_id*/){
                return true;
            });
        ep.set_pubrec_handler(
            [&]
            (std::uint16_t /*packet_id*/){
                return true;
            });
        ep.set_pubrel_handler(
            [&]
            (std::uint16_t /*packet_id*/){
                return true;
            });
        ep.set_pubcomp_handler(
            [&]
            (std::uint16_t /*packet_id*/){
                return true;
            });
        ep.set_publish_handler(
            [&]
            (std::uint8_t header,
             boost::optional<std::uint16_t> /*packet_id*/,
             std::string topic_name,
             std::string contents){
                std::uint8_t qos = mqtt::publish::get_qos(header);
                bool is_retain = mqtt::publish::is_retain(header);
                do_publish(
                    std::make_shared<std::string>(std::move(topic_name)),
                    std::make_shared<std::string>(std::move(contents)),
                    qos,
                    is_retain);
                return true;
            });
        ep.set_subscribe_handler(
            [&]
            (std::uint16_t packet_id,
             std::vector<std::tuple<std::string, std::uint8_t>> entries) {
                std::vector<std::uint8_t> res;
                res.reserve(entries.size());
                for (auto const& e : entries) {
                    std::string const& topic = std::get<0>(e);
                    std::uint8_t qos = std::get<1>(e);
                    res.emplace_back(qos);
                    subs_.emplace(std::make_shared<std::string>(topic), ep.shared_from_this(), qos);
                }
                ep.suback(packet_id, res);
                for (auto const& e : entries) {
                    std::string const& topic = std::get<0>(e);
                    std::uint8_t qos = std::get<1>(e);
                    auto it = retains_.find(topic);
                    if (it != retains_.end()) {
                        ep.publish(
                            as::buffer(*it->topic),
                            as::buffer(*it->contents),
                            [t = it->topic, c = it->contents] {},
                            std::min(it->qos, qos),
                            true);
                    }
                }
                return true;
            }
        );
        ep.set_unsubscribe_handler(
            [&]
            (std::uint16_t packet_id,
             std::vector<std::string> topics) {
                for (auto const& topic : topics) {
                    subs_.erase(topic);
                }
                ep.unsuback(packet_id);
                return true;
            }
        );
        ep.set_pingreq_handler(
            [&] {
                ep.pingresp();
                return true;
            }
        );
    }

private:
    bool try_connect(
        bool clean_session,
        con_sp_t const& spep,
        std::string const& client_id,
        boost::optional<mqtt::will> will) {
        auto it_ret = cons_.emplace(client_id, spep);
        if (!std::get<1>(it_ret)) {
            pending_.emplace_back(clean_session, spep, client_id, std::move(will));
            return false;
        }
        auto cid = boost::apply_visitor(
            make_lambda_visitor<std::string>(
                [&](auto const& con) {
                    return con->client_id();
                }
            ),
            spep
        );
        connect_proc(clean_session, spep, client_id, std::move(will));
        return true;
    }

    void connect_proc(
        bool clean_session,
        con_sp_t const& spep,
        std::string const& client_id,
        boost::optional<mqtt::will> will) {
        if (clean_session) {
            boost::apply_visitor(
                make_lambda_visitor<void>(
                    [&](auto& con) {
                        con->connack(false, mqtt::connect_return_code::accepted);
                    }
                ),
                spep
            );
            sessions_.erase(client_id);
            subsessions_.erase(client_id);
        }
        else {
            {
                auto it = sessions_.find(client_id);
                if (it == sessions_.end()) {
                    boost::apply_visitor(
                        make_lambda_visitor<void>(
                            [&](auto& con) {
                                con->connack(false, mqtt::connect_return_code::accepted);
                            }
                        ),
                        spep
                    );
                }
                else {
                    sessions_.erase(it);
                    boost::apply_visitor(
                        make_lambda_visitor<void>(
                            [&](auto& con) {
                                con->connack(true, mqtt::connect_return_code::accepted);
                            }
                        ),
                        spep
                    );
                }
            }
            auto r = subsessions_.equal_range(client_id);
            std::vector<session_data> data;
            if (r.first != r.second) {
                data = std::move(r.first->s->data);
            }
            while (r.first != r.second) {
                subs_.emplace(r.first->topic, spep, r.first->qos);
                r.first = subsessions_.erase(r.first);
            }
            for (auto const& d : data) {
                boost::apply_visitor(
                    make_lambda_visitor<void>(
                        [&](auto& con) {
                            con->publish(
                                as::buffer(*d.topic),
                                as::buffer(*d.contents),
                                [t = d.topic, c = d.contents] {},
                                d.qos,
                                true
                            );
                        }
                    ),
                    spep
                );
            }
        }
        if (will) {
            will_.emplace(spep, std::move(will.get()));
        }
    }

    void do_publish(
        std::shared_ptr<std::string> const& topic,
        std::shared_ptr<std::string> const& contents,
        std::uint8_t qos,
        bool is_retain) {
        {
            auto const& idx = subs_.get<tag_topic>();
            auto r = idx.equal_range(*topic);
            for (; r.first != r.second; ++r.first) {
                boost::apply_visitor(
                    make_lambda_visitor<void>(
                        [&](auto& con) {
                            con->publish(
                                as::buffer(*topic),
                                as::buffer(*contents),
                                [topic, contents] {},
                                std::min(r.first->qos, qos),
                                false
                            );
                        }
                    ),
                    r.first->con
                );
            }
        }
        {
            auto const& idx = subsessions_.get<tag_topic>();
            auto r = idx.equal_range(*topic);
            for (; r.first != r.second; ++r.first) {
                r.first->s->data.emplace_back(
                    topic,
                    contents,
                    std::min(r.first->qos, qos)
                );
            }
        }
        if (is_retain) {
            if (contents->empty()) {
                retains_.erase(*topic);
            }
            else {
                retains_.erase(*topic);
                retains_.emplace(topic, qos, contents);
            }
        }
    }

    template <typename Endpoint>
    void close_proc(Endpoint& con, bool send_will) {
        auto cs = con.clean_session();
        auto client_id = con.client_id();

        {   // will processing
            auto it = will_.find(con.shared_from_this());
            if (it != will_.end()) {
                if (send_will) {
                    do_publish(
                        std::make_shared<std::string>(std::move(it->will.topic())),
                        std::make_shared<std::string>(std::move(it->will.message())),
                        it->will.qos(),
                        it->will.retain());
                }
                will_.erase(it);
            }
        }

        auto& idx = cons_.get<tag_con>();
        idx.erase(con.shared_from_this());
        {
            auto& idx = subs_.get<tag_con>();
            auto r = idx.equal_range(con.shared_from_this());
            if (cs) {
                idx.erase(r.first, r.second);
            }
            else {
                sessions_.emplace(client_id);
                auto s = std::make_shared<session>(client_id);
                while (r.first != r.second) {
                    subsessions_.emplace(r.first->topic, s, r.first->qos);
                    r.first = idx.erase(r.first);
                }
            }
        }

        auto it = pending_.begin();
        auto end = pending_.end();
        while (it != end) {
            if (try_connect(it->clean_session, it->spep, it->client_id, it->will)) {
                it = pending_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

private:

    struct tag_topic {};
    struct tag_con {};
    struct tag_client_id {};

    struct cid_con {
        cid_con(std::string const& cid, con_sp_t const& con)
            :cid(cid), con(con) {}
        std::string cid;
        con_sp_t con;
    };
    using mi_cid_con = mi::multi_index_container<
        cid_con,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(cid_con, std::string, cid)
            >,
            mi::ordered_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(cid_con, con_sp_t, con)
            >
        >
    >;

    struct sub_con {
        sub_con(
            std::shared_ptr<std::string> const& topic,
            con_sp_t const& con,
            std::uint8_t qos)
            :topic(topic), con(con), qos(qos) {}
        std::string const& get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        con_sp_t con;
        std::uint8_t qos;
    };
    using mi_sub_con = mi::multi_index_container<
        sub_con,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_con, std::string const&, sub_con::get_topic)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
            >
        >
    >;

    struct retain {
        retain(
            std::shared_ptr<std::string> const& topic,
            std::uint8_t qos,
            std::shared_ptr<std::string> const& contents)
            :topic(topic), qos(qos), contents(contents) {}
        std::string const& get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        std::uint8_t qos;
        std::shared_ptr<std::string> contents;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(retain, std::string const&, retain::get_topic)
            >
        >
    >;

    struct session_data {
        session_data(
            std::shared_ptr<std::string> const& topic,
            std::shared_ptr<std::string> const& contents,
            std::uint8_t qos)
            : topic(std::move(topic)), contents(contents), qos(qos) {}
        std::shared_ptr<std::string> topic;
        std::shared_ptr<std::string> contents;
        std::uint8_t qos;
    };
    struct session {
        explicit session(std::string const& client_id)
            :client_id(client_id) {}
        std::string client_id;
        std::vector<session_data> data;
    };
    struct sub_session {
        sub_session(
            std::shared_ptr<std::string> const& topic,
            std::shared_ptr<session> const& s,
            std::uint8_t qos)
            :topic(topic), s(s), qos(qos) {}
        std::string const& get_client_id() const {
            return s->client_id;
        }
        std::string const& get_topic() const {
            return *topic;
        }
        std::shared_ptr<std::string> topic;
        std::shared_ptr<session> s;
        std::uint8_t qos;
    };
    using mi_sub_session = mi::multi_index_container<
        sub_session,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_session, std::string const&, sub_session::get_client_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_session, std::string const&, sub_session::get_topic)
            >
        >
    >;

    struct con_will {
        con_will(con_sp_t const& con, mqtt::will&& will):con(con), will(will) {}
        con_sp_t con;
        mqtt::will will;
    };
    using mi_con_will = mi::multi_index_container<
        con_will,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(con_will, con_sp_t, con)
            >
        >
    >;

    struct pending {
        pending(
            bool clean_session,
            con_sp_t const& spep,
            std::string const& client_id,
            boost::optional<mqtt::will> will)
            : clean_session(clean_session), spep(spep), client_id(client_id), will(std::move(will)) {}
        bool clean_session;
        con_sp_t spep;
        std::string const& client_id;
        boost::optional<mqtt::will> will;
    };
    using mi_pending = mi::multi_index_container<
        pending,
        mi::indexed_by<
            mi::sequenced<>
        >
    >;

    as::io_service& ios_;
    as::deadline_timer tim_disconnect_;
    boost::optional<boost::posix_time::time_duration> delay_disconnect_;
    mi_cid_con cons_;
    mi_sub_con subs_;
    std::set<std::string> sessions_;
    mi_sub_session subsessions_;
    mi_retain retains_;
    mi_con_will will_;
    mi_pending pending_;
};

#endif // MQTT_TEST_BROKER_HPP

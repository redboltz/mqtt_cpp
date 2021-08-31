// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_INFLIGHT_MESSAGE_HPP)
#define MQTT_BROKER_INFLIGHT_MESSAGE_HPP

#include <mqtt/config.hpp>

#include <chrono>

#include <boost/asio/steady_timer.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/message_variant.hpp>
#include <mqtt/any.hpp>
#include <mqtt/visitor_util.hpp>

#include <mqtt/broker/common_type.hpp>
#include <mqtt/broker/tags.hpp>
#include <mqtt/broker/property_util.hpp>

MQTT_BROKER_NS_BEGIN

class inflight_messages;

class inflight_message {
public:
    inflight_message(
        store_message_variant msg,
        any life_keeper,
        std::shared_ptr<as::steady_timer> tim_message_expiry)
        :msg_ { force_move(msg) },
         life_keeper_ { force_move(life_keeper) },
         tim_message_expiry_ { force_move(tim_message_expiry) }
    {}

    packet_id_t packet_id() const {
        return
            MQTT_NS::visit(
                make_lambda_visitor(
                    [](auto const& m) {
                        return m.packet_id();
                    }
                ),
                msg_
            );
    }

    void send(endpoint_t& ep) const {
        optional<store_message_variant> msg_opt;
        if (tim_message_expiry_) {
            MQTT_NS::visit(
                make_lambda_visitor(
                    [&](v5::basic_publish_message<sizeof(packet_id_t)> const& m) {
                        auto updated_msg = m;
                        auto d =
                            std::chrono::duration_cast<std::chrono::seconds>(
                                tim_message_expiry_->expiry() - std::chrono::steady_clock::now()
                            ).count();
                        if (d < 0) d = 0;
                        updated_msg.update_prop(
                            v5::property::message_expiry_interval(
                                static_cast<uint32_t>(d)
                            )
                        );
                        msg_opt.emplace(force_move(updated_msg));
                    },
                    [](auto const&) {
                    }
                ),
                msg_
            );
        }
        // packet_id_exhausted never happen because inflight message has already
        // allocated packet_id at the previous connection.
        // In async_send_store_message(), packet_id is registered.
        ep.async_send_store_message(
            msg_opt ? msg_opt.value() : msg_,
            life_keeper_,
            [sp = ep.shared_from_this()](error_code ec) {
                if (ec) {
                    MQTT_LOG("mqtt_broker", trace)
                        << MQTT_ADD_VALUE(address, sp.get())
                        <<  ec;
                }
            }
        );
    }

private:
    friend class inflight_messages;

    store_message_variant msg_;
    any life_keeper_;
    std::shared_ptr<as::steady_timer> tim_message_expiry_;
};

class inflight_messages {
public:
    void insert(
        store_message_variant msg,
        any life_keeper,
        std::shared_ptr<as::steady_timer> tim_message_expiry
    ) {
        messages_.emplace_back(
            force_move(msg),
            force_move(life_keeper),
            force_move(tim_message_expiry)
        );
    }

    void send_all_messages(endpoint_t& ep) {
        for (auto const& ifm : messages_) {
            ifm.send(ep);
        }
    }

    void clear() {
        messages_.clear();
    }

    template <typename Tag>
    decltype(auto) get() {
        return messages_.get<Tag>();
    }

    template <typename Tag>
    decltype(auto) get() const {
        return messages_.get<Tag>();
    }

private:
    using mi_inflight_message = mi::multi_index_container<
        inflight_message,
        mi::indexed_by<
            mi::sequenced<
                mi::tag<tag_seq>
            >,
            mi::ordered_unique<
                mi::tag<tag_pid>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(inflight_message, packet_id_t, packet_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(inflight_message, std::shared_ptr<as::steady_timer>, tim_message_expiry_)
            >
        >
    >;

    mi_inflight_message messages_;
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_INFLIGHT_MESSAGE_HPP

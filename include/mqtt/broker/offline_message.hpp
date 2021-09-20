// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_OFFLINE_MESSAGE_HPP)
#define MQTT_BROKER_OFFLINE_MESSAGE_HPP

#include <mqtt/config.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>

#include <mqtt/buffer.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/publish.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/broker/common_type.hpp>
#include <mqtt/broker/tags.hpp>
#include <mqtt/broker/property_util.hpp>

MQTT_BROKER_NS_BEGIN

namespace mi = boost::multi_index;

class offline_messages;

// The offline_message structure holds messages that have been published on a
// topic that a not-currently-connected client is subscribed to.
// When a new connection is made with the client id for this saved data,
// these messages will be published to that client, and only that client.
class offline_message {
public:
    offline_message(
        buffer topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props,
        std::shared_ptr<as::steady_timer> tim_message_expiry)
        : topic_(force_move(topic)),
          contents_(force_move(contents)),
          pubopts_(pubopts),
          props_(force_move(props)),
          tim_message_expiry_(force_move(tim_message_expiry))
    { }

    bool send(endpoint_t& ep) {
        auto props = props_;
        if (tim_message_expiry_) {
            auto d =
                std::chrono::duration_cast<std::chrono::seconds>(
                    tim_message_expiry_->expiry() - std::chrono::steady_clock::now()
                ).count();
            if (d < 0) d = 0;
            set_property<v5::property::message_expiry_interval>(
                props,
                v5::property::message_expiry_interval(
                    static_cast<uint32_t>(d)
                )
            );
        }
        auto qos_value = pubopts_.get_qos();
        if (qos_value == qos::at_least_once ||
            qos_value == qos::exactly_once) {
            if (auto pid = ep.acquire_unique_packet_id_no_except()) {
                ep.async_publish(
                    pid.value(),
                    force_move(topic_),
                    force_move(contents_),
                    pubopts_,
                    force_move(props),
                    any{},
                    [sp = ep.shared_from_this()]
                    (error_code ec) {
                        if (ec) {
                            MQTT_LOG("mqtt_broker", warning)
                                << MQTT_ADD_VALUE(address, sp.get())
                                << ec.message();
                        }
                    }
                );
                return true;
            }
        }
        else {
            ep.publish(
                topic_,
                contents_,
                pubopts_,
                force_move(props)
            );
            return true;
        }
        return false;
    }

private:
    friend class offline_messages;

    buffer topic_;
    buffer contents_;
    publish_options pubopts_;
    v5::properties props_;
    std::shared_ptr<as::steady_timer> tim_message_expiry_;
};

class offline_messages {
public:
    void send_until_fail(endpoint_t& ep) {
        auto& idx = messages_.get<tag_seq>();
        while (!idx.empty()) {
            auto it = idx.begin();

            // const_cast is appropriate here
            // See https://github.com/boostorg/multi_index/issues/50
            auto& m = const_cast<offline_message&>(*it);
            if (m.send(ep)) {
                idx.pop_front();
            }
            else {
                break;
            }
        }
    }

    void clear() {
        messages_.clear();
    }

    bool empty() const {
        return messages_.empty();
    }

    void push_back(
        as::io_context& timer_ioc,
        buffer pub_topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {
        optional<std::chrono::steady_clock::duration> message_expiry_interval;

        auto v = get_property<v5::property::message_expiry_interval>(props);
        if (v) {
            message_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
        }

        std::shared_ptr<as::steady_timer> tim_message_expiry;
        if (message_expiry_interval) {
            tim_message_expiry = std::make_shared<as::steady_timer>(timer_ioc, message_expiry_interval.value());
            tim_message_expiry->async_wait(
                [this, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)](error_code ec) mutable {
                    if (auto sp = wp.lock()) {
                        if (!ec) {
                            messages_.get<tag_tim>().erase(sp);
                        }
                    }
                }
            );
        }

        auto& seq_idx = messages_.get<tag_seq>();
        seq_idx.emplace_back(
            force_move(pub_topic),
            force_move(contents),
            pubopts,
            force_move(props),
            force_move(tim_message_expiry)
        );
    }

private:
    using mi_offline_message = mi::multi_index_container<
        offline_message,
        mi::indexed_by<
            mi::sequenced<
                mi::tag<tag_seq>
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(offline_message, std::shared_ptr<as::steady_timer>, tim_message_expiry_)
            >
        >
    >;

    mi_offline_message messages_;
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_OFFLINE_MESSAGE_HPP

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
        v5::properties props,
        publish_options pubopts,
        std::shared_ptr<as::steady_timer> tim_message_expiry)
        : topic_(force_move(topic)),
          contents_(force_move(contents)),
          props_(force_move(props)),
          pubopts_(pubopts),
          tim_message_expiry_(force_move(tim_message_expiry))
    { }

    void send(endpoint_t& ep) const {
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

        ep.publish(topic_, contents_, pubopts_, force_move(props));
    }

private:
    friend class offline_messages;

    buffer topic_;
    buffer contents_;
    v5::properties props_;
    publish_options pubopts_;
    std::shared_ptr<as::steady_timer> tim_message_expiry_;
};

class offline_messages {
public:
    void send_all_messages(endpoint_t& ep) {
        try {
            auto& idx = messages_.get<tag_seq>();
            while (!idx.empty()) {
                idx.modify(
                    idx.begin(),
                    [&](auto& e) {
                        e.send(ep);
                    }
                );
                idx.pop_front();
            }
        }
        catch (packet_id_exhausted_error const& e) {
            MQTT_LOG("mqtt_broker", warning)
                << MQTT_ADD_VALUE(address, &ep)
                << e.what();
        }
        for (auto const& oflm : messages_) {
            oflm.send(ep);
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

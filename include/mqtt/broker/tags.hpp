// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_TAGS_HPP)
#define MQTT_BROKER_TAGS_HPP

#include <mqtt/config.hpp>

#include <mqtt/broker/broker_namespace.hpp>

MQTT_BROKER_NS_BEGIN

struct tag_seq {};
struct tag_con {};
struct tag_topic{};
struct tag_topic_filter{};
struct tag_con_topic_filter {};
struct tag_cid {};
struct tag_cid_topic_filter {};
struct tag_tim {};
struct tag_pid {};
struct tag_sn_tp {};
struct tag_cid_sn {};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_TAGS_HPP

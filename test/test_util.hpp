// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_TEST_UTIL_HPP)
#define MQTT_TEST_TEST_UTIL_HPP

#include "checker.hpp"
#include <mqtt/sync_client.hpp>

template <typename Client>
inline void connect_no_clean(Client& c) {
    c->set_clean_session(false);
    switch (c->get_protocol_version()) {
    case MQTT_NS::protocol_version::v3_1_1:
        c->connect();
        break;
    case MQTT_NS::protocol_version::v5:
        // set session_expiry_interval as infinity.
        c->connect(
            MQTT_NS::v5::properties{
                MQTT_NS::v5::property::session_expiry_interval(0xFFFFFFFFUL)
            }
        );
        break;
    default:
        BOOST_CHECK(false);
        break;
    }
}

#endif // MQTT_TEST_TEST_UTIL_HPP

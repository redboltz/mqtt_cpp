// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/config.hpp>

#include "../test/system/test_server_no_tls.hpp"
#include <mqtt/setup_log.hpp>

#include <mqtt/broker/broker.hpp>

int main() {
    MQTT_NS::setup_log();
    boost::asio::io_context ioc;
    MQTT_NS::broker::broker_t b(ioc);
    test_server_no_tls s(ioc, b);
    ioc.run();
}

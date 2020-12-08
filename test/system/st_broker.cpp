// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "test_util.hpp"
#include "../common/global_fixture.hpp"

BOOST_AUTO_TEST_SUITE(st_broker)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( broker_bug ) {

    boost::asio::io_context iocb;
    MQTT_NS::broker::broker_t  b(iocb);
    MQTT_NS::optional<test_server_no_tls> s;

    std::thread th(
        [&] {
            s.emplace(iocb, b);
            iocb.run();
        }
    );

    std::vector<std::shared_ptr<std::thread>> client_th;

    std::size_t num_clients = 10;

    auto client_thread_func =
        [] {
            boost::asio::io_context ioc;

            int publish_count = 100;

            auto c1 = MQTT_NS::make_client(ioc, broker_url, broker_notls_port);
            c1->set_clean_session(true);
            c1->set_client_id("cid1");

            c1->set_connack_handler(
                [&c1, &publish_count]
                (bool /*sp*/, MQTT_NS::connect_return_code /*connack_return_code*/) {
                    std::cout << "Publish: " << publish_count << std::endl;
                    for (std::size_t i = 0; i != 100; ++i) {
                        c1->publish("topic1", "topic1_contents1", MQTT_NS::qos::at_most_once);
                    }
                    return true;
                }
            );

            c1->connect();
            ioc.run();
        };

    for (unsigned int i = 0; i != num_clients; ++i) {
        client_th.push_back(std::make_shared<std::thread>(client_thread_func));
    }

    for(auto& th: client_th) {
        th->join();
    }

    as::post(
        iocb,
        [&] {
            s->close();
        }
    );

    th.join();
}

BOOST_AUTO_TEST_SUITE_END()

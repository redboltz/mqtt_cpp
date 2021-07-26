// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "ordered_caller.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(st_length_check)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        clear_ordered();
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
        c->set_client_id("cid1");
        c->set_clean_session(true);


        checker chk = {
            // connect
            cont("h_connack"),
            // subscribe
            cont("h_lc_suback"),
            cont("h_suback"),
            // publish
            cont("h_lc_publish"), // fail
            cont("h_error"),
        };

        c->set_connack_handler(
            [&chk, &c]
            (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == MQTT_NS::connect_return_code::accepted);
                c->subscribe("topic1", MQTT_NS::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&chk, &c, &finish]
            (MQTT_NS::error_code ec) {
                MQTT_CHK("h_error");
                BOOST_TEST(ec == boost::system::errc::protocol_error);
                finish();
                c->force_disconnect();
            });
        c->set_suback_handler(
            [&chk, &c]
            (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> /*results*/) {
                MQTT_CHK("h_suback");
                c->publish("topic1", "topic1_contents", MQTT_NS::qos::at_most_once);
                return true;
            });
        c->set_publish_handler(
            []
            (MQTT_NS::optional<packet_id_t> /*packet_id*/,
             MQTT_NS::publish_options /*pubopts*/,
             MQTT_NS::buffer /*topic*/,
             MQTT_NS::buffer /*contents*/) {
                BOOST_CHECK(false);
                return false;
            });
        c->set_is_valid_length_handler(
            [&chk]
            (MQTT_NS::control_packet_type cpt, std::size_t /*len*/) {
                bool rval = false;;
                auto ret = MQTT_ORDERED(
                    [&] {
                        BOOST_TEST(cpt == MQTT_NS::control_packet_type::suback);
                        MQTT_CHK("h_lc_suback");
                        rval = true;
                    },
                    [&] {
                        BOOST_TEST(cpt == MQTT_NS::control_packet_type::publish);
                        MQTT_CHK("h_lc_publish");
                        rval = false;
                    }
                );
                BOOST_TEST(ret);
                return rval;
            });
        c->connect();
        ioc.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

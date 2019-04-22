// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <iostream>
#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

BOOST_AUTO_TEST_SUITE(test_length_check)

BOOST_AUTO_TEST_CASE( pub_qos0_sub_qos0 ) {
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;

        using packet_id_t = typename std::remove_reference_t<decltype(*c)>::packet_id_t;
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
            (bool sp, std::uint8_t connack_return_code) {
                MQTT_CHK("h_connack");
                BOOST_TEST(sp == false);
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                c->subscribe("topic1", mqtt::qos::at_most_once);
                return true;
            });
        c->set_close_handler(
            []
            () {
                BOOST_CHECK(false);
            });
        c->set_error_handler(
            [&chk, &c, &s]
            (boost::system::error_code const& ec) {
                MQTT_CHK("h_error");
                BOOST_TEST(ec == boost::system::errc::message_size);
                s.close();
                c->force_disconnect();
            });
        c->set_suback_handler(
            [&chk, &c]
            (packet_id_t /*packet_id*/, std::vector<mqtt::optional<std::uint8_t>> /*results*/) {
                MQTT_CHK("h_suback");
                c->publish_at_most_once("topic1", "topic1_contents");
                return true;
            });
        c->set_publish_handler(
            []
            (std::uint8_t /*header*/,
             mqtt::optional<packet_id_t> ,
             std::string /*topic*/,
             std::string /*contents*/) {
                BOOST_CHECK(false);
                return false;
            });
        c->set_is_valid_length_handler(
            [&chk]
            (std::uint8_t cpt, std::size_t /*len*/) {
                bool rval = false;;
                auto ret = chk.match(
                    "h_connack",
                    [&] {
                        BOOST_TEST(cpt == mqtt::control_packet_type::suback);
                        MQTT_CHK("h_lc_suback");
                        rval = true;
                    },
                    "h_suback",
                    [&] {
                        BOOST_TEST(cpt == mqtt::control_packet_type::publish);
                        MQTT_CHK("h_lc_publish");
                        rval = false;
                    }
                );
                BOOST_TEST(ret);
                return rval;
            });
        c->connect();
        ios.run();
        BOOST_TEST(chk.all());
    };
    do_combi_test_sync(test);
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"

#include <mqtt/optional.hpp>

#include <iterator>

BOOST_AUTO_TEST_SUITE(test_message)

BOOST_AUTO_TEST_CASE( publish_empty ) {
    char const buf[] = {};
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_fixed_header ) {
    char const buf[] = {
        0b00110100 // fixed header
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_remaining_length ) {
    char const buf[] = {
        0b00110100, // fixed header
        0b00000000  // remaining length
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_topic_name_length ) {
    char const buf[] = {
        0b00110100, // fixed header
        8,          // remaining length

        0x00,       // topic_name length
        7,          //
        '1'
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id ) {
    char const buf[] = {
        0b00110100, // fixed header
        8,          // remaining length

        0x00,       // topic_name length
        5,          //
        '1',
        '2',
        '3',
        '4',
        '5',
        0x01        // packet_id (half)
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_bad_qos ) {
    char const buf[] = {
        0b00110110,// fixed header (bad qos)
        8,         // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
        0x01,       // packet_id
        0x02        //
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(false);
    }
    catch (mqtt::protocol_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok ) {
    char const buf[] = {
        0b00110100, // fixed header
        8,         // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
        0x01,       // packet_id
        0x02        //
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(true);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok_qos0 ) {
    char const buf[] = {
        0b00110000, // fixed header
        6,         // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(true);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_get_attributes1 ) {
    char const buf[] = {
        0b00110101, // fixed header
        8,         // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
        0x01,       // packet_id
        0x02        //
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(m.is_retain() == true);
        BOOST_TEST(m.is_dup() == false);
        auto t = m.topic();
        auto topic = mqtt::string_view(mqtt::get_pointer(t), mqtt::get_size(t));
        BOOST_TEST(topic == "1234");
        auto p = m.payload();
        auto payload = mqtt::string_view(mqtt::get_pointer(p), mqtt::get_size(p));
        BOOST_TEST(payload == "");
    }
    catch (...) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_get_attributes2 ) {
    char const buf[] = {
        0b00111100, // fixed header
        10,         // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
        0x01,       // packet_id
        0x02,       //
        'A',        // payload
        'B'
    };
    try {
        auto m = mqtt::publish_message(buf, buf + sizeof(buf));
        BOOST_TEST(m.is_retain() == false);
        BOOST_TEST(m.is_dup() == true);
        auto t = m.topic();
        auto topic = mqtt::string_view(mqtt::get_pointer(t), mqtt::get_size(t));
        BOOST_TEST(topic == "1234");
        auto p = m.payload();
        auto payload = mqtt::string_view(mqtt::get_pointer(p), mqtt::get_size(p));
        BOOST_TEST(payload == "AB");
        BOOST_TEST(m.continuous_buffer() == std::string(buf, sizeof(buf)));
    }
    catch (...) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_SUITE_END()

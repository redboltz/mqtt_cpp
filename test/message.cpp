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

using namespace mqtt::literals;

BOOST_AUTO_TEST_CASE( connect_cbuf ) {
    std::string cid = "cid";
    mqtt::optional<mqtt::will> w = mqtt::will("wt"_mb, "wmsg"_mb, false, 0);
    mqtt::optional<std::string> user = std::string("user");
    mqtt::optional<std::string> password = std::string("pw");
    auto m = mqtt::connect_message(
        10,
        cid,
        false,
        w,
        user,
        password
    );
    std::string expected {
        0b0001'0000,
        35,
        0,
        4, // len of MQTT
        'M',
        'Q',
        'T',
        'T',
        4, // version 3.1.1
        static_cast<char>(0b11000100u),
        0,  // keep alive
        10, //
        0,  // cid len
        3,  //
        'c',
        'i',
        'd',
        0,  // will toipic len
        2,  //
        'w',
        't',
        0,  // will message len
        4,  //
        'w',
        'm',
        's',
        'g',
        0,  // user name len
        4,  //
        'u',
        's',
        'e',
        'r',
        0,  // password
        2,  //
        'p',
        'w',
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}


BOOST_AUTO_TEST_CASE( connack_cbuf ) {
    auto m = mqtt::connack_message(true, 0x80);
    std::string expected {
        0b0010'0000,
        0b0000'0010,
        0b0000'0001,
        static_cast<char>(0b1000'0000u),
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_CASE( connack_num_of_cbs ) {
    auto m = mqtt::connack_message(true, 0x80);
    std::string expected {
        0b0010'0000,
        0b0000'0010,
        0b0000'0001,
        static_cast<char>(0b1000'0000u),
    };
    BOOST_TEST(m.num_of_const_buffer_sequence() == 1);
}


BOOST_AUTO_TEST_CASE( publish_empty ) {
    std::string buf;
    try {
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_fixed_header ) {
    std::string buf {
        0b00110100 // fixed header
    };
    try {
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_remaining_length ) {
    std::string buf {
        0b00110100, // fixed header
        0b00000000  // remaining length
    };
    try {
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_topic_name_length ) {
    std::string buf {
        0b00110100, // fixed header
        8,          // remaining length

        0x00,       // topic_name length
        7,          //
        '1'
    };
    try {
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id ) {
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_bad_qos ) {
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (mqtt::protocol_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok ) {
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(true);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok_qos0 ) {
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(true);
    }
    catch (mqtt::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_get_attributes1 ) {
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
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
    std::string buf {
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
        auto m = mqtt::publish_message(buf);
        BOOST_TEST(m.is_retain() == false);
        BOOST_TEST(m.is_dup() == true);
        auto t = m.topic();
        auto topic = mqtt::string_view(mqtt::get_pointer(t), mqtt::get_size(t));
        BOOST_TEST(topic == "1234");
        auto p = m.payload();
        auto payload = mqtt::string_view(mqtt::get_pointer(p), mqtt::get_size(p));
        BOOST_TEST(payload == "AB");
        BOOST_TEST(m.continuous_buffer() == buf);
    }
    catch (...) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( subscribe_cbuf ) {
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> v;
    std::string topic = "tp";
    v.emplace_back(as::buffer(topic), 1);
    auto m = mqtt::subscribe_message(v, 2);
    std::string expected {
        static_cast<char>(0b1000'0010u),
        7,
        0,           // packet id
        2,           //
        0,           // topic filter length
        2,           //
        't',
        'p',
        1,           // qos
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_CASE( suback_cbuf ) {
    auto m = mqtt::suback_message(std::vector<std::uint8_t>{1}, 2);
    std::string expected {
        static_cast<char>(0b1001'0000u),
        3,
        0,           // packet id
        2,           //
        1,           // return code
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_CASE( unsubscribe_cbuf ) {
    std::vector<as::const_buffer> v;
    std::string topic = "tp";
    v.emplace_back(as::buffer(topic));
    auto m = mqtt::unsubscribe_message(v, 2);
    std::string expected {
        static_cast<char>(0b1010'0010u),
        6,
        0,           // packet id
        2,           //
        0,           // topic filter length
        2,           //
        't',
        'p',
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_CASE( pingreq_cbuf ) { // behalf of header only message
    auto m = mqtt::pingreq_message();
    std::string expected {
        static_cast<char>(0b1100'0000u),
        0,
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_SUITE_END()

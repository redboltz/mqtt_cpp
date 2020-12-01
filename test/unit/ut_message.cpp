// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/optional.hpp>
#include <mqtt/message_variant.hpp>

#include <iterator>

BOOST_AUTO_TEST_SUITE(ut_message)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( connect_cbuf ) {
    auto cid = "cid"_mb;
    MQTT_NS::optional<MQTT_NS::will> w = MQTT_NS::will("wt"_mb, "wmsg"_mb, MQTT_NS::retain::no | MQTT_NS::qos::at_most_once);
    MQTT_NS::optional<MQTT_NS::buffer> user = "user"_mb;
    MQTT_NS::optional<MQTT_NS::buffer> password = "pw"_mb;
    auto m = MQTT_NS::connect_message(
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
    auto m = MQTT_NS::connack_message(true, MQTT_NS::connect_return_code::server_unavailable);
    std::string expected {
        0b0010'0000,
        0b0000'0010,
        0b0000'0001,
        0b0000'0011,
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_CASE( connack_num_of_cbs ) {
    auto m = MQTT_NS::connack_message(true, MQTT_NS::connect_return_code::server_unavailable);
    BOOST_TEST(m.num_of_const_buffer_sequence() == 1);
}


BOOST_AUTO_TEST_CASE( publish_empty ) {
    MQTT_NS::buffer buf;
    try {
        auto m = MQTT_NS::publish_message(buf);
        BOOST_TEST(false);
    }
    catch (MQTT_NS::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_fixed_header ) {
    std::string buf {
        0b00110100 // fixed header
    };
    try {
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(false);
    }
    catch (MQTT_NS::remaining_length_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_remaining_length ) {
    std::string buf {
        0b00110100, // fixed header
        0b00000000  // remaining length
    };
    try {
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(false);
    }
    catch (MQTT_NS::remaining_length_error const&) {
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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(false);
    }
    catch (MQTT_NS::remaining_length_error const&) {
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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(false);
    }
    catch (MQTT_NS::remaining_length_error const&) {
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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(false);
    }
    catch (MQTT_NS::protocol_error const&) {
        BOOST_TEST(true);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok ) {
    std::string buf {
        0b00110100, // fixed header
        8,          // remaining length

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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(true);
    }
    catch (MQTT_NS::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_packet_id_ok_qos0 ) {
    std::string buf {
        0b00110000, // fixed header
        6,          // remaining length

        0x00,       // topic_name length
        4,          //
        '1',
        '2',
        '3',
        '4',
    };
    try {
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(true);
    }
    catch (MQTT_NS::remaining_length_error const&) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( publish_get_attributes1 ) {
    std::string buf {
        0b00110101, // fixed header
        8,          // remaining length

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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(m.is_retain() == true);
        BOOST_TEST(m.is_dup() == false);
        BOOST_TEST(m.topic() == "1234");
        BOOST_TEST(m.payload().empty());
        BOOST_TEST(m.payload_as_buffer().empty());
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
        auto m = MQTT_NS::publish_message(MQTT_NS::buffer(MQTT_NS::string_view(buf)));
        BOOST_TEST(m.is_retain() == false);
        BOOST_TEST(m.is_dup() == true);
        BOOST_TEST(m.topic() == "1234");
        BOOST_TEST(m.payload().front() == "AB");
        BOOST_TEST(m.payload_as_buffer() == "AB");
        BOOST_TEST(m.continuous_buffer() == buf);
    }
    catch (...) {
        BOOST_TEST(false);
    }
}

BOOST_AUTO_TEST_CASE( subscribe_cbuf ) {
    static const MQTT_NS::string_view str("tp");
    auto m = MQTT_NS::subscribe_message({ { as::buffer(str.data(), str.size()), MQTT_NS::qos::at_least_once} }, 2);
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
    auto m = MQTT_NS::suback_message({ MQTT_NS::suback_return_code::success_maximum_qos_1 }, 2);
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
    static const MQTT_NS::string_view str("tp");
    auto m = MQTT_NS::unsubscribe_message({ as::buffer(str.data(), str.size()) }, 2);
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
    auto m = MQTT_NS::pingreq_message();
    std::string expected {
        static_cast<char>(0b1100'0000u),
        0,
    };
    BOOST_TEST(m.continuous_buffer() == expected);
}

BOOST_AUTO_TEST_SUITE_END()

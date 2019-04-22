// Copyright udonmo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_main.hpp"
#include "combi_test.hpp"

namespace mqtt {
namespace utf8string {
std::ostream& operator<<(std::ostream& o, validation e) {

    switch (e) {
    case validation::well_formed:
        o << "validation::well_formed";
        break;
    case validation::ill_formed:
        o << "validation::ill_formed";
        break;
    case validation::well_formed_with_non_charactor:
        o << "validation::well_formed_with_non_charactor";
        break;
    }
    return o;
}
} // namespace utf8string
} // namespace mqtt

BOOST_AUTO_TEST_SUITE(test_utf8string_validate)

BOOST_AUTO_TEST_CASE( one_byte ) {
#if defined(MQTT_USE_STR_CHECK)
    using namespace mqtt::utf8string;
    std::string l;

    // one byte charactor

    // nul charactor
    l = {0x00};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // control charactor
    l = {0x01};
    BOOST_TEST(validate_contents(l) == validation::well_formed_with_non_charactor);

    // control charactor
    l = {0x1f};
    BOOST_TEST(validate_contents(l) == validation::well_formed_with_non_charactor);

    // valid charactor(0x20)
    l = {' '};
    BOOST_TEST(validate_contents(l) == validation::well_formed);

    // valid charactor(0x7e)
    l = {'~'};
    BOOST_TEST(validate_contents(l) == validation::well_formed);

    // control charactor
    l = {0x7f};
    BOOST_TEST(validate_contents(l) == validation::well_formed_with_non_charactor);

#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( two_bytes ) {
#if defined(MQTT_USE_STR_CHECK)
    using namespace mqtt::utf8string;
    std::string l;

    // two bytes charactor

    // valid encoded string case 110XXXXx 10xxxxxx
    // included invalid encoded utf8
    // case 110XXXXx 11xxxxxx
    //                ^
    l = {static_cast<char>(0b1100'0010u), static_cast<char>(0b1100'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 110XXXXx 00xxxxxx
    //               ^
    l = {static_cast<char>(0b1100'0010u), static_cast<char>(0b0000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    // included invalid encoded utf8
    // case 111XXXXx 10xxxxxx
    //        ^
    l = {static_cast<char>(0b1110'0010u), static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 100XXXXx 10xxxxxx
    //       ^
    l = {static_cast<char>(0b1000'0010u), static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 010XXXXx 10xxxxxx
    //      ^
    l = {static_cast<char>(0b0100'0010u), static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    // overlong utf8
    // case U+0000
    l = {static_cast<char>(0b1100'0000u), static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // overlong utf8
    // case U+007F
    l = {static_cast<char>(0b1100'0001u), static_cast<char>(0b1011'1111u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    char const *s;
    // control charactor
    s = u8"\u0080";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);

    // control charactor
    s = u8"\u009f";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);


    // valid charactor
    s = u8"\u00a0";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\u07ff";
    BOOST_TEST(validate_contents(s) == validation::well_formed);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( three_bytes ) {
#if defined(MQTT_USE_STR_CHECK)
    using namespace mqtt::utf8string;
    std::string l;

    // three bytes charactor

    // valid encoded string case 1110XXXX 10Xxxxxx 10xxxxxx
    // included invalid encoded utf8
    // case 1110XXXX 10Xxxxxx 11xxxxxx
    //                         ^
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1100'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1110XXXX 10Xxxxxx 00xxxxxx
    //                        ^
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b0000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1110XXXX 11Xxxxxx 10xxxxxx
    //                ^
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1110XXXX 00Xxxxxx 10xxxxxx
    //               ^
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b0010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1111XXXX 10Xxxxxx 10xxxxxx
    //         ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1100XXXX 10Xxxxxx 10xxxxxx
    //        ^
    l = {static_cast<char>(0b1100'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 1010XXXX 10Xxxxxx 10xxxxxx
    //       ^
    l = {static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 0110XXXX 10Xxxxxx 10xxxxxx
    //      ^
    l = {static_cast<char>(0b0110'0000u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    // included overlong utf8
    // case U+0000
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included overlong utf8
    // case U+07FF
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1001'1111u),
        static_cast<char>(0b1011'1111u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    // included surrogate utf8
    // case U+D800
    l = {static_cast<char>(0b1110'1101u),
        static_cast<char>(0b1010'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included surrogate utf8
    // case U+DFFF
    l = {static_cast<char>(0b1110'1101u),
        static_cast<char>(0b1011'1111u),
        static_cast<char>(0b1011'1111u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    char const *s;
    // valid charactor
    s = u8"\u0800";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\ud7ff";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\ue000";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // non charactor
    s = u8"\ufffe";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);

    // non charactor
    s = u8"\uffff";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( four_bytes ) {
#if defined(MQTT_USE_STR_CHECK)
    using namespace mqtt::utf8string;
    std::string l;

    // four bytes charactor


    // valid encoded string case 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx
    // included invalid encoded utf8
    // case 11110XXX 10XXxxxx 10xxxxxx 11xxxxxx
    //                                  ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1100'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11110XXX 10XXxxxx 10xxxxxx 00xxxxxx
    //                                 ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b0000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11110XXX 10XXxxxx 11xxxxxx 10xxxxxx
    //                         ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1100'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11110XXX 10XXxxxx 00xxxxxx 10xxxxxx
    //                        ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b0000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11110XXX 11XXxxxx 10xxxxxx 10xxxxxx
    //                ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1101'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11110XXX 00XXxxxx 10xxxxxx 10xxxxxx
    //               ^
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b0001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11111XXX 10XXxxxx 10xxxxxx 10xxxxxx
    //          ^
    l = {static_cast<char>(0b1111'1000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11100XXX 10XXxxxx 10xxxxxx 10xxxxxx
    //         ^
    l = {static_cast<char>(0b1110'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 11010XXX 10XXxxxx 10xxxxxx 10xxxxxx
    //        ^
    l = {static_cast<char>(0b1101'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 10110XXX 10XXxxxx 10xxxxxx 10xxxxxx
    //       ^
    l = {static_cast<char>(0b1011'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included invalid encoded utf8
    // case 01110XXX 10XXxxxx 10xxxxxx 10xxxxxx
    //      ^
    l = {static_cast<char>(0b0111'0000u),
        static_cast<char>(0b1001'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);


    // included overlong utf8
    // case U+0000
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u),
        static_cast<char>(0b1000'0000u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included overlong utf8
    // case U+FFFF
    l = {static_cast<char>(0b1111'0000u),
        static_cast<char>(0b1000'1111u),
        static_cast<char>(0b1011'1111u),
        static_cast<char>(0b1011'1111u)};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    char const* s;

    // included over U+10FFFF utf8
    // clang++ checks it on compile time and error
    // case U+10FFFF
    // s = u8"\U00110000";
    // BOOST_TEST(validate_contents(s) == validation::ill_formed);

    // valid charactor
    s = u8"\U00010000";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\U0001fffd";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\U00020000";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // valid charactor
    s = u8"\U0010fffd";
    BOOST_TEST(validate_contents(s) == validation::well_formed);

    // non charactor
    s = u8"\u0001fffe";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);

    // non charactor
    s = u8"\u0001ffff";
    BOOST_TEST(validate_contents(s) == validation::well_formed_with_non_charactor);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( combination ) {
#if defined(MQTT_USE_STR_CHECK)
    using namespace mqtt::utf8string;
    std::string l;

    // included invalid charactor
    l = {'a', '\x01', '\x00'};
    BOOST_TEST(validate_contents(l) == validation::ill_formed);

    // included non charactor
    l = {'a', '\x01'};
    BOOST_TEST(validate_contents(l) == validation::well_formed_with_non_charactor);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_overlength_client_id ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string cid(0x10000, 'a');
            c->set_client_id(cid);
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_length_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_client_id ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string cid(1, '\0');
            c->set_client_id(cid);
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_contents_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_overlength_user_name ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string un(0x10000, 'a');
            c->set_user_name(un);
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_length_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_user_name ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string un(1, '\0');
            c->set_user_name(un);
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_contents_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_overlength_will_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string wt(0x10000, 'a');
            c->set_will(mqtt::will(wt, ""));
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_length_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_will_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& /*s*/, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        try {
            std::string wt(1, '\0');
            c->set_will(mqtt::will(wt, ""));
            c->set_clean_session(true);
            c->connect();
            ios.run();
            BOOST_CHECK(false);
        }
        catch (mqtt::utf8string_contents_error const&) {
            BOOST_CHECK(true);
        }
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( publish_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->publish(tp, "topic1_contents", mqtt::qos::at_most_once);
                    return true;
                }
                catch (mqtt::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( publish_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->publish(tp, "topic1_contents", mqtt::qos::at_most_once);
                    return true;
                }
                catch (mqtt::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( subscribe_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->subscribe(tp, mqtt::qos::at_most_once);
                    return true;
                }
                catch (mqtt::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( subscribe_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->subscribe(tp, mqtt::qos::at_most_once);
                    return true;
                }
                catch (mqtt::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( unsubscribe_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->unsubscribe(tp);
                    return true;
                }
                catch (mqtt::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( unsubscribe_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_service& ios, auto& c, auto& s, auto& /*b*/) {
        if (c->get_protocol_version() != mqtt::protocol_version::v3_1_1) return;
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, std::uint8_t) {
                try {
                    c->unsubscribe(tp);
                    return true;
                }
                catch (mqtt::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    s.close();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ios.run();
    };
    do_combi_test(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_SUITE_END()

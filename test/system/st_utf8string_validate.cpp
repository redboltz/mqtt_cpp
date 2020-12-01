// Copyright udonmo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "combi_test.hpp"
#include "../common/global_fixture.hpp"

namespace MQTT_NS {
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
} // namespace MQTT_NS

BOOST_AUTO_TEST_SUITE(st_utf8string_validate)

using namespace MQTT_NS::literals;

BOOST_AUTO_TEST_CASE( connect_overlength_client_id ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string cid(0x10000, 'a');
            c->set_client_id(cid);
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_length_error const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
        catch (boost::bad_numeric_cast const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_client_id ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string cid(1, '\0');
            c->set_client_id(cid);
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_contents_error const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_overlength_user_name ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string un(0x10000, 'a');
            c->set_user_name(un);
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_length_error const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_user_name ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string un(1, '\0');
            c->set_user_name(un);
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_contents_error const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_overlength_will_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string wt(0x10000, 'a');
            c->set_will(MQTT_NS::will(MQTT_NS::buffer(MQTT_NS::string_view(wt)), ""_mb));
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_length_error const&) {
            BOOST_CHECK(true);
            c->force_disconnect();
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( connect_invalid_will_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        try {
            std::string wt(1, '\0');
            c->set_will(MQTT_NS::will(MQTT_NS::buffer(MQTT_NS::string_view(wt)), ""_mb));
            c->set_clean_session(true);
            c->connect();
            ioc.run();
            BOOST_CHECK(false);
        }
        catch (MQTT_NS::utf8string_contents_error const&) {
            c->force_disconnect();
            BOOST_CHECK(true);
            finish();
        }
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( publish_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->publish(tp, "topic1_contents", MQTT_NS::qos::at_most_once);
                    return true;
                }
                catch (MQTT_NS::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
                catch (boost::bad_numeric_cast const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( publish_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->publish(tp, "topic1_contents", MQTT_NS::qos::at_most_once);
                    return true;
                }
                catch (MQTT_NS::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( subscribe_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->subscribe(tp, MQTT_NS::qos::at_most_once);
                    return true;
                }
                catch (MQTT_NS::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( subscribe_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->subscribe(tp, MQTT_NS::qos::at_most_once);
                    return true;
                }
                catch (MQTT_NS::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( unsubscribe_overlength_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(0x10000, 'a');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->unsubscribe(tp);
                    return true;
                }
                catch (MQTT_NS::utf8string_length_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_CASE( unsubscribe_invalid_topic ) {
#if defined(MQTT_USE_STR_CHECK)
    auto test = [](boost::asio::io_context& ioc, auto& c, auto finish, auto& /*b*/) {
        if (c->get_protocol_version() != MQTT_NS::protocol_version::v3_1_1) {
            finish();
            return;
        }
        std::string tp(1, '\0');
        c->set_clean_session(true);
        c->connect();
        c->set_connack_handler(
            [&]
            (bool, MQTT_NS::connect_return_code) {
                try {
                    c->unsubscribe(tp);
                    return true;
                }
                catch (MQTT_NS::utf8string_contents_error const&) {
                    BOOST_CHECK(true);
                    finish();
                    c->force_disconnect();
                    return false;
                }
            }
        );
        ioc.run();
    };
    do_combi_test_sync(test);
#endif // MQTT_USE_STR_CHECK
}

BOOST_AUTO_TEST_SUITE_END()

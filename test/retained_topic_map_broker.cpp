// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "global_fixture.hpp"

#include "retained_topic_map.hpp"

BOOST_AUTO_TEST_SUITE(test_retained_topic_map_broker)

using namespace MQTT_NS::literals;

struct retain {
    retain(
        MQTT_NS::buffer topic,
        MQTT_NS::buffer contents,
        MQTT_NS::v5::properties props,
        MQTT_NS::qos qos_value)
        :topic(MQTT_NS::force_move(topic)),
         contents(MQTT_NS::force_move(contents)),
         props(MQTT_NS::force_move(props)),
         qos_value(qos_value)
    { }
    MQTT_NS::buffer topic;
    MQTT_NS::buffer contents;
    MQTT_NS::v5::properties props;
    MQTT_NS::qos qos_value;
};

using retained_messages = retained_topic_map<retain>;

BOOST_AUTO_TEST_CASE( multi_non_wc_crud ) {
    retained_messages m;

    // publish
    {
        retain r {
            "a/b/c"_mb,
            "contents1"_mb,
            MQTT_NS::v5::properties {},
            MQTT_NS::qos::at_most_once
        };
        m.insert_or_update(r.topic, r);
    }
    {
        retain r {
            "a/b"_mb,
            "contents2"_mb,
            MQTT_NS::v5::properties {},
            MQTT_NS::qos::at_most_once
        };
        m.insert_or_update(r.topic, r);
    }

    // subscribe match
    {
        std::set<MQTT_NS::string_view> msgs {
            "contents1"
        };
        m.find(
            "a/b/c",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }
    {
        std::set<MQTT_NS::string_view> msgs {
            "contents2"
        };
        m.find(
            "a/b",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }

    // remove (publish with empty contents
    {
        m.erase("a/b"_mb);
        m.find(
            "a/b",
            [&](retain const&) {
                BOOST_TEST(false);
            }
        );
        std::set<MQTT_NS::string_view> msgs {
            "contents1"
        };
        m.find(
            "a/b/c",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }
    {
        m.erase("a/b/c"_mb);
        m.find(
            "a/b",
            [&](retain const&) {
                BOOST_TEST(false);
            }
        );
        m.find(
            "a/b/c",
            [&](retain const&) {
                BOOST_TEST(false);
            }
        );
    }
}

BOOST_AUTO_TEST_CASE( multi_wc_crud ) {
    retained_messages m;

    // publish
    {
        retain r {
            "a/b/c"_mb,
            "contents1"_mb,
            MQTT_NS::v5::properties {},
            MQTT_NS::qos::at_most_once
        };
        m.insert_or_update(r.topic, r);
    }
    {
        retain r {
            "a/b"_mb,
            "contents2"_mb,
            MQTT_NS::v5::properties {},
            MQTT_NS::qos::at_most_once
        };
        m.insert_or_update(r.topic, r);
    }

    // subscribe match
    {
        std::set<MQTT_NS::string_view> msgs {
            "contents1",
        };
        m.find(
            "a/+/c",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }
    {
        std::set<MQTT_NS::string_view> msgs {
            "contents1",
            "contents2",
        };
        m.find(
            "a/#",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }
    {
        std::set<MQTT_NS::string_view> msgs {
            "contents1",
            "contents2",
        };
        m.find(
            "a/+/#",
            [&](retain const& v) {
                BOOST_TEST(msgs.erase(v.contents) == 1);
            }
        );
        BOOST_TEST(msgs.empty());
    }

    // remove (publish with empty contents)
    {
        m.erase("a/b"_mb);
        {
            std::set<MQTT_NS::string_view> msgs {
                "contents1",
            };
            m.find(
                "a/#",
                [&](retain const& v) {
                    BOOST_TEST(msgs.erase(v.contents) == 1);
                }
            );
            BOOST_TEST(msgs.empty());
        }
        {
            std::set<MQTT_NS::string_view> msgs {
                "contents1"
            };
            m.find(
                "a/#",
                [&](retain const& v) {
                    BOOST_TEST(msgs.erase(v.contents) == 1);
                }
            );
            BOOST_TEST(msgs.empty());
        }
    }
    {
        m.erase("a/b/c"_mb);
        m.find(
            "a/+/c",
            [&](retain const&) {
                BOOST_TEST(false);
            }
        );
        m.find(
            "#",
            [&](retain const&) {
                BOOST_TEST(false);
            }
        );
    }
}

BOOST_AUTO_TEST_SUITE_END()

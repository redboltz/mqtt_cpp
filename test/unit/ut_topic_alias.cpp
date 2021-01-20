// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/topic_alias_send.hpp>
#include <mqtt/topic_alias_recv.hpp>

BOOST_AUTO_TEST_SUITE(ut_topic_alias)


BOOST_AUTO_TEST_CASE( send ) {
    MQTT_NS::topic_alias_send tas{5};
    tas.insert_or_update("topic1", 1);
    tas.insert_or_update("topic3", 3);
    BOOST_TEST(tas.find(1) == "topic1");
    BOOST_TEST(tas.find(3) == "topic3");
    BOOST_TEST(tas.find(2) == ""); // not registered
    BOOST_TEST(tas.get_lru_alias() == 2); // first vacant
    tas.insert_or_update("topic2", 2);
    BOOST_TEST(tas.get_lru_alias() == 4); // first vacant
    tas.insert_or_update("topic4", 4);
    BOOST_TEST(tas.get_lru_alias() == 5); // first vacant
    tas.insert_or_update("topic5", 5);

    // map fullfilled

    BOOST_TEST(tas.get_lru_alias() == 1); // least recently used
    tas.insert_or_update("topic10", 1);   // update
    BOOST_TEST(tas.get_lru_alias() == 3); // least recently used
    BOOST_TEST(tas.find(1) == "topic10");

    BOOST_TEST(tas.find(3) == "topic3");
    BOOST_TEST(tas.get_lru_alias() == 2); // least recently used

    // find from topic to alias
    BOOST_TEST(tas.find("topic2").value() == 2);
    BOOST_TEST(tas.get_lru_alias() == 2); // LRU doesn't update
    BOOST_TEST(!tas.find("non exist"));

    tas.clear();
    BOOST_TEST(tas.get_lru_alias() == 1);
    BOOST_TEST(tas.find(1) == "");
    BOOST_TEST(tas.find(2) == "");
    BOOST_TEST(tas.find(3) == "");
    BOOST_TEST(tas.find(4) == "");
    BOOST_TEST(tas.find(5) == "");
    tas.insert_or_update("topic1", 1);
    BOOST_TEST(tas.find(1) == "topic1");

}

BOOST_AUTO_TEST_CASE( recv ) {
    MQTT_NS::topic_alias_send tar{5};
    tar.insert_or_update("topic1", 1);
    tar.insert_or_update("topic3", 3);
    BOOST_TEST(tar.find(1) == "topic1");
    BOOST_TEST(tar.find(3) == "topic3");
    BOOST_TEST(tar.find(2) == ""); // not registered
    tar.insert_or_update("topic10", 1);  // update
    BOOST_TEST(tar.find(1) == "topic10");

    tar.clear();

    BOOST_TEST(tar.find(1) == ""); // not registered
    BOOST_TEST(tar.find(3) == ""); // not registered
    tar.insert_or_update("topic1", 1);
    BOOST_TEST(tar.find(1) == "topic1");
}

BOOST_AUTO_TEST_SUITE_END()

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

#include <iostream>

BOOST_AUTO_TEST_SUITE(test_retained_map)

BOOST_AUTO_TEST_CASE(test_retained_topic_map) {
    retained_topic_map<std::string> map;
    map.insert_or_update("a/b/c", "123");
    map.insert_or_update("a/b", "123");

    map.erase("a/b/c");
    BOOST_TEST(map.size() != 1);

    map.erase("a/b");
    BOOST_TEST(map.size() == 1);

    std::vector<std::string> values = {
        "example/test/A", "example/test/B", "example/A/test", "example/B/test"
    };

    for(auto const &i: values) {
        map.insert_or_update(i, i);
    }

    std::vector<std::string> matches;
    map.find(values[0], [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 1);
    BOOST_TEST(matches[0] == values[0]);

    matches = { };
    map.find(values[1], [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 1);
    BOOST_TEST(matches[0] == values[1]);

    matches = { };
    map.find("example/test/+", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 2);
    BOOST_TEST(matches[0] == values[0]);
    BOOST_TEST(matches[1] == values[1]);

    matches = { };
    map.find("example/+/B", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 1);
    BOOST_TEST(matches[0] == values[1]);

    matches = { };
    map.find("example/#", [&matches](std::string const &a) {
        matches.push_back(a);
    });

    BOOST_TEST(matches.size() == 4);

    std::vector<std::string> diff;
    std::sort(matches.begin(), matches.end());
    std::sort(values.begin(), values.end());
    std::set_difference(matches.begin(), matches.end(), values.begin(), values.end(), diff.begin());
    BOOST_TEST(diff.empty());

    BOOST_TEST(map.erase("non-existent") == 0);

    for (auto const& i : values) {
        BOOST_TEST(map.size() != 0);
        BOOST_TEST(map.erase(i) == 1);
    }

    BOOST_TEST(map.size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()

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

#include <boost/format.hpp>
#include <algorithm>
#include <random>

BOOST_AUTO_TEST_SUITE(test_retained_map)

BOOST_AUTO_TEST_CASE(general) {
    retained_topic_map<std::string> map;
    map.insert_or_assign("a/b/c", "123");
    BOOST_TEST(map.size() == 1);
    BOOST_TEST(map.internal_size() == 4);


    BOOST_TEST(map.insert_or_assign("a/b", "123") == 1);
    BOOST_TEST(map.size() == 2);
    BOOST_TEST(map.internal_size() == 4);

    BOOST_TEST(map.insert_or_assign("a/b", "123") == 0);
    BOOST_TEST(map.size() == 2);
    BOOST_TEST(map.internal_size() == 4);

    BOOST_TEST(map.erase("a") == 0);
    BOOST_TEST(map.erase("a") == 0);

    BOOST_TEST(map.erase("a/b/c") == 1);
    BOOST_TEST(map.size() == 1);

    BOOST_TEST(map.erase("a/b") == 1);
    BOOST_TEST(map.size() == 0);
    BOOST_TEST(map.internal_size() == 1);

    std::vector<std::string> values = {
        "example/test/A", "example/test/B", "example/A/test", "example/B/test"
    };

    for (auto const& i : values) {
        map.insert_or_assign(i, i);
    }
    BOOST_TEST(map.size() == 4);

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

    matches = { };
    map.find("#", [&matches](std::string const &a) {
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

    BOOST_TEST(map.size() == 0);
    BOOST_TEST(map.internal_size() == 1);
}

BOOST_AUTO_TEST_CASE(erase_lower_first) {
    retained_topic_map<std::string> map;
    map.insert_or_assign("a/b/c", "1");
    map.insert_or_assign("a/b", "2");

    auto e1 = map.erase("a/b/c"); // erase lower first
    BOOST_TEST(e1 == 1);
    {
        std::set<std::string> match {
            "2",
        };
        map.find(
            "a/b",
            [&](std::string const &v) {
                BOOST_TEST(match.erase(v) == 1);
            }
        );
        BOOST_TEST(match.empty());
    }
    {
        map.find(
            "a/b/c",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }

    auto e2 = map.erase("a/b");
    BOOST_TEST(e2 == 1);
    {
        map.find(
            "a/b/c",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }
    {
        map.find(
            "a/b",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }
}

BOOST_AUTO_TEST_CASE(erase_upper_first) {
    retained_topic_map<std::string> map;
    map.insert_or_assign("a/b/c", "1");
    map.insert_or_assign("a/b", "2");

    auto e1 = map.erase("a/b"); // erase upper first

    BOOST_TEST(e1 == 1);
    {
        std::set<std::string> match {
            "1",
        };
        map.find(
            "a/b/c",
            [&](std::string const &v) {
                BOOST_TEST(match.erase(v) == 1);
            }
        );
        BOOST_TEST(match.empty());
    }
    {
        map.find(
            "a/b",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }

    auto e2 = map.erase("a/b/c");
    BOOST_TEST(e2 == 1);
    {
        map.find(
            "a/b/c",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }
    {
        map.find(
            "a/b",
            [&](std::string const &) {
                BOOST_TEST(false);
            }
        );
    }
}

BOOST_AUTO_TEST_CASE(large_number_of_topics) {
    retained_topic_map<std::pair<std::size_t, std::size_t>> map;

    std::vector< std::pair<std::string, std::pair<std::size_t, std::size_t> > > created_topics;

    constexpr std::size_t num_topics = 25;
    for (std::size_t i = 0; i < num_topics; ++i) {
        for (std::size_t j = 0; j < num_topics; ++j) {
            std::string topic = (boost::format("topic/first_level_%d/second_level_%d") % i % j).str();
            auto value = std::make_pair(i, j);

            map.insert_or_assign(topic, value);

            created_topics.push_back(std::make_pair(topic, value));
        }
    }

    BOOST_TEST(map.size() == num_topics * num_topics);

    std::vector< std::pair<std::size_t, std::size_t> > received_values;
    std::vector< std::pair<std::size_t, std::size_t> > searched_values;

    std::shuffle(created_topics.begin(), created_topics.end(), std::default_random_engine(0x12345));

    for (auto const& i : created_topics) {
       map.find(
            i.first,
            [&](std::pair<std::size_t, std::size_t> const& value) {
                received_values.push_back(value);
            }
        );

        searched_values.push_back(i.second);
    }

    BOOST_TEST(searched_values == received_values);

    for (auto const& i : created_topics) {
        map.erase(i.first);
    }

    BOOST_TEST(map.size() == 0);
    BOOST_TEST(map.internal_size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()

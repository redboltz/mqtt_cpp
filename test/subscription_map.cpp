// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "test_main.hpp"
#include "combi_test.hpp"
#include "checker.hpp"
#include "global_fixture.hpp"

#include "subscription_map.hpp"

#include <iostream>

BOOST_AUTO_TEST_SUITE(test_subscription_map)

BOOST_AUTO_TEST_CASE( failed_erase ) {
    using func_t = std::function<void()>;
    using value_t = std::shared_ptr<func_t>; // shared_ptr for '<' and hash
    using sm_t = multiple_subscription_map<value_t>;

    sm_t m;
    auto v1 = std::make_shared<func_t>([] { std::cout << "v1" << std::endl; });
    auto v2 = std::make_shared<func_t>([] { std::cout << "v2" << std::endl; });

    BOOST_TEST(m.size() == 1);
    auto it_success1 = m.insert("a/b/c", v1);
    assert(it_success1.second);
    BOOST_TEST(m.size() != 1);

    auto it_success2 = m.insert("a/b", v2);
    assert(it_success2.second);
    BOOST_TEST(m.size() != 1);

    auto e1 = m.erase(it_success1.first, v1);
    BOOST_TEST(m.size() != 1);

    auto e2 = m.erase(it_success2.first, v2); //  Invalid handle was specified is thrown here
    BOOST_TEST(m.size() == 1);

}	

BOOST_AUTO_TEST_CASE( test_single_subscription ) {
    std::string text = "example/test/A";

    single_subscription_map< std::string > map;
    auto handle = map.insert(text, text);
    BOOST_TEST(handle.second == "A");
    BOOST_TEST(map.handle_to_subscription(handle) == text);
    BOOST_CHECK_THROW(map.insert(text, text), std::exception);
    map.update(handle, "new_value");
    map.erase(handle);

    BOOST_TEST(map.size() == 1);

    map.insert(text, text);
    BOOST_TEST(map.size() != 1);

    map.erase(text);
    BOOST_TEST(map.size() == 1);

    std::vector<std::string> values = {
        "example/test/A", "example/+/A", "example/#", "#"
    };

    for(auto const &i: values) {
        map.insert(i, i);
    }

    // Attempt to remove entry which has no value
    BOOST_TEST(map.erase("example") == 0);
    BOOST_TEST(map.erase(map.lookup("example")) == 0);
    BOOST_TEST(map.erase("example") == 0);
    BOOST_TEST(map.erase(map.lookup("example")) == 0);

    std::vector<std::string> matches;
    map.find("example/test/A", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 4);

    matches = {};
    map.find("hash_match_only", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 1);

    matches = {};
    map.find("example/hash_only", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 2);

    matches = {};
    map.find("example/plus/A", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 3);

    BOOST_TEST(map.erase("non-existent") == 0);

    for(auto const &i: values) {
        BOOST_TEST(map.size() != 0);
        BOOST_TEST(map.erase(i) == 1);
    }

    BOOST_TEST(map.size() == 1);

    std::vector< single_subscription_map< std::string >::handle > handles;
    for(auto const &i: values) {
        handles.push_back(map.insert(i, i));
    }

    for(auto const &i: handles) {
        BOOST_TEST(map.size() != 0);
        BOOST_TEST(map.erase(i) == 1);
    }

    BOOST_TEST(map.size() == 1);

}

BOOST_AUTO_TEST_CASE( test_multiple_subscription ) {
    std::string text = "example/test/A";

    multiple_subscription_map<std::string> map;

    map.insert("a/b/c", "123");
    map.insert("a/b", "123");

    map.erase("a/b/c", "123");
    BOOST_TEST(map.size() != 1);

    map.erase("a/b", "123");
    BOOST_TEST(map.size() == 1);

    std::vector<std::string> values = {
        "example/test/A", "example/+/A", "example/#", "#"
    };

    // Add some duplicates and overlapping paths
    map.insert(values[0], values[0]);
    BOOST_TEST(map.insert(values[0], values[0]).second == false);
    BOOST_TEST(map.insert(values[0], "blaat").second == true);

    map.erase(values[0], "blaat");
    BOOST_TEST(map.size() != 1);

    map.erase(values[0], values[0]);
    BOOST_TEST(map.size() == 1);

    // Perform test again but this time using handles
    map.insert(values[0], values[0]);
    BOOST_TEST(map.insert(map.lookup(values[0]), values[0]).second == false);
    BOOST_TEST(map.insert(map.lookup(values[0]), "blaat").second == true);

    map.erase(map.lookup(values[0]), "blaat");
    BOOST_TEST(map.size() != 1);

    map.erase(map.lookup(values[0]), values[0]);
    BOOST_TEST(map.size() == 1);

    for(auto const &i: values) {
        map.insert(i, i);
    }

    // Attempt to remove entry which has no value
    BOOST_TEST(map.erase("example", "example") == 0);
    BOOST_TEST(map.erase(map.lookup("example"), "example") == 0);
    BOOST_TEST(map.erase("example", "example") == 0);
    BOOST_TEST(map.erase(map.lookup("example"), "example") == 0);

    BOOST_TEST(map.lookup(values[0]).second == "A");
    BOOST_TEST(map.handle_to_subscription(map.lookup(values[0])) == values[0]);

    std::vector<std::string> matches;
    map.find("example/test/A", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 4);

    matches = {};
    map.find("hash_match_only", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 1);

    matches = {};
    map.find("example/hash_only", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 2);

    matches = {};
    map.find("example/plus/A", [&matches](std::string const &a) {
        matches.push_back(a);
    });
    BOOST_TEST(matches.size() == 3);

    BOOST_TEST(map.erase("non-existent", "non-existent") == 0);

    for(auto const &i: values) {
        BOOST_TEST(map.size() != 0);
        BOOST_TEST(map.erase(i, i) == 1);
    }

    BOOST_TEST(map.size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()

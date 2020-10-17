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

    auto it_success1 = m.insert("a/b/c", v1);
    assert(it_success1.second);
    m.dump(std::cout);
    auto it_success2 = m.insert("a/b", v2);
    assert(it_success2.second);
    m.dump(std::cout);

    auto e1 = m.erase(it_success1.first, v1);
    std::cout << e1 << std::endl;
    m.dump(std::cout);
    auto e2 = m.erase(it_success2.first, v2); //  Invalid handle was specified is thrown here
    std::cout << e2 << std::endl;
    m.dump(std::cout);
}	

BOOST_AUTO_TEST_SUITE_END()

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

BOOST_AUTO_TEST_SUITE(test_subscription_map_broker)

using namespace MQTT_NS::literals;

namespace mi = boost::multi_index;

using endpoint_t = int;
using con_sp_t = std::shared_ptr<endpoint_t>;

struct sub_con_online;
struct sub_con_offline;

// subscription_map manage only a reference of sub_con.
// It is more efficient than using shared_ptr.
// However, managing lifetime is user's responsibility.
// I design that multi_index container of sub_con always has longer lifetime
// than subscription_map.
using sub_con_online_cref = std::reference_wrapper<sub_con_online const>;
using sub_con_offline_cref = std::reference_wrapper<sub_con_offline const>;


struct sub_con_online_cref_hasher
{
    std::size_t operator()(sub_con_online_cref const& s) const noexcept;
};

struct sub_con_offline_cref_hasher
{
    std::size_t operator()(sub_con_offline_cref const& s) const noexcept;
};

struct sub_con_online_cref_equal_to
{
    bool operator()(sub_con_online_cref const& lhs, sub_con_online_cref const& rhs) const noexcept;
};

struct sub_con_offline_cref_equal_to
{
    bool operator()(sub_con_offline_cref const& lhs, sub_con_offline_cref const& rhs) const noexcept;
};

using sub_con_online_map = multiple_subscription_map<sub_con_online_cref, int, sub_con_online_cref_hasher, sub_con_online_cref_equal_to >;
using sub_con_offline_map = multiple_subscription_map<sub_con_offline_cref, int, sub_con_offline_cref_hasher, sub_con_offline_cref_equal_to >;

struct sub_con_online {
    sub_con_online(
        con_sp_t con,
        MQTT_NS::buffer topic_filter,
        MQTT_NS::subscribe_options subopts,
        MQTT_NS::optional<std::size_t> sid)
        :con(MQTT_NS::force_move(con)),
         topic_filter(MQTT_NS::force_move(topic_filter)),
         subopts(subopts),
         sid(sid) {}

    void deliver() {}
    bool from_me(endpoint_t const& ep) const {
        return con.get() == &ep;
    }
    con_sp_t con;
    MQTT_NS::buffer topic_filter;
    MQTT_NS::subscribe_options subopts;
    MQTT_NS::optional<std::size_t> sid;
    MQTT_NS::optional<sub_con_online_map::handle> h; // to efficient remove
};



struct sub_con_offline {
    sub_con_offline(
        MQTT_NS::buffer client_id,
        MQTT_NS::buffer topic_filter,
        MQTT_NS::subscribe_options subopts,
        MQTT_NS::optional<std::size_t> sid)
        :client_id(MQTT_NS::force_move(client_id)),
         topic_filter(MQTT_NS::force_move(topic_filter)),
         subopts(subopts),
         sid(sid) {}

    void deliver() {}
    bool from_me(endpoint_t const&) const {
        return false;
    }
    MQTT_NS::buffer client_id;
    MQTT_NS::buffer topic_filter;
    MQTT_NS::subscribe_options subopts;
    MQTT_NS::optional<std::size_t> sid;
    sub_con_offline_map::handle h; // to efficient remove
};

std::size_t sub_con_online_cref_hasher::operator()(sub_con_online_cref const& s) const noexcept
{
    std::size_t result = 0;
    boost::hash_combine(result, s.get().con);
    boost::hash_combine(result, s.get().topic_filter);
    return result;
}

std::size_t sub_con_offline_cref_hasher::operator()(sub_con_offline_cref const& s) const noexcept
{
    std::size_t result = 0;
    boost::hash_combine(result, s.get().client_id);
    boost::hash_combine(result, s.get().topic_filter);
    return result;
}

bool sub_con_online_cref_equal_to::operator()(sub_con_online_cref const& lhs, sub_con_online_cref const& rhs) const noexcept {
    return lhs.get().con == rhs.get().con && lhs.get().topic_filter == rhs.get().topic_filter;
}

bool sub_con_offline_cref_equal_to::operator()(sub_con_offline_cref const& lhs, sub_con_offline_cref const& rhs) const noexcept {
    return lhs.get().client_id == rhs.get().client_id && lhs.get().topic_filter == rhs.get().topic_filter;
}

inline bool operator<(sub_con_online_cref lhs, sub_con_online_cref rhs) {
    return &lhs.get() < &rhs.get();
}

struct tag_con {};
struct tag_topic_filter {};
struct tag_con_topic_filter {};
struct tag_cid {};
struct tag_cid_topic_filter {};
struct tag_tim {};

using mi_sub_con_online = mi::multi_index_container<
    sub_con_online,
    mi::indexed_by<
        mi::ordered_non_unique<
            mi::tag<tag_topic_filter>,
            BOOST_MULTI_INDEX_MEMBER(sub_con_online, MQTT_NS::buffer, topic_filter)
        >,
        mi::ordered_non_unique<
            mi::tag<tag_con>,
            BOOST_MULTI_INDEX_MEMBER(sub_con_online, con_sp_t, con)
        >,
        // Don't allow the same connection object to have the same topic_filter multiple times.
        // Note that this index does not get used by any code in the broker
        // other than to enforce the uniqueness constraints.
        // Potentially this can be enabled only in debug builds.
        mi::ordered_unique<
            mi::tag<tag_con_topic_filter>,
            mi::composite_key<
                sub_con_online,
                BOOST_MULTI_INDEX_MEMBER(sub_con_online, con_sp_t, con),
                BOOST_MULTI_INDEX_MEMBER(sub_con_online, MQTT_NS::buffer, topic_filter)
            >
        >
    >
>;

using mi_sub_con_offline = mi::multi_index_container<
    sub_con_offline,
    mi::indexed_by<
        // Allow multiple client id's for the same topic_filter
        mi::ordered_non_unique<
            mi::tag<tag_cid>,
            BOOST_MULTI_INDEX_MEMBER(sub_con_offline, MQTT_NS::buffer, client_id)
        >,
        // Allow multiple topic_filters for the same client id
        mi::ordered_non_unique<
            mi::tag<tag_topic_filter>,
            BOOST_MULTI_INDEX_MEMBER(sub_con_offline, MQTT_NS::buffer, topic_filter)
        >,
        // Don't allow the same client id to have the same topic_filter multiple times.
        // Note that this index does not get used by any code in the broker
        // other than to enforce the uniqueness constraints.
        // Potentially this can be enabled only in debug builds.
        mi::ordered_unique<
            mi::tag<tag_cid_topic_filter>,
            mi::composite_key<
                sub_con_offline,
                BOOST_MULTI_INDEX_MEMBER(sub_con_offline, MQTT_NS::buffer, client_id),
                BOOST_MULTI_INDEX_MEMBER(sub_con_offline, MQTT_NS::buffer, topic_filter)
            >
        >
    >
>;

BOOST_AUTO_TEST_CASE( multi_non_wc_crud ) {
    sub_con_online_map m;
    mi_sub_con_online scos;

    auto con1 = std::make_shared<endpoint_t>(1);
    auto con2 = std::make_shared<endpoint_t>(2);

    auto insert_or_update =
        [&](
            con_sp_t const& con,
            MQTT_NS::buffer topic_filter,
            MQTT_NS::subscribe_options subopts,
            MQTT_NS::optional<std::size_t> sid = MQTT_NS::nullopt
        ) {
            auto& idx = scos.get<tag_con_topic_filter>();
            auto it = idx.lower_bound(std::make_tuple(con, topic_filter));
            if ((it == idx.end()) || it->con != con || it->topic_filter != topic_filter) {
                // insert
                return std::make_pair(
                    idx.emplace_hint(it, con, MQTT_NS::force_move(topic_filter), subopts, sid),
                    true
                );
            }
            else {
                // update
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.subopts = subopts;
                        e.sid = sid;
                    }
                );
                return std::make_pair(it, false);
            }
        };

    // new subscribe
    {
        auto it_success = insert_or_update(con1, "a/b/c"_mb, MQTT_NS::qos::at_most_once, 1);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        BOOST_CHECK(!elem.h);
        // new insert or update (insert case)
        auto h = m.insert_or_update(elem.topic_filter, elem, 0);
        auto& idx = scos.get<tag_con_topic_filter>();
        idx.modify(
            it,
            [&](sub_con_online& e) {
                e.h = h.first; // update handle
            }
        );
        BOOST_CHECK(elem.h);
    }
    {
        auto it_success = insert_or_update(con2, "a/b/c"_mb, MQTT_NS::qos::at_most_once, 5);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        if (elem.h) {
            // update
            BOOST_TEST(false);
        }
        else {
            // new insert
            auto h = m.insert_or_update(elem.topic_filter, elem, 0);
            auto& idx = scos.get<tag_con_topic_filter>();
            idx.modify(
                it,
                [&](sub_con_online& e) {
                    e.h = h.first; // update handle
                }
            );
            BOOST_CHECK(elem.h);
        }
    }

    // publish handle
    {
        std::set<int> entries {
            1,
            2
        };

        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                sc.get().from_me(*sc.get().con); // call const member function
                scos.modify(
                    scos.iterator_to(sc.get()),
                    [&](sub_con_online& e) {
                        e.deliver(); // call non const member function
                    }
                );
            }
        );
        BOOST_TEST(entries.empty());
    }

    // unsubscribe (con1 "a/b/c")
    {
        auto& idx = scos.get<tag_con_topic_filter>();
        auto it = idx.find(std::make_tuple(con1, "a/b/c"_mb));
        BOOST_TEST((it != idx.end()));
        m.erase(it->h.value(), *it);
        idx.erase(it);

        std::set<int> entries {
            2
        };
        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                BOOST_TEST(sc.get().con == con2);
                BOOST_TEST(sc.get().subopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_TEST(*sc.get().sid == 5);
            }
        );
        BOOST_TEST(entries.empty());
    }

    // update subscribe (con2 "a/b/c")
    {
        auto it_success = insert_or_update(con2, "a/b/c"_mb, MQTT_NS::qos::at_least_once, 10);
        BOOST_TEST(!it_success.second);

        std::set<int> entries {
            2
        };
        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                BOOST_TEST(sc.get().con == con2);
                BOOST_TEST(sc.get().subopts.get_qos() == MQTT_NS::qos::at_least_once);
                BOOST_TEST(*sc.get().sid == 10);
            }
        );
        BOOST_TEST(entries.empty());
    }

    // unsubscribe or move to offline by disconnect (unsubscribe)
    // This is a biggest reason to use multi_index.
    {
        auto& idx = scos.get<tag_con>();
        auto r = idx.equal_range(con2); // con2 could subscribe multiple topic filters
        auto it = r.first;
        while (it != r.second) {
            auto const& elem = *it;
            // call remove for each handle - Value pairs
            // it is a little inefficient but still enough efficient
            m.erase(elem.h.value(), elem);
            ++it;
        }
        idx.erase(r.first, r.second);

        m.find(
            "a/b/c",
            [&](sub_con_online_cref, int /*value*/) {
                BOOST_TEST(false);
            }
        );

        BOOST_TEST(scos.empty());
    }
}

BOOST_AUTO_TEST_CASE( multi_non_wc_crud_ow ) {
    sub_con_online_map m;
    mi_sub_con_online scos;

    auto con1 = std::make_shared<endpoint_t>(1);
    auto con2 = std::make_shared<endpoint_t>(2);

    auto insert_or_update =
        [&](
            con_sp_t const& con,
            MQTT_NS::buffer topic_filter,
            MQTT_NS::subscribe_options subopts,
            MQTT_NS::optional<std::size_t> sid = MQTT_NS::nullopt
        ) {
            auto& idx = scos.get<tag_con_topic_filter>();
            auto it = idx.lower_bound(std::make_tuple(con, topic_filter));
            if ((it == idx.end()) || it->con != con || it->topic_filter != topic_filter) {
                // insert
                return std::make_pair(
                    idx.emplace_hint(it, con, MQTT_NS::force_move(topic_filter), subopts, sid),
                    true
                );
            }
            else {
                // update
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.subopts = subopts;
                        e.sid = sid;
                    }
                );
                return std::make_pair(it, false);
            }
        };

    // new subscribe
    {
        auto it_success = insert_or_update(con1, "a/b/c"_mb, MQTT_NS::qos::at_most_once, 1);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        BOOST_CHECK(!elem.h);
        // new insert or update (insert case)
        auto h = m.insert_or_update(elem.topic_filter, elem, 0);
        auto& idx = scos.get<tag_con_topic_filter>();
        idx.modify(
            it,
            [&](sub_con_online& e) {
                e.h.emplace(h.first); // update handle
            }
        );
        BOOST_CHECK(elem.h);
    }
    {
        auto it_success = insert_or_update(con2, "a/b"_mb, MQTT_NS::qos::at_most_once, 5);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        if (elem.h) {
            // update
            BOOST_TEST(false);
        }
        else {
            // new insert
            auto h = m.insert_or_update(elem.topic_filter, elem, 0);
            auto& idx = scos.get<tag_con_topic_filter>();
            idx.modify(
                it,
                [&](sub_con_online& e) {
                    e.h = h.first; // update handle
                }
            );
            BOOST_CHECK(elem.h);
        }
    }

    // publish handle
    {
        std::set<int> entries {
            1,
        };

        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                sc.get().from_me(*sc.get().con); // call const member function
                scos.modify(
                    scos.iterator_to(sc.get()),
                    [&](sub_con_online& e) {
                        e.deliver(); // call non const member function
                    }
                );
            }
        );
        BOOST_TEST(entries.empty());
    }
    {
        std::set<int> entries {
            2,
        };

        m.find(
            "a/b",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                sc.get().from_me(*sc.get().con); // call const member function
                scos.modify(
                    scos.iterator_to(sc.get()),
                    [&](sub_con_online& e) {
                        e.deliver(); // call non const member function
                    }
                );
            }
        );
        BOOST_TEST(entries.empty());
    }

    // unsubscribe (con2 "a/b")
    {
        auto& idx = scos.get<tag_con_topic_filter>();
        auto it = idx.find(std::make_tuple(con2, "a/b"_mb));
        BOOST_TEST((it != idx.end()));
        m.erase(it->h.value(), *it);
        idx.erase(it);

        std::set<int> entries {
            1
        };

        m.find(
            "a/b",
            [&](sub_con_online_cref, int /*value*/) {
                BOOST_TEST(false);
            }
        );
        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                BOOST_TEST(sc.get().con == con1);
                BOOST_TEST(sc.get().subopts.get_qos() == MQTT_NS::qos::at_most_once);
                BOOST_TEST(*sc.get().sid == 1);
            }
        );
        BOOST_TEST(entries.empty());
    }
    // unsubscribe (con1 "a/b/c")
    {
        auto& idx = scos.get<tag_con_topic_filter>();
        auto it = idx.find(std::make_tuple(con1, "a/b/c"_mb));
        BOOST_TEST((it != idx.end()));
        m.erase(it->h.value(), *it);
        idx.erase(it);
        m.find(
            "a/b",
            [&](sub_con_online_cref, int /*value*/) {
                BOOST_TEST(false);
            }
        );
        m.find(
            "a/b/c",
            [&](sub_con_online_cref, int /*value*/) {
                BOOST_TEST(false);
            }
        );
    }
}

BOOST_AUTO_TEST_CASE( multi_wc_crud ) {
    sub_con_online_map m;
    mi_sub_con_online scos;

    auto con1 = std::make_shared<endpoint_t>(1);
    auto con2 = std::make_shared<endpoint_t>(2);

    auto insert_or_update =
        [&](
            con_sp_t const& con,
            MQTT_NS::buffer topic_filter,
            MQTT_NS::subscribe_options subopts,
            MQTT_NS::optional<std::size_t> sid = MQTT_NS::nullopt
        ) {
            auto& idx = scos.get<tag_con_topic_filter>();
            auto it = idx.lower_bound(std::make_tuple(con, topic_filter));
            if ((it == idx.end()) || it->con != con || it->topic_filter != topic_filter) {
                // insert
                return std::make_pair(
                    idx.emplace_hint(it, con, MQTT_NS::force_move(topic_filter), subopts, sid),
                    true
                );
            }
            else {
                // update
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.subopts = subopts;
                        e.sid = sid;
                    }
                );
                return std::make_pair(it, false);
            }
        };

    // new subscribe
    {
        auto it_success = insert_or_update(con1, "a/+/c"_mb, MQTT_NS::qos::at_most_once, 1);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        BOOST_CHECK(!elem.h);
        // new insert or update (insert case)
        auto h = m.insert_or_update(elem.topic_filter, elem, 0);
        auto& idx = scos.get<tag_con_topic_filter>();
        idx.modify(
            it,
            [&](sub_con_online& e) {
                e.h = h.first; // update handle
            }
        );
        BOOST_CHECK(elem.h);
    }
    {
        auto it_success = insert_or_update(con2, "a/#"_mb, MQTT_NS::qos::at_most_once, 5);
        BOOST_TEST(it_success.second);
        auto it = it_success.first; // it is const_iterator due to multi_index
        auto& elem = *it;

        if (elem.h) {
            // update
            BOOST_TEST(false);
        }
        else {
            // new insert
            auto h = m.insert_or_update(elem.topic_filter, elem, 0);
            auto& idx = scos.get<tag_con_topic_filter>();
            idx.modify(
                it,
                [&](sub_con_online& e) {
                    e.h = h.first; // update handle
                }
            );
            BOOST_CHECK(elem.h);
        }
    }

    // publish handle
    {
        std::set<int> entries {
            1,
            2
        };

        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                sc.get().from_me(*sc.get().con); // call const member function
                scos.modify(
                    scos.iterator_to(sc.get()),
                    [&](sub_con_online& e) {
                        e.deliver(); // call non const member function
                    }
                );
            }
        );
        BOOST_TEST(entries.empty());
    }
    {
        std::set<int> entries {
            2
        };

        m.find(
            "a/b/d",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                sc.get().from_me(*sc.get().con); // call const member function
                scos.modify(
                    scos.iterator_to(sc.get()),
                    [&](sub_con_online& e) {
                        e.deliver(); // call non const member function
                    }
                );
            }
        );
        BOOST_TEST(entries.empty());
    }

    // unsubscribe (con1 "a/+/c")
    {
        auto& idx = scos.get<tag_con_topic_filter>();
        auto it = idx.find(std::make_tuple(con1, "a/+/c"_mb));
        BOOST_TEST((it != idx.end()));
        m.erase(it->h.value(), *it);
        idx.erase(it);

        std::set<int> entries {
            2
        };
        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                BOOST_TEST(sc.get().con == con2);
                BOOST_TEST(sc.get().subopts.get_qos() == MQTT_NS::qos::at_most_once);
            }
        );
        BOOST_TEST(entries.empty());
    }

    // update subscribe (con2 "a/#")
    {
        auto it_success = insert_or_update(con2, "a/#"_mb, MQTT_NS::qos::at_least_once, 10);
        BOOST_TEST(!it_success.second);

        std::set<int> entries {
            2
        };
        m.find(
            "a/b/c",
            [&](sub_con_online_cref sc, int /*value*/) {
                BOOST_TEST(entries.erase(*sc.get().con) == 1);
                BOOST_TEST(*sc.get().con == 2);
                BOOST_TEST(sc.get().subopts.get_qos() == MQTT_NS::qos::at_least_once);
                BOOST_TEST(*sc.get().sid == 10);
            }
        );
        BOOST_TEST(entries.empty());
    }

    // unsubscribe or move to offline by disconnect (unsubscribe)
    // This is a biggest reason to use multi_index.
    {
        auto& idx = scos.get<tag_con>();
        auto r = idx.equal_range(con2); // con2 could subscribe multiple topic filters
        auto it = r.first;
        while (it != r.second) {
            auto const& elem = *it;
            // call remove for each handle - Value pairs
            // it is a little inefficient but still enough efficient
            m.erase(elem.h.value(), elem);
            ++it;
        }
        idx.erase(r.first, r.second);

        m.find(
            "a/b/c",
            [&](sub_con_online_cref, int /*value*/) {
                BOOST_TEST(false);
            }
        );

        BOOST_TEST(scos.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()

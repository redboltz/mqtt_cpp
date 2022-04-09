// Copyright Wouter van Kleunen 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_RETAINED_TOPIC_MAP_HPP)
#define MQTT_BROKER_RETAINED_TOPIC_MAP_HPP

#include <deque>

#include <boost/functional/hash.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/buffer.hpp>

#include <mqtt/broker/topic_filter.hpp>

MQTT_BROKER_NS_BEGIN

namespace mi = boost::multi_index;

template<typename Value>
class retained_topic_map {

    // Exceptions used
    static void throw_max_stored_topics() { throw std::overflow_error("Retained map maximum number of topics reached"); }
    static void throw_no_wildcards_allowed() { throw std::runtime_error("Retained map no wildcards allowed in retained topic name"); }

    using node_id_t = std::size_t;

    static constexpr node_id_t root_parent_id = 0;
    static constexpr node_id_t root_node_id = 1;
    static constexpr node_id_t max_node_id = std::numeric_limits<node_id_t>::max();

    struct path_entry {
        node_id_t parent_id;
        buffer name_buffer;
        string_view name;

        node_id_t id;

        std::size_t count = 1;
        static constexpr std::size_t max_count = std::numeric_limits<std::size_t>::max();

        // Increase the count for this node
        void increase_count() {
            if (count == max_count) {
                throw_max_stored_topics();
            }

            ++count;
        }

        // Decrease the count for this node
        void decrease_count() {
            BOOST_ASSERT(count >= count);
            --count;
        }

        optional<Value> value;

        path_entry(node_id_t parent_id, string_view name, node_id_t id)
            : parent_id(parent_id), name_buffer(allocate_buffer(name)), name(name_buffer), id(id)
        { }
    };

    struct wildcard_index_tag { };
    struct direct_index_tag { };

    // allow for two indices on retained topics
    using path_entry_set = mi::multi_index_container<
      path_entry,
      mi::indexed_by<
        // index required for direct child access
        mi::hashed_unique <
            mi::tag<direct_index_tag>,
            mi::composite_key<path_entry,
                BOOST_MULTI_INDEX_MEMBER(path_entry, node_id_t, parent_id),
                BOOST_MULTI_INDEX_MEMBER(path_entry, string_view, name) >
            >,

        // index required for wildcard processing
        mi::ordered_non_unique< mi::tag<wildcard_index_tag>, BOOST_MULTI_INDEX_MEMBER(path_entry, node_id_t, parent_id) >
      >
    >;

    using direct_const_iterator = typename path_entry_set::template index<direct_index_tag>::type::const_iterator;
    using wildcard_const_iterator = typename path_entry_set::template index<wildcard_index_tag>::type::const_iterator;

    path_entry_set map;
    size_t map_size;
    node_id_t next_node_id;

    direct_const_iterator root;

    direct_const_iterator create_topic(string_view topic) {
         direct_const_iterator parent = root;

        topic_filter_tokenizer(
            topic,
            [this, &parent](string_view t) {
                if (t == "+" || t == "#") {
                    throw_no_wildcards_allowed();
                }

                node_id_t parent_id = parent->id;

                auto& direct_index = map.template get<direct_index_tag>();
                direct_const_iterator entry = direct_index.find(std::make_tuple(parent_id, t));

                if (entry == direct_index.end()) {
                    entry = map.insert(path_entry(parent->id, t, next_node_id++)).first;
                    if (next_node_id == max_node_id) {
                        throw_max_stored_topics();
                    }
                }
                else {
                    direct_index.modify(entry, [](path_entry& entry){ entry.increase_count(); });
                }

                parent = entry;
                return true;
            }
        );

        return parent;
    }

    std::vector<direct_const_iterator> find_topic(string_view topic) {
        std::vector<direct_const_iterator> path;
        direct_const_iterator parent = root;

        topic_filter_tokenizer(
            topic,
            [this, &parent, &path](string_view t) {
                auto const& direct_index = map.template get<direct_index_tag>();
                auto entry = direct_index.find(std::make_tuple(parent->id, t));

                if (entry == direct_index.end()) {
                    path = std::vector<direct_const_iterator>();
                    return false;
                }

                path.push_back(entry);
                parent = entry;
                return true;
            }
        );

        return path;
    }

    // Match all underlying topics when a hash entry is matched
    // perform a breadth-first iteration over all items in the tree below
    template<typename Output>
    void match_hash_entries(node_id_t parent, Output&& callback, bool ignore_system) const {
        std::deque<node_id_t> entries;
        entries.push_back(parent);
        std::deque<node_id_t> new_entries;

        auto const& wildcard_index = map.template get<wildcard_index_tag>();

        while (!entries.empty()) {
            new_entries.resize(0);

            for (auto root : entries) {
                // Find all entries below this node
                for (auto i = wildcard_index.lower_bound(root); i != wildcard_index.end() && i->parent_id == root; ++i) {

                    // Should we ignore system matches
                    if (!ignore_system || i->name.empty() || i->name[0] != '$') {
                        if (i->value) {
                            callback(*i->value);
                        }

                        new_entries.push_back(i->id);
                    }
                }
            }

            // Ignore system only on first level
            ignore_system = false;
            std::swap(entries, new_entries);
        }

    }

    // Find all topics that match the specified topic filter
    template<typename Output>
    void find_match(string_view topic_filter, Output&& callback) const {
        std::deque<direct_const_iterator> entries;
        entries.push_back(root);

        std::deque<direct_const_iterator> new_entries;
        topic_filter_tokenizer(
            topic_filter,
            [this, &entries, &new_entries, &callback](string_view t) {
                auto const& direct_index = map.template get<direct_index_tag>();
                auto const& wildcard_index = map.template get<wildcard_index_tag>();
                new_entries.resize(0);

                for (auto const& entry : entries) {
                    node_id_t parent = entry->id;

                    if (t == string_view("+")) {
                        for (auto i = wildcard_index.lower_bound(parent); i != wildcard_index.end() && i->parent_id == parent; ++i) {
                            if (parent != root_node_id || i->name.empty() || i->name[0] != '$') {
                                new_entries.push_back(map.template project<direct_index_tag, wildcard_const_iterator>(i));
                            }
                            else {
                                break;
                            }
                        }
                    }
                    else if (t == string_view("#")) {
                        match_hash_entries(parent, callback, parent == root_node_id);
                        return false;
                    }
                    else {
                        direct_const_iterator i = direct_index.find(std::make_tuple(parent, t));
                        if (i != direct_index.end()) {
                            new_entries.push_back(i);
                        }
                    }
                }

                std::swap(new_entries, entries);
                return !entries.empty();
            }
        );

        for (auto& entry : entries) {
            if (entry->value) {
                callback(*entry->value);
            }
        }
    }

    // Remove a value at the specified topic
    size_t erase_topic(string_view topic) {
        auto path = find_topic(topic);

        // Reset the value if there is actually something stored
        if (!path.empty() && path.back()->value) {
            auto& direct_index = map.template get<direct_index_tag>();
            direct_index.modify(path.back(), [](path_entry &entry){ entry.value = nullopt; });

            // Do iterators stay valid when erasing ? I think they do ?
            for (auto entry : path) {
                direct_index.modify(entry, [](path_entry& entry){ entry.decrease_count(); });

                if (entry->count == 0) {
                    map.erase(entry);
                }
            }

            return 1;
        }

        return 0;
    }

    // Increase the number of topics for this path
    void increase_topics(std::vector<direct_const_iterator> const &path) {
        auto& direct_index = map.template get<direct_index_tag>();

        for(auto& i : path) {
            direct_index.modify(i, [](path_entry& entry){ entry.increase_count(); });
        }
    }

    // Increase the map size (total number of topics stored)
    void increase_map_size() {
        if(map_size == std::numeric_limits<decltype(map_size)>::max()) {
            throw_max_stored_topics();
        }

        ++map_size;
    }

    // Decrease the map size (total number of topics stored)
    void decrease_map_size(size_t count) {
        BOOST_ASSERT(map_size >= count);
        map_size -= count;
    }

    void init_map() {
        map_size = 0;
        // Create the root node
        root = map.insert(path_entry(root_parent_id, "", root_node_id)).first;
        next_node_id = root_node_id + 1;
    }

public:
    retained_topic_map()
    {
        init_map();
    }

    // Insert a value at the specified topic
    template<typename V>
    std::size_t insert_or_assign(string_view topic, V&& value) {
        auto& direct_index = map.template get<direct_index_tag>();
        auto path = this->find_topic(topic);

        if (path.empty()) {
            auto new_topic = this->create_topic(topic);
            direct_index.modify(new_topic, [&value](path_entry &entry) mutable { entry.value.emplace(std::forward<V>(value)); });
            increase_map_size();
            return 1;
        }

        if (!path.back()->value) {
            this->increase_topics(path);
            direct_index.modify(path.back(), [&value](path_entry &entry) mutable { entry.value.emplace(std::forward<V>(value)); });
            increase_map_size();
            return 1;
        }

        direct_index.modify(path.back(), [&value](path_entry &entry) mutable { entry.value.emplace(std::forward<V>(value)); });

        return 0;
    }

    // Find all stored topics that math the specified topic_filter
    template<typename Output>
    void find(string_view topic_filter, Output&& callback) const {
        find_match(topic_filter, std::forward<Output>(callback));
    }

    // Remove a stored value at the specified topic
    std::size_t erase(string_view topic) {
        auto result = erase_topic(topic);
        decrease_map_size(result);
        return result;
    }

    // Get the number of entries stored in the map
    std::size_t size() const { return map_size; }

    // Get the number of entries in the map (for debugging purpose only)
    std::size_t internal_size() const { return map.size(); }

    // Clear all topics
    void clear() {
        map.clear();
        init_map();
    }

    // Dump debug information
    template<typename Output>
    void dump(Output &out) {
        auto const& direct_index = map.template get<direct_index_tag>();
        for (auto const& i : direct_index) {
            out << i.parent_id << " " << i.name << " " << (i.value ? "init" : "-") << " " << i.count << '\n';
        }
    }

};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_RETAINED_TOPIC_MAP_HPP

// Copyright Wouter van Kleunen 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIPTION_MAP_HPP)
#define MQTT_SUBSCRIPTION_MAP_HPP

#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/optional.hpp>

#include <boost/range/adaptor/reversed.hpp>
#include "topic_filter_tokenizer.hpp"

/**
 *
 * In MQTT we have:
 *    Clients subscribed with certain topic filters, topic filters are path with may contain wildcards such as
 *    + and #
 *       . A subscription to "#" will not receive any messages published to a topic beginning with a $
 *       · A subscription to "+/monitor/Clients" will not receive any messages published to "$SYS/monitor/Clients"
 *       · A subscription to "$SYS/#" will receive messages published to topics beginning with "$SYS/"
 *       · A subscription to "$SYS/monitor/Clients/+" will receive messages published to "$SYS/monitor/Clients/"
 *       · For a Client to receive messages from topics that begin with $SYS/ and from topics that don’t begin with a $, it has to subscribe to both “#” and “$SYS/#”
 *    Check whether a string is a valid subscription using 'mqtt_valid_subscription'
 *
 *    Topics being published, a topic is a sort of path and does not contain wildcards
 *       · $SYS/ has been widely adopted as a prefix to topics that contain Server-specific information or control APIs
 *       · Applications cannot use a topic with a leading $ character for their own purposes
 *    Check whether a string is a valid topic using 'mqtt_valid_topic'
 *
 *
 *    We introduce two data structures:
 *       . A subscription map, storing a topic_filter -> data
 *           Using a published topic, we can find all topic filters which match the specified topic
 *       . A stored topic map, storing topic -> data
 *           Using a new topic filter, we can find all stored topics which match the specified topic filter
 */

template<typename Value>
class subscription_map_base {
public:
    using node_id_t = std::size_t;
    using path_entry_key = std::pair< node_id_t, MQTT_NS::buffer>;
    using handle = path_entry_key;

    static constexpr node_id_t root_node_id = 0;

private:
    struct path_entry {
        node_id_t id;
        path_entry_key parent;

        std::size_t count = 1;

        // the following flags and values are stored in count
        static constexpr std::size_t has_hash_child_flag = (static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 1));
        static constexpr std::size_t has_plus_child_flag = (static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 2));
        static constexpr std::size_t max_count = (has_plus_child_flag - 1);

        Value value;

        path_entry(node_id_t id, path_entry_key parent)
            : id(id), parent(parent)
        {}
    };

    using this_type = subscription_map_base<Value>;

    using map_type = std::unordered_map< path_entry_key, path_entry, boost::hash< path_entry_key > >;
    using map_type_iterator = typename map_type::iterator;
    using map_type_const_iterator = typename map_type::const_iterator;

    map_type map;
    map_type_iterator root;
    path_entry_key root_key;
    node_id_t next_node_id = root_node_id;

protected:
    size_t map_size = 0;

    map_type_iterator get_key(path_entry_key key) { return map.find(key); }
    map_type_iterator begin() { return map.begin(); }
    map_type_iterator end() { return map.end(); }
    map_type const& get_map() const { return map; }

    handle path_to_handle(std::vector< map_type_iterator > const& path) const {
        return path.back()->first;
    }

    std::vector< map_type_iterator> find_topic_filter(MQTT_NS::string_view topic_filter) {
        auto parent = root;
        std::vector< map_type_iterator > path;

        topic_filter_tokenizer(
            topic_filter,
            [this, &path, &parent](MQTT_NS::string_view t) {
                auto entry = map.find(path_entry_key(parent->second.id, t));

                if (entry == map.end()) {
                    path = std::vector<map_type_iterator>();
                    return false;
                }

                path.push_back(entry);
                parent = entry;
                return true;
            }
        );

        return path;
    }

    void throw_max_filters_exception() const { throw std::overflow_error("Maximum number of stored topic filters reached"); }

    std::vector<map_type_iterator> create_topic_filter(MQTT_NS::string_view topic_filter) {
        auto parent = root;

        // Check on root entry if we can still add an entry
        if ((parent->second.count & path_entry::max_count) == path_entry::max_count) {
            throw_max_filters_exception();
        }

        std::vector<map_type_iterator> result;

        topic_filter_tokenizer(
            topic_filter,
            [this, &parent, &result](MQTT_NS::string_view t) {
                auto parent_id = parent->second.id;
                auto entry = map.find(path_entry_key(parent_id, t));

                if (entry == map.end()) {
                    entry =
                        map.emplace(
                            path_entry_key(
                                parent_id,
                                MQTT_NS::allocate_buffer(t)
                            ),
                            path_entry(next_node_id++, parent->first)
                        ).first;
                    if (t == "+") {
                        parent->second.count |= path_entry::has_plus_child_flag;
                    }

                    if (t == "#") {
                        parent->second.count |= path_entry::has_hash_child_flag;
                    }

                    if (next_node_id == std::numeric_limits<node_id_t>::max()) {
                        throw_max_filters_exception();
                    }

                }
                else {
                    entry->second.count++;
                }

                result.push_back(entry);
                parent = entry;
                return true;
            }
        );

        return result;
    }

    // Remove a value at the specified path
    void remove_topic_filter(std::vector< map_type_iterator > const& path) {
        bool remove_plus_child_flag = false;
        bool remove_hash_child_flag = false;

        // Go through entries to remove
        for (auto entry = path.rbegin(); entry != path.rend(); ++entry) {
            if (remove_plus_child_flag) {
                (*entry)->second.count &= ~path_entry::has_plus_child_flag;
                remove_plus_child_flag = false;
            }

            if (remove_hash_child_flag) {
                (*entry)->second.count &= ~path_entry::has_hash_child_flag;
                remove_hash_child_flag = false;
            }

            --(*entry)->second.count;
            if (((*entry)->second.count & path_entry::max_count) == 0) {
                remove_plus_child_flag = ((*entry)->first.second == "+");
                remove_hash_child_flag = ((*entry)->first.second == "#");
                map.erase((*entry)->first);
            }
        }

        if (remove_plus_child_flag) {
            root->second.count &= ~path_entry::has_plus_child_flag;
        }

        if (remove_hash_child_flag) {
            root->second.count &= ~path_entry::has_hash_child_flag;
        }
    }

    template <typename ThisType, typename Output>
    static void find_match_impl(ThisType& self, MQTT_NS::string_view topic, Output&& callback) {
        using iterator_type = decltype(self.map.end()); // const_iterator or iterator depends on self

        std::vector<iterator_type> entries;
        entries.push_back(self.root);


        topic_filter_tokenizer(
            topic,
            [&self, &entries, &callback](MQTT_NS::string_view t) {
                std::vector<iterator_type> new_entries;

                for (auto& entry : entries) {
                    auto parent = entry->second.id;
                    auto i = self.map.find(path_entry_key(parent, t));
                    if (i != self.map.end()) {
                        new_entries.push_back(i);
                    }

                    if (entry->second.count & path_entry::has_plus_child_flag) {
                        i = self.map.find(path_entry_key(parent, MQTT_NS::string_view("+")));
                        if (i != self.map.end()) {
                            if (parent != self.root_node_id || t.empty() || t[0] != '$') {
                                new_entries.push_back(i);
                            }
                        }
                    }

                    if (entry->second.count & path_entry::has_hash_child_flag) {
                        i = self.map.find(path_entry_key(parent, MQTT_NS::string_view("#")));
                        if (i != self.map.end()) {
                            if (parent != self.root_node_id || t.empty() || t[0] != '$'){
                                callback(i->second.value);
                            }
                        }
                    }
                }

                std::swap(entries, new_entries);
                return !entries.empty();
            }
        );

        for (auto& entry : entries) {
            callback(entry->second.value);
        }
    }

    // Find all topic filters that match the specified topic
    template<typename Output>
    void find_match(MQTT_NS::string_view topic, Output&& callback) const {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    // Find all topic filters and allow modification
    template<typename Output>
    void modify_match(MQTT_NS::string_view topic, Output callback) {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    template<typename Output>
    void handle_to_iterators(handle h, Output output) {
        auto i = h;
        while(true) {
            if (i == root_key) {
                return;
            }

            auto entry_iter = map.find(i);
            if (entry_iter == map.end()) {
                throw std::runtime_error("Invalid handle was specified");
            }

            output(entry_iter);
            i = entry_iter->second.parent;
        }
    }

    // Get the iterators of a handle
    std::vector<map_type_iterator> handle_to_iterators(handle h) {
        std::vector<map_type_iterator> result;
        handle_to_iterators(h, [&result](map_type_iterator i) { result.push_back(i); });
        std::reverse(result.begin(), result.end());
        return result;
    }

    // Increase the number of subscriptions for this handle
    void increase_subscriptions(handle h) {
        handle_to_iterators(h, [](map_type_iterator i) { ++(i->second.count); });
    }

    // Increase the number of subscriptions for this path
    void increase_subscriptions(std::vector<map_type_iterator> const &path) {
        for (auto i : path) {
            ++(i->second.count);
        }
    }

    subscription_map_base()
        : root_key(path_entry_key(std::numeric_limits<node_id_t>::max(), MQTT_NS::allocate_buffer("")))
    {
        // Create the root node
        root = map.emplace(root_key, path_entry(root_node_id, path_entry_key())).first;
        ++next_node_id;
    }

public:
    // Return the number of elements in the tree
    std::size_t internal_size() const { return map.size(); }

    // Return the number of registered topic filters
    std::size_t size() const { return this->map_size; }

    // Lookup a topic filter
    MQTT_NS::optional<handle> lookup(MQTT_NS::string_view topic_filter) {
        auto path = this->find_topic_filter(topic_filter);
        if(path.empty())
            return MQTT_NS::optional<handle>();
        else
            return this->path_to_handle(std::move(path));
    }

    // Get path of topic_filter
    std::string handle_to_topic_filter(handle h) {
        std::string result;

        handle_to_iterators(h, [&result](map_type_iterator i) {
            if (result.empty())
                result = std::string(i->first.second);
            else
                result = std::string(i->first.second) + "/" + result;
        });

        return result;
    }
};

template<typename Value>
class single_subscription_map
    : public subscription_map_base< MQTT_NS::optional<Value> > {

public:

    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a value at the specified topic_filter
    template <typename V>
    handle insert(MQTT_NS::string_view topic_filter, V&& value) {
        auto existing_subscription = this->find_topic_filter(topic_filter);
        if (!existing_subscription.empty()) {
            if(existing_subscription.back()->second.value)
                throw std::runtime_error("Subscription already exists in map");
            existing_subscription.back()->second.value.emplace(std::forward<V>(value));
            return this->path_to_handle(std::move(existing_subscription));
        }

        auto new_topic_filter = this->create_topic_filter(topic_filter);
        new_topic_filter.back()->second.value = value;
        ++this->map_size;
        return this->path_to_handle(new_topic_filter);
    }

    // Update a value at the specified topic filter
    template <typename V>
    void update(MQTT_NS::string_view topic_filter, V&& value) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            throw std::runtime_error("Invalid topic filter was specified");
        }

        path.back()->second.value.emplace(std::forward<V>(value));
    }

    template <typename V>
    void update(handle h, V&& value) {
        auto entry_iter = this->get_key(h);
        if (entry_iter == this->end()) {
            throw std::runtime_error("Invalid topic filter was specified");
        }
        entry_iter->second.value.emplace(std::forward<V>(value));
    }

    // Remove a value at the specified topic filter
    std::size_t erase(MQTT_NS::string_view topic_filter) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_topic_filter(path);
        --this->map_size;
        return 1;
    }

    // Remove a value using a handle
    std::size_t erase(handle h) {
        auto path = this->handle_to_iterators(h);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_topic_filter(path);
        --this->map_size;
        return 1;
    }

    // Find all topic filters that match the specified topic
    template<typename Output>
    void find(MQTT_NS::string_view topic, Output callback) const {
        this->find_match(
            topic,
            [&callback]( MQTT_NS::optional<Value> const& value ) {
                if (value) {
                    callback(value.value());
                }
            }
        );
    }

};

template<typename Key, typename Value, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>, class Cont = std::unordered_map<Key, Value, Hash, Pred, std::allocator< std::pair<const Key, Value> > > >
class multiple_subscription_map
    : public subscription_map_base< Cont >
{

public:
    using container_t = Cont;

    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a key => value at the specified topic filter
    // returns the handle and true if key was inserted, false if key was updated
    template <typename K, typename V>
    std::pair<handle, bool> insert_or_assign(MQTT_NS::string_view topic_filter, K&& key, V&& value) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            auto new_topic_filter = this->create_topic_filter(topic_filter);
            new_topic_filter.back()->second.value.emplace(std::forward<K>(key), std::forward<V>(value));
            ++this->map_size;
            return std::make_pair(this->path_to_handle(std::move(new_topic_filter)), true);
        }
        else {
            auto& subscription_set = path.back()->second.value;

#if __cpp_lib_unordered_map_try_emplace >= 201411L
            auto insert_result = subscription_set.insert_or_assign(std::forward<K>(key), std::forward<V>(value));
            if(insert_result.second) {
                this->increase_subscriptions(path);
                ++this->map_size;
            }
            return std::make_pair(this->path_to_handle(std::move(path)), insert_result.second);
#else
            auto iter = subscription_set.find(key);
            if(iter == subscription_set.end()) {
                subscription_set.emplace(std::forward<K>(key), std::forward<V>(value));
                this->increase_subscriptions(path);
                ++this->map_size;
            } else {
                iter->second = std::forward<V>(value);
            }
            return std::make_pair(this->path_to_handle(std::move(path)), iter == subscription_set.end());

#endif
        }
    }

    // Insert a key => value with a handle to the topic filter
    // returns the handle and true if key was inserted, false if key was updated
    template <typename K, typename V>
    std::pair<handle, bool> insert_or_assign(handle h, K&& key, V&& value) {
        // Remove the specified value
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        auto& subscription_set = h_iter->second.value;

#if __cpp_lib_unordered_map_try_emplace >= 201411L
        auto insert_result = subscription_set.insert_or_assign(std::forward<K>(key), std::forward<V>(value));
        if(insert_result.second) {
            this->increase_subscriptions(h);
            ++this->map_size;
        }
        return std::make_pair(std::move(h), insert_result.second);
#else
        auto iter = subscription_set.find(key);
        if(iter == subscription_set.end()) {
            subscription_set.emplace(std::forward<K>(key), std::forward<V>(value));
            this->increase_subscriptions(h);
            ++this->map_size;
        } else {
            iter->second = std::forward<V>(value);
        }
        return std::make_pair(std::move(h), iter == subscription_set.end());
#endif
    }

    // Remove a value at the specified handle
    // returns the number of removed elements
    std::size_t erase(handle h, Key const& key) {
        // Remove the specified value
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        // Remove the specified value
        auto& subscription_set = h_iter->second.value;
        auto result = subscription_set.erase(key);
        if (result) {
            this->remove_topic_filter(this->handle_to_iterators(h));
            --this->map_size;
        }

        return result;
    }

    // Remove a value at the specified topic filter
    // returns the number of removed elements
    std::size_t erase(MQTT_NS::string_view topic_filter, Key const& key) {
        // Find the topic filter in the map
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            return 0;
        }

        // Remove the specified value
        auto result = path.back()->second.value.erase(key);
        if (result) {
            --this->map_size;
            this->remove_topic_filter(path);
        }

        return result;
    }

    // Find all topic filters that match the specified topic
    template<typename Output>
    void find(MQTT_NS::string_view topic, Output callback) const {
        this->find_match(
            topic,
            [&callback]( Cont const &values ) {
                for (auto const& i : values) {
                    callback(i.first, i.second);
                }
            }
        );
    }

    // Find all topic filters that match and allow modification
    template<typename Output>
    void modify(MQTT_NS::string_view topic, Output callback) {
        auto modify_callback = [&callback]( Cont &values ) {
            for (auto& i : values) {
                callback(i.first, i.second);
            }
        };

        this->template modify_match<decltype(modify_callback)>(topic, modify_callback);
    }

    template<typename Output>
    void dump(Output &out) {
        for (auto const& i: this->get_map()) {
            out << i.first.first << " " << i.first.second << " " << i.second.value.size() << " " << i.second.count << std::endl;
        }
    }

};

#endif // MQTT_SUBSCRIPTION_MAP_HPP

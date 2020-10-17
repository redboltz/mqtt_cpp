// Copyright wkl04 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIPTION_MAP_HPP)
#define MQTT_SUBSCRIPTION_MAP_HPP

#include <mqtt/string_view.hpp>

#include <unordered_map>
#include <boost/functional/hash.hpp>
#include "topic_filter_tokenizer.hpp"

#include <sstream>

/**
 *
 * In MQTT we have:
 *    Clients subscribed with certain subscriptions, subscriptions are path with may contain wildcards such as
 *    + and #
 *       . A subscription to “#” will not receive any messages published to a topic beginning with a $
 *       · A subscription to “+/monitor/Clients” will not receive any messages published to “$SYS/monitor/Clients”
 *       · A subscription to “$SYS/#” will receive messages published to topics beginning with “$SYS/”
 *       · A subscription to “$SYS/monitor/+” will receive messages published to “$SYS/monitor/Clients”
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
 *       . A subscription map, storing a subscription -> data
 *           Using a published topic, we can find all subscriptions which match the specified topic
 *       . A stored topic map, storing topic -> data
 *           Using a new subscription, we can find all topics which match the specified subscription
 */

template<typename Value>
class subscription_map_base {
public:
    using node_id_t = size_t;
    using path_entry_key = std::pair< node_id_t, MQTT_NS::buffer>;
    using handle = std::vector<path_entry_key>;

    static constexpr node_id_t root_node_id = 0;

private:
    struct path_entry {
        node_id_t id;

        size_t count = 1;

        // the following flags and values are stored in count
        static constexpr size_t has_hash_child_flag = (static_cast<size_t>(1) << (sizeof(size_t) * 8 - 1));
        static constexpr size_t has_plus_child_flag = (static_cast<size_t>(1) << (sizeof(size_t) * 8 - 2));
        static constexpr size_t max_count = (has_plus_child_flag - 1);

        Value value;

        path_entry(node_id_t id)
            : id(id)
        {}
    };

    using map_type = std::unordered_map< path_entry_key, path_entry, boost::hash< path_entry_key > >;
    using map_type_iterator = typename map_type::iterator;
    using map_type_const_iterator = typename map_type::const_iterator;

    map_type map;
    map_type_iterator root;
    node_id_t next_node_id;

protected:
    map_type_iterator get_key(path_entry_key key) { return map.find(key); }
    map_type_iterator begin() { return map.begin(); }
    map_type_iterator end() { return map.end(); }
    map_type const& get_map() const { return map; }

    handle path_to_handle(std::vector< map_type_iterator > const& path) const {
        std::vector < path_entry_key > result;
        result.reserve(path.size());

        for (auto const& i : path) {
            result.push_back(i->first);
        }

        return result;
    }

    std::vector< map_type_iterator> find_subscription(MQTT_NS::string_view const& subscription) {
        auto parent = root;
        std::vector< map_type_iterator > path;

        topic_filter_tokenizer(
            subscription,
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

    std::vector<map_type_iterator> create_subscription(MQTT_NS::string_view subscription) {
        auto parent = root;

        // Check on root entry if we can still add an entry
        if ((parent->second.count & path_entry::max_count) == path_entry::max_count) {
            throw std::overflow_error("Maximum number of subscriptions reached");
        }

        std::vector<map_type_iterator> result;

        topic_filter_tokenizer(
            subscription,
            [this, &parent, &result](MQTT_NS::string_view t) {
                auto parent_id = parent->second.id;
                auto entry = map.find(path_entry_key(parent_id, t));

                if (entry == map.end())  {
                    entry =
                        map.emplace(
                            path_entry_key(
                                parent_id,
                                MQTT_NS::allocate_buffer(t)
                            ),
                            path_entry(next_node_id++)
                        ).first;
                    if (t == "+") {
                        parent->second.count |= path_entry::has_plus_child_flag;
                    }

                    if (t == "#") {
                        parent->second.count |= path_entry::has_hash_child_flag;
                    }

                    if (next_node_id == std::numeric_limits<decltype(next_node_id)>::max()) {
                        throw std::overflow_error("Maximum number of subscriptions reached");
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

    // Remove a value at the specified subscription path
    void remove_subscription(std::vector< map_type_iterator > const& path) {
        if (path.empty()) {
            return;
        }

        std::vector<path_entry_key> remove_keys;
        remove_keys.reserve(path.size());

        // First parent is always the root
        auto parent = root;

        // Go through entries to remove
        for (auto entry : path) {
            --entry->second.count;
            if ((entry->second.count & path_entry::max_count) == 0) {
                if (entry->first.second == "+") {
                    parent->second.count &= ~path_entry::has_plus_child_flag;
                }

                if (entry->first.second == "#") {
                    parent->second.count &= ~path_entry::has_hash_child_flag;
                }

                remove_keys.push_back(entry->first);
            }
        }

        for(auto key : remove_keys) {
            map.erase(key);
        }
   }

    // Find all subscriptions that match the specified topic
    template<typename Output>
    void find_match(MQTT_NS::string_view topic, Output callback) const {
        std::deque<map_type_const_iterator> entries;
        entries.push_back(root);

        std::deque<map_type_const_iterator> new_entries;

        topic_filter_tokenizer(
            topic,
            [this, &entries, &new_entries, &callback](MQTT_NS::string_view t) {
                new_entries.resize(0);

                for (auto const& entry : entries) {
                    auto parent = entry->second.id;
                    auto i = map.find(path_entry_key(parent, t));
                    if (i != map.end()) {
                        new_entries.push_back(i);
                    }

                    if (entry->second.count & path_entry::has_plus_child_flag) {
                        i = map.find(path_entry_key(parent, MQTT_NS::string_view("+")));
                        if (i != map.end()) {
                            if (parent != root_node_id || (t.empty() ? true : t[0] != '$')) {
                                new_entries.push_back(i);
                            }
                        }
                    }

                    if (entry->second.count & path_entry::has_hash_child_flag) {
                        i = map.find(path_entry_key(parent, MQTT_NS::string_view("#")));
                        if (i != map.end()) {
                            if (parent != root_node_id || (t.empty() ? true : t[0] != '$')){
                                callback(i->second.value);
                            }
                        }
                    }
                }

                std::swap(entries, new_entries);
                return !entries.empty();
            }
        );

        for (auto const& entry : entries) {
            callback(entry->second.value);
        }
    }

    // Get the iterators of a handle
    std::vector<map_type_iterator> handle_to_iterators(handle h) {
        std::vector<map_type_iterator> result;
        for (auto i : h) {
            auto entry_iter = map.find(i);
            if (entry_iter == map.end()) {
                throw std::runtime_error("Invalid handle was specified");
            }

            result.push_back(entry_iter);
        }

        return result;
    }

    subscription_map_base()
        : next_node_id(root_node_id)
    {
        // Create the root node
        root = map.emplace(path_entry_key(std::numeric_limits<node_id_t>::max(), MQTT_NS::allocate_buffer("")), path_entry(root_node_id)).first;
        ++next_node_id;
    }

public:
    // Return the number of elements in the tree
    size_t size() const { return map.size(); }

    // Lookup a subscription
    handle lookup(MQTT_NS::string_view subscription) {
        return this->path_to_handle(this->find_subscription(subscription));
    }

    // Get path of subscription
    std::string handle_to_subscription(handle h)  const {
        std::ostringstream result;
        bool first_entry = true;

        for (auto const& i : h) {
            if (!first_entry) {
                result << "/";
            }

            result << i.second;
            first_entry = false;
        }

        return result.str();
    }
};

template<typename Value>
class single_subscription_map
    : public subscription_map_base<Value> {

public:

    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a value at the specified subscription path
    handle insert(MQTT_NS::string_view subscription, Value value) {
        auto existing_subscription = this->find_subscription(subscription);
        if (!existing_subscription.empty())
            throw std::runtime_error("Subscription already exists in map");

        auto new_subscription_path = this->create_subscription(subscription);
        new_subscription_path.back()->second.value = std::move(value);
        return this->path_to_handle(new_subscription_path);
    }

    // Update a value at the specified subscription path
    void update(MQTT_NS::string_view subscription, Value value) {
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            throw std::runtime_error("Invalid subscription was specified");
        }

        path.back()->second.value = std::move(value);
    }

    void update(handle h, Value value) {
        auto entry_iter = this->get_key(h.back());
        if (entry_iter == this->end()) {
            throw std::runtime_error("Invalid subscription was specified");
        }
        entry_iter->second.value = std::move(value);
    }

    // Remove a value at the specified subscription path
    void erase(MQTT_NS::string_view subscription) {
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            return;
        }

        this->remove_subscription(path);
    }

    // Remove a value using a handle
    void erase(handle h) {
        auto path = this->handle_to_iterators(h);
        if (!path.empty()) {
            this->remove_subscription(path);
        }
    }

    // Find all subscriptions that match the specified topic
    template<typename Output>
    void find(MQTT_NS::string_view topic, Output callback) const {
        this->find_match(topic, std::move(callback));
    }
};


template<typename Value, class Cont = std::set<Value, std::less<Value>, std::allocator<Value> > >
class multiple_subscription_map
    : public subscription_map_base< Cont >
{

public:
    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a value at the specified subscription path
    std::pair<handle, bool> insert(MQTT_NS::string_view subscription, Value value) {
        auto new_subscription_path = this->create_subscription(subscription);
        new_subscription_path.back()->second.value.insert(std::move(value));
        return std::make_pair(this->path_to_handle(new_subscription_path), true);
    }

    // Insert a value with a handle to the subscription
    std::pair<handle, bool> insert(handle h, Value value) {
        if (h.empty()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        // Remove the specified value
        auto h_iter = this->get_key(h.back());
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        auto& subscription_set = h_iter->second.value;
        bool insert_result = subscription_set.insert(std::move(value)).second;
        return std::make_pair(h, insert_result);
    }

    // Remove a value at the specified subscription path
    // returns the value of the removed element (if found)
    size_t erase(MQTT_NS::string_view subscription, Value const& value) {
        // Find the subscription in the map
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            return boost::optional<Value>();
        }

        // Remove the specified value
        auto& subscription_set = path.back()->second.value;
        auto result = subscription_set.erase(value);

        // If all values removed, remove the subscription
        if(subscription_set.empty())
            this->remove_subscription(path);

        return result;
    }

    // Remove a value at the specified handle
    // returns the value of the removed element (if found)
    size_t erase(handle h, Value const& value) {
        if (h.empty()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        // Remove the specified value
        auto h_iter = this->get_key(h.back());
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        // Remove the specified value
        auto& subscription_set = h_iter->second.value;
        auto result = subscription_set.erase(value);

        // If all values removed, remove the subscription
        if(subscription_set.empty())
            this->remove_subscription(this->handle_to_iterators(h));

        return result;
    }

        // Find all subscriptions that match the specified topic
    template<typename Output>
    void find(MQTT_NS::string_view topic, Output callback) const {
        this->find_match(
            topic,
            [&callback]( Cont const &values ) {
                for (Value const& i : values) {
                    callback(i);
                }
            }
        );
    }

    void dump(std::ostream &out) {
        for (auto const& i: this->get_map()) {
            out << i.first.first << " " << i.first.second << " " << i.second.value.size() << " " << i.second.count << std::endl;
        }
    }
private:
};

#endif // MQTT_SUBSCRIPTION_MAP_HPP

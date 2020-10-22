// Copyright Wouter van Kleunen 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SUBSCRIPTION_MAP_HPP)
#define MQTT_SUBSCRIPTION_MAP_HPP

#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include <boost/variant.hpp>
#include <boost/functional/hash.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/optional.hpp>

#include <boost/range/adaptor/reversed.hpp>
#include "topic_filter_tokenizer.hpp"

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

    std::vector< map_type_iterator> find_subscription(MQTT_NS::string_view subscription) {
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
                            if (parent != self.root_node_id || (t.empty() ? true : t[0] != '$')) {
                                new_entries.push_back(i);
                            }
                        }
                    }

                    if (entry->second.count & path_entry::has_hash_child_flag) {
                        i = self.map.find(path_entry_key(parent, MQTT_NS::string_view("#")));
                        if (i != self.map.end()) {
                            if (parent != self.root_node_id || (t.empty() ? true : t[0] != '$')){
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

    // Find all subscriptions that match the specified topic
    template<typename Output>
    void find_match(MQTT_NS::string_view topic, Output&& callback) const {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    template<typename Output>
    void find_match(MQTT_NS::string_view topic, Output&& callback) {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    template<typename Output>
    void modify_match(MQTT_NS::string_view topic, Output callback) {
        find_match(topic, [&callback](Value& i) {
            callback(i);
        });
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

    // Lookup a subscription
    handle lookup(MQTT_NS::string_view subscription) {
        return this->path_to_handle(this->find_subscription(subscription));
    }

    // Get path of subscription
    std::string handle_to_subscription(handle h) {
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

    // Insert a value at the specified subscription path
    template <typename V>
    handle insert(MQTT_NS::string_view subscription, V&& value) {
        auto existing_subscription = this->find_subscription(subscription);
        if (!existing_subscription.empty()) {
            if(existing_subscription.back()->second.value)
                throw std::runtime_error("Subscription already exists in map");
            existing_subscription.back()->second.value.emplace(std::forward<V>(value));
            return this->path_to_handle(existing_subscription);
        }

        auto new_subscription_path = this->create_subscription(subscription);
        new_subscription_path.back()->second.value = value;
        ++this->map_size;
        return this->path_to_handle(new_subscription_path);
    }

    // Update a value at the specified subscription path
    template <typename V>
    void update(MQTT_NS::string_view subscription, V&& value) {
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            throw std::runtime_error("Invalid subscription was specified");
        }

        path.back()->second.value.emplace(std::forward<V>(value));
    }

    template <typename V>
    void update(handle h, V&& value) {
        auto entry_iter = this->get_key(h);
        if (entry_iter == this->end()) {
            throw std::runtime_error("Invalid subscription was specified");
        }
        entry_iter->second.value.emplace(std::forward<V>(value));
    }

    // Remove a value at the specified subscription path
    std::size_t erase(MQTT_NS::string_view subscription) {
        auto path = this->find_subscription(subscription);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_subscription(path);
        --this->map_size;
        return 1;
    }

    // Remove a value using a handle
    std::size_t erase(handle h) {
        auto path = this->handle_to_iterators(h);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_subscription(path);
        --this->map_size;
        return 1;
    }

    // Find all subscriptions that match the specified topic
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

// Following templated functions allow for perfect forwarding through lambda capture
// https://isocpp.org/blog/2016/12/capturing-perfectly-forwarded-objects-in-lambdas-vittorio-romeo
template <typename... Ts> auto fwd_capture(Ts&&... xs)
{
    return std::tuple<Ts...>(std::forward<decltype(xs)>(xs)...);
}

template <typename T> decltype(auto) access(T&& x) { return std::get<0>(std::forward<decltype(x)>(x)); }

template<typename Key, typename Value, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class multiple_subscription_map_store_flat = boost::container::flat_map<Key, Value>,
         class multiple_subscription_map_store_hash = std::unordered_map<Key, Value, Hash, Pred>,
         class multiple_subscription_map_store = boost::variant< multiple_subscription_map_store_flat, multiple_subscription_map_store_hash > >
class multiple_subscription_map
    : public subscription_map_base< multiple_subscription_map_store >
{

    // Insert or update an entry in hash or flat map
    static inline bool insert_or_update_map(multiple_subscription_map_store &values, std::pair<Key, Value>&& new_value) {
       return boost::apply_visitor( [new_value = fwd_capture(std::forward<decltype(new_value)>(new_value))](auto &values) {
            auto result = values.insert( access(new_value) );
            if(!result.second) {
                result.first->second = access(new_value).second;
            }
           return result.second;
        }, values );
    }

    // Erase entry from flat or hash map
    static inline bool erase_map(multiple_subscription_map_store &values, Key const &key) {
       return boost::apply_visitor( [&key](auto &values) {
            return values.erase(key);
        }, values );
    }

    // Retrieve current size of the map
    static inline std::size_t size_of_map(multiple_subscription_map_store &values) {
       return boost::apply_visitor( [](auto &values) {
            return values.size();
        }, values );
    }

    // Threshold to convert between flatmap and hash
    static constexpr std::size_t convert_to_hash_threshold = 1500;

    // Convert a hash map to a flat map
    static multiple_subscription_map_store_flat convert_to_flatmap(multiple_subscription_map_store_hash&& input)
    {
        multiple_subscription_map_store_flat result;
        for(auto&& i : input) {
            result.insert_or_assign(std::move(i.first), std::move(i.second));
        }
        return result;
    }

    // Convert a flat map to a hash map
    static multiple_subscription_map_store_hash convert_to_hash(multiple_subscription_map_store_flat&& input)
    {
        multiple_subscription_map_store_hash result;
        for(auto&& i : input) {
            result.insert(i);
        }
        return result;
    }

public:
    using container_t = multiple_subscription_map_store;

    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a key => value at the specified subscription path
    // returns the handle and true if key was inserted, false if key was updated
    template <typename V>
    std::pair<handle, bool> insert_or_update(MQTT_NS::string_view subscription, Key const &key, V&& value) {
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            auto new_subscription_path = this->create_subscription(subscription);
            insert_or_update_map(new_subscription_path.back()->second.value, { key, std::forward<V>(value) });
            ++this->map_size;
            return std::make_pair(this->path_to_handle(new_subscription_path), true);
        }
        else {
            auto& subscription_set = path.back()->second.value;

            auto new_pair = std::make_pair(key, std::forward<V>(value));
            auto insert_result = insert_or_update_map(subscription_set, std::move(new_pair));
            if(insert_result) {
                this->increase_subscriptions(path);
                ++this->map_size;
            }

            auto result = std::make_pair(this->path_to_handle(path), insert_result);

            // Convert map to a hash map, if large number of subscriptions is reached
           if(size_of_map(subscription_set) >= convert_to_hash_threshold && boost::get<multiple_subscription_map_store_flat>(&subscription_set) != nullptr)
                subscription_set = convert_to_hash(boost::get<multiple_subscription_map_store_flat>(std::move(subscription_set)));

            return result;
        }
    }

    // Insert a key => value with a handle to the subscription
    // returns the handle and true if key was inserted, false if key was updated
    template <typename V>
    std::pair<handle, bool> insert_or_update(handle h, Key const &key, V&& value) {
        // Remove the specified value
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        auto& subscription_set = h_iter->second.value;

        auto new_pair = std::make_pair(key, std::forward<V>(value));
        auto insert_result = insert_or_update_map(subscription_set, std::move(new_pair));
        if (insert_result) {
            ++this->map_size;
            this->increase_subscriptions(h);
        }

        // Convert map to a hash map, if large number of subscriptions is reached
        if(size_of_map(subscription_set) >= convert_to_hash_threshold && boost::get<multiple_subscription_map_store_flat>(&subscription_set) != nullptr)
            subscription_set = convert_to_hash(boost::get<multiple_subscription_map_store_flat>(std::move(subscription_set)));

        return std::make_pair(h, insert_result);
    }

    // Remove a value at the specified subscription path
    // returns the number of removed elements
    std::size_t erase(MQTT_NS::string_view subscription, Key const& key) {
        // Find the subscription in the map
        auto path = this->find_subscription(subscription);
        if (path.empty()) {
            return 0;
        }

        auto& subscription_set = path.back()->second.value;
        auto old_size = size_of_map(subscription_set);

        // Remove the specified value
        auto result = erase_map(subscription_set, key);
        if (result) {
            --this->map_size;
            this->remove_subscription(path);

            // Convert map back to a flat map, if number of subscriptions is small enough
            if(old_size == convert_to_hash_threshold && size_of_map(subscription_set) < convert_to_hash_threshold && boost::get<multiple_subscription_map_store_hash>(&subscription_set) != nullptr)
                subscription_set = convert_to_flatmap(boost::get<multiple_subscription_map_store_hash>(std::move(subscription_set)));

        }

        return result;
    }

    // Remove a value at the specified handle
    // returns the number of removed elements
    std::size_t erase(handle h, Key const& key) {
        // Remove the specified value
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            throw std::runtime_error("Invalid handle was specified");
        }

        auto& subscription_set = h_iter->second.value;
        auto old_size = size_of_map(subscription_set);

        // Remove the specified value
        auto result = erase_map(subscription_set, key);
        if (result) {
            --this->map_size;
            this->remove_subscription(this->handle_to_iterators(h));

            // Convert map back to a flat map, if number of subscriptions is small enough
            if(old_size == convert_to_hash_threshold && size_of_map(subscription_set) < convert_to_hash_threshold && boost::get<multiple_subscription_map_store_hash>(&subscription_set) != nullptr)
                subscription_set = convert_to_flatmap(boost::get<multiple_subscription_map_store_hash>(std::move(subscription_set)));
        }

        return result;
    }

    // Find all subscriptions that match the specified topic
    template<typename Output>
    void find(MQTT_NS::string_view topic, Output callback) const {
        this->find_match(
            topic,
            [&callback]( multiple_subscription_map_store const &values ) {
                boost::apply_visitor( [&callback](auto const &values) {
                   for (auto const& i : values) {
                       callback(i.first, i.second);
                   }
                }, values );
            }
        );
    }

    // Find all subscriptions that match and allow modification
    template<typename Output>
    void modify(MQTT_NS::string_view topic, Output callback) {
        auto modify_callback = [&callback]( multiple_subscription_map_store &values ) {
            boost::apply_visitor( [&callback](auto& values) {
               for (auto& i : values) {
                   callback(i.first, i.second);
               }
            }, values );
        };

        this->template modify_match<decltype(modify_callback)>(topic, modify_callback);
    }


    template<typename Output>
    void dump(Output &out) {
        for (auto const& i : this->get_map()) {
            out << i.first.first << " " << i.first.second << " " << i.second.value.size() << " " << i.second.count << std::endl;
        }
    }

};

#endif // MQTT_SUBSCRIPTION_MAP_HPP

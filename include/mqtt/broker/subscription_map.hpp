// Copyright Wouter van Kleunen 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SUBSCRIPTION_MAP_HPP)
#define MQTT_BROKER_SUBSCRIPTION_MAP_HPP

#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <mqtt/broker/broker_namespace.hpp>

#include <mqtt/string_view.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/buffer.hpp>

#include <mqtt/broker/topic_filter.hpp>

MQTT_BROKER_NS_BEGIN

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
 *
 *   Subscription map stores all entries in a tree
 *   the tree starts from a root node, and topic filters are tokenized and stored in the tree
 *
 *   For example if the topic_filter example/monitor/Clients is stored, the following nodes are created:
 *   root -> example -> monitor -> Clients
 *
 *   Every node in the tree may store one or multiple subscribers. Nodes store a reference count to the number of subscribers
 *   so for example, if we store the following topic_filters:
 *     example/
 *     example/monitor/Clients
 *
 *   the subscription map looks as follows:
 *     root(2) -> example(2) -> monitor(1) -> Clients (1)
 *
 *   hash and + are stored as normal nodes within the tree, but the parent node knows if a hash child is available. This
 *   improves the matching, no extra lookup is required to see if a # or + child is available in a child node:
 *
 *     example/#
 *
 *    stores the following tree:
 *      root -> example (hash: yes) -> #
 *
 *    and
 *
 *      example/+
 *
 *    stores the following tree:
 *      root -> example (plus: yes) -> #
 *
 *    all node entries are stored in a single hash map. The key for every node is: (parent node id, path)
 *
 *      so if we store: root/example/test
 *      root (id:1) -> example (id:2, key:1,example) -> test (id:3, key:2,test)
 *
 *    also, every node stores the key of its parent, allowing quick traversing from leaf to root of the tree
 */

// Combined storage for count and flags
// we can have 32bit or 64bit version

// Compile error on other platforms (not 32 or 64 bit)
template<std::size_t N>
struct count_storage {
   static_assert(N == 4 || N == 8, "Subscription map count_storage only knows how to handle architectures with 32 or 64 bit size_t: please update to support your platform.");
};

template<>
struct count_storage<4> {
public:
    count_storage(std::uint32_t v = 1)
        : value_(v & 0x3fffffffUL), has_hash_child_(false), has_plus_child_(false)
    { }

    static constexpr std::size_t max() { return std::numeric_limits<uint32_t>::max() >> 2; }

    std::uint32_t value() const { return value_; }
    void set_value(std::uint32_t v) {
        value_ = v & 0x3fffffffUL;
    }
    void increment_value() {
        ++value_;
    }
    void decrement_value() {
        --value_;
    }

    bool has_hash_child() const { return has_hash_child_; }
    void set_hash_child(bool v) {
        has_hash_child_ = v;
    }

    bool has_plus_child() const { return has_plus_child_; }
    void set_plus_child(bool v) {
        has_plus_child_ = v;
    }

private:
    std::uint32_t value_ : 30;
    std::uint32_t has_hash_child_ : 1;
    std::uint32_t has_plus_child_ : 1;

};

template<>
struct count_storage<8> {
public:
    count_storage(std::uint64_t v= 1)
        : value_(v & 0x3fffffffffffffffULL), has_hash_child_(false), has_plus_child_(false)
    { }

    static constexpr std::uint64_t max() { return std::numeric_limits<uint64_t>::max() >> 2; }

    std::uint64_t value() const { return value_; }
    void set_value(std::uint64_t v) {
        value_ = v & 0x3fffffffffffffffULL;
    }
    void increment_value() {
        ++value_;
    }
    void decrement_value() {
        --value_;
    }

    bool has_hash_child() const { return has_hash_child_; }
    void set_hash_child(bool v) {
        has_hash_child_ = v;
    }

    bool has_plus_child() const { return has_plus_child_; }
    void set_plus_child(bool v) {
        has_plus_child_ = v;
    }


private:
    std::uint64_t value_ : 62;
    std::uint64_t has_hash_child_ : 1;
    std::uint64_t has_plus_child_ : 1;
};

template<typename Value>
class subscription_map_base {
public:
    using node_id_t = std::size_t;
    using path_entry_key = std::pair<node_id_t, buffer>;
    using handle = path_entry_key;

private:

    // Generate a node id for a new node
    node_id_t generate_node_id() {
        if(next_node_id == std::numeric_limits<node_id_t>::max())
            throw_max_stored_topics();
         return ++next_node_id;
    }

    using count_storage_t = count_storage<sizeof(void *)>;

    struct path_entry {
        node_id_t id;
        path_entry_key parent;

        count_storage_t count;

        Value value;

        path_entry(node_id_t id, path_entry_key parent)
            : id(id), parent(parent)
        {}
    };

    // Increase the subscription count for a specific node
    static void increase_count_storage(count_storage_t &count) {
        if(count.value() == count_storage_t::max()) {
            throw_max_stored_topics();
        }

        count.increment_value();
    }

    // Decrease the subscription count for a specific node
    static void decrease_count_storage(count_storage_t& count) {
        BOOST_ASSERT(count.value() > 0);
        count.decrement_value();
    }

    using this_type = subscription_map_base<Value>;

    // Use boost hash to hash pair in path_entry_key
    using map_type = std::unordered_map< path_entry_key, path_entry, boost::hash< path_entry_key > >;

    map_type map;
    using map_type_iterator = typename map_type::iterator;
    using map_type_const_iterator = typename map_type::const_iterator;

    node_id_t next_node_id = 0;

protected:
    // Key and id of the root key
    path_entry_key root_key;
    node_id_t root_node_id;

    // Return the iterator of the root
    map_type_iterator get_root() { return map.find(root_key); };
    map_type_const_iterator get_root() const { return map.find(root_key); };


    // Map size tracks the total number of subscriptions within the map
    size_t map_size = 0;

    map_type_iterator get_key(path_entry_key key) { return map.find(key); }
    map_type_iterator begin() { return map.begin(); }
    map_type_iterator end() { return map.end(); }
    map_type const& get_map() const { return map; }

    handle path_to_handle(std::vector< map_type_iterator > const& path) const {
        return path.back()->first;
    }

    std::vector< map_type_iterator> find_topic_filter(string_view topic_filter) {
        auto parent_id = get_root()->second.id;
        std::vector< map_type_iterator > path;

        topic_filter_tokenizer(
            topic_filter,
            [this, &path, &parent_id](string_view t) mutable {
                auto entry = map.find(path_entry_key(parent_id, t));

                if (entry == map.end()) {
                    path.clear();
                    return false;
                }

                path.push_back(entry);
                parent_id = entry->second.id;
                return true;
            }
        );

        return path;
    }

    std::vector<map_type_iterator> create_topic_filter(string_view topic_filter) {
        auto parent = get_root();

        std::vector<map_type_iterator> result;

        topic_filter_tokenizer(
            topic_filter,
            [this, &parent, &result](string_view t) mutable {
                auto entry = map.find(path_entry_key(parent->second.id, t));

                if (entry == map.end()) {
                    entry =
                        map.emplace(
                            path_entry_key(
                                parent->second.id,
                                allocate_buffer(t)
                            ),
                            path_entry(generate_node_id(), parent->first)
                        ).first;

                    parent->second.count.set_plus_child(parent->second.count.has_plus_child() || (t == "+"));
                    parent->second.count.set_hash_child(parent->second.count.has_hash_child() || (t == "#"));
                }
                else {
                    increase_count_storage(entry->second.count);
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
        for (auto& entry : boost::adaptors::reverse(path)) {
            if (remove_plus_child_flag) {
                entry->second.count.set_plus_child(false);
                remove_plus_child_flag = false;
            }

            if (remove_hash_child_flag) {
                entry->second.count.set_hash_child(false);
                remove_hash_child_flag = false;
            }

            decrease_count_storage(entry->second.count);
            if (entry->second.count.value() == 0) {
                remove_plus_child_flag = (entry->first.second == "+");
                remove_hash_child_flag = (entry->first.second == "#");

                // Erase in unordered map only invalidates erased iterator
                // other iterators are unaffected
                map.erase(entry->first);
            }
        }

        auto root = get_root();
        if (remove_plus_child_flag) {
            root->second.count.set_plus_child(false);
        }

        if (remove_hash_child_flag) {
            root->second.count.set_hash_child(false);
        }
    }

    template <typename ThisType, typename Output>
    static void find_match_impl(ThisType& self, string_view topic, Output&& callback) {
        using iterator_type = decltype(self.map.end()); // const_iterator or iterator depends on self

        std::vector<iterator_type> entries;
        entries.push_back(self.get_root());

        topic_filter_tokenizer(
            topic,
            [&self, &entries, &callback](string_view t) {
                std::vector<iterator_type> new_entries;

                for (auto& entry : entries) {
                    auto parent = entry->second.id;
                    auto i = self.map.find(path_entry_key(parent, t));
                    if (i != self.map.end()) {
                        new_entries.push_back(i);
                    }

                    if (entry->second.count .has_plus_child()) {
                        i = self.map.find(path_entry_key(parent, string_view("+")));
                        if (i != self.map.end()) {
                            if (parent != self.root_node_id || t.empty() || t[0] != '$') {
                                new_entries.push_back(i);
                            }
                        }
                    }

                    if (entry->second.count.has_hash_child()) {
                        i = self.map.find(path_entry_key(parent, string_view("#")));
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
    void find_match(string_view topic, Output&& callback) const {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    // Find all topic filters and allow modification
    template<typename Output>
    void modify_match(string_view topic, Output&& callback) {
        find_match_impl(*this, topic, std::forward<Output>(callback));
    }

    template<typename ThisType, typename Output>
    static void handle_to_iterators(ThisType& self, handle const &h, Output&& output) {
        auto i = h;
        while(i != self.root_key) {
            auto entry_iter = self.map.find(i);
            if (entry_iter == self.map.end()) {
                throw_invalid_handle();
            }

            output(entry_iter);
            i = entry_iter->second.parent;
        }
    }

    // Exceptions used
    static void throw_invalid_topic_filter() { throw std::runtime_error("Subscription map invalid topic filter was specified"); }
    static void throw_invalid_handle() { throw std::runtime_error("Subscription map invalid handle was specified"); }
    static void throw_max_stored_topics() { throw std::overflow_error("Subscription map maximum number of stored topic filters reached"); }

    // Get the iterators of a handle
    std::vector<map_type_iterator> handle_to_iterators(handle const &h) {
        std::vector<map_type_iterator> result;
        handle_to_iterators(*this, h, [&result](map_type_iterator i) { result.push_back(i); });
        std::reverse(result.begin(), result.end());
        return result;
    }

    // Increase the number of subscriptions for this handle
    void increase_subscriptions(handle const &h) {
        handle_to_iterators(*this, h, [](map_type_iterator i) {
            increase_count_storage(i->second.count);
        });
    }

    // Increase the map size (total number of subscriptions stored)
    void increase_map_size() {
        if(map_size == std::numeric_limits<decltype(map_size)>::max()) {
            throw_max_stored_topics();
        }

        ++map_size;
    }

    // Decrease the map size (total number of subscriptions stored)
    void decrease_map_size() {
        BOOST_ASSERT(map_size > 0);
        --map_size;
    }

    // Increase the number of subscriptions for this path
    void increase_subscriptions(std::vector<map_type_iterator> const &path) {
        for (auto i : path) {
            increase_count_storage(i->second.count);
        }
    }

    subscription_map_base()
    {
        // Create the root node
        root_node_id = generate_node_id();
        root_key = path_entry_key(generate_node_id(), buffer());
        map.emplace(root_key, path_entry(root_node_id, path_entry_key()));
    }

public:
    // Return the number of elements in the tree
    std::size_t internal_size() const { return map.size(); }

    // Return the number of registered topic filters
    std::size_t size() const { return this->map_size; }

    // Lookup a topic filter
    optional<handle> lookup(string_view topic_filter) {
        auto path = this->find_topic_filter(topic_filter);
        if(path.empty())
            return optional<handle>();
        else
            return this->path_to_handle(force_move(path));
    }

    // Get path of topic_filter
    std::string handle_to_topic_filter(handle const &h) const {
        std::string result;

        handle_to_iterators(*this, h, [&result](map_type_const_iterator i) {
            if (result.empty()) {
                result = std::string(i->first.second);
            }
            else {
                result = std::string(i->first.second) + "/" + result;
            }
        });

        return result;
    }
};

template<typename Value>
class single_subscription_map
    : public subscription_map_base< optional<Value> > {

public:

    // Handle of an entry
    using handle = typename subscription_map_base< Value >::handle;

    // Insert a value at the specified topic_filter
    template <typename V>
    std::pair<handle, bool> insert(string_view topic_filter, V&& value) {
        auto existing_subscription = this->find_topic_filter(topic_filter);
        if (!existing_subscription.empty()) {
            if(existing_subscription.back()->second.value)
                return std::make_pair(this->path_to_handle(force_move(existing_subscription)), false);

            existing_subscription.back()->second.value.emplace(std::forward<V>(value));
            return std::make_pair(this->path_to_handle(force_move(existing_subscription)), true);
        }

        auto new_topic_filter = this->create_topic_filter(topic_filter);
        new_topic_filter.back()->second.value = value;
        this->increase_map_size();
        return std::make_pair(this->path_to_handle(force_move(new_topic_filter)), true);
    }

    // Update a value at the specified topic filter
    template <typename V>
    void update(string_view topic_filter, V&& value) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            this->throw_invalid_topic_filter();
        }

        path.back()->second.value.emplace(std::forward<V>(value));
    }

    template <typename V>
    void update(handle const &h, V&& value) {
        auto entry_iter = this->get_key(h);
        if (entry_iter == this->end()) {
            this->throw_invalid_topic_filter();
        }
        entry_iter->second.value.emplace(std::forward<V>(value));
    }

    // Remove a value at the specified topic filter
    std::size_t erase(string_view topic_filter) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_topic_filter(path);
        this->decrease_map_size();
        return 1;
    }

    // Remove a value using a handle
    std::size_t erase(handle const &h) {
        auto path = this->handle_to_iterators(h);
        if (path.empty() || !path.back()->second.value) {
            return 0;
        }

        this->remove_topic_filter(path);
        this->decrease_map_size();
        return 1;
    }

    // Find all topic filters that match the specified topic
    template<typename Output>
    void find(string_view topic, Output&& callback) const {
        this->find_match(
            topic,
            [&callback]( optional<Value> const& value ) {
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
    std::pair<handle, bool> insert_or_assign(string_view topic_filter, K&& key, V&& value) {
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            auto new_topic_filter = this->create_topic_filter(topic_filter);
            new_topic_filter.back()->second.value.emplace(std::forward<K>(key), std::forward<V>(value));
            this->increase_map_size();
            return std::make_pair(this->path_to_handle(force_move(new_topic_filter)), true);
        }
        else {
            auto& subscription_set = path.back()->second.value;

#if __cpp_lib_unordered_map_try_emplace >= 201411L
            auto insert_result = subscription_set.insert_or_assign(std::forward<K>(key), std::forward<V>(value));
            if(insert_result.second) {
                this->increase_subscriptions(path);
                this->increase_map_size();
            }
            return std::make_pair(this->path_to_handle(force_move(path)), insert_result.second);
#else
            auto iter = subscription_set.find(key);
            if(iter == subscription_set.end()) {
                subscription_set.emplace(std::forward<K>(key), std::forward<V>(value));
                this->increase_subscriptions(path);
                this->increase_map_size();
            } else {
                iter->second = std::forward<V>(value);
            }
            return std::make_pair(this->path_to_handle(force_move(path)), iter == subscription_set.end());

#endif
        }
    }

    // Insert a key => value with a handle to the topic filter
    // returns the handle and true if key was inserted, false if key was updated
    template <typename K, typename V>
    std::pair<handle, bool> insert_or_assign(handle const &h, K&& key, V&& value) {
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            this->throw_invalid_handle();
        }

        auto& subscription_set = h_iter->second.value;

#if __cpp_lib_unordered_map_try_emplace >= 201411L
        auto insert_result = subscription_set.insert_or_assign(std::forward<K>(key), std::forward<V>(value));
        if(insert_result.second) {
            this->increase_subscriptions(h);
            this->increase_map_size();
        }
        return std::make_pair(h, insert_result.second);
#else
        auto iter = subscription_set.find(key);
        if(iter == subscription_set.end()) {
            subscription_set.emplace(std::forward<K>(key), std::forward<V>(value));
            this->increase_subscriptions(h);
            this->increase_map_size();
        } else {
            iter->second = std::forward<V>(value);
        }
        return std::make_pair(h, iter == subscription_set.end());
#endif
    }

    // Remove a value at the specified handle
    // returns the number of removed elements
    std::size_t erase(handle const &h, Key const& key) {
        // Find the handle in the map
        auto h_iter = this->get_key(h);
        if (h_iter == this->end()) {
            this->throw_invalid_handle();
        }

        // Remove the specified value
        auto result = h_iter->second.value.erase(key);
        if (result) {
            this->remove_topic_filter(this->handle_to_iterators(h));
            this->decrease_map_size();
        }

        return result;
    }

    // Remove a value at the specified topic filter
    // returns the number of removed elements
    std::size_t erase(string_view topic_filter, Key const& key) {
        // Find the topic filter in the map
        auto path = this->find_topic_filter(topic_filter);
        if (path.empty()) {
            return 0;
        }

        // Remove the specified value
        auto result = path.back()->second.value.erase(key);
        if (result) {
            this->decrease_map_size();
            this->remove_topic_filter(path);
        }

        return result;
    }

    // Find all topic filters that match the specified topic
    template<typename Output>
    void find(string_view topic, Output&& callback) const {
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
    void modify(string_view topic, Output&& callback) {
        this->modify_match(
            topic,
            [&callback]( Cont &values ) {
                for (auto& i : values) {
                    callback(i.first, i.second);
                }
            }
        );
    }

    template<typename Output>
    void dump(Output &out) {
        out << "Root node id: " << this->root_node_id << std::endl;
        for (auto const& i: this->get_map()) {
            out << "(" << i.first.first << ", " << i.first.second << "): id: " << i.second.id << ", size: " << i.second.value.size() << ", value: " << i.second.count.value << std::endl;
        }
    }

};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SUBSCRIPTION_MAP_HPP

// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VALUE_ALLOCATOR_HPP)
#define MQTT_VALUE_ALLOCATOR_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

#include <ostream>
#include <limits>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt/optional.hpp>

namespace MQTT_NS {

namespace mi = boost::multi_index;

template <typename T>
class value_allocator {
    using value_type = T;

    class value_interval {
    public:
        explicit value_interval(value_type v) : low_{v}, high_{v} {}
        value_interval(value_type l, value_type h) : low_{l}, high_{h} {}

        // true
        // | lhs |   | rhs |
        //
        // false
        // | lhs |
        //      | rhs |
        //
        // false
        // |   lhs   |
        //   | rhs |
        //
        // false
        //   | lhs |
        // |   rhs   |
        //
        // false
        //   | lhs |
        // | rhs |
        //
        // false
        // | rhs |   | lhs |
        //
        friend bool operator<(value_interval const& lhs, value_interval const& rhs) {
            return
                (lhs.low_ < rhs.low_) &&
                (lhs.high_ < rhs.low_);
        }
        value_type low() const {
            return low_;
        }
        value_type high() const {
            return high_;
        }

        friend std::ostream& operator<<(std::ostream& o, value_interval const v) {
            o << '[' << v.low() << ',' << v.high() << ']';
            return o;
        }

    private:
        value_type low_;
        value_type high_;
    };

public:
    /**
     * @brief Create value_allocator
     *        The allocator has [lowest, highest] values.
     * @param lowest The lowest value
     * @param highest The highest value.
     */
    value_allocator(value_type lowest, value_type highest)
        :lowest_{lowest}, highest_{highest} {

        BOOST_ASSERT(std::numeric_limits<value_type>::min() <= lowest);
        BOOST_ASSERT(highest <= std::numeric_limits<value_type>::max());
        // create one interval that contains whole values
        pool_.emplace(lowest_, highest_);
    }

    /**
     * @brief Allocate one value.
     * @return If allocator has at least one value, then returns lowest value, otherwise return nullopt.
     */
    optional<value_type> allocate() {
        if (pool_.empty()) return nullopt;

        // The smallest interval is the target.
        auto it = pool_.begin();
        value_type value = it->low();

        if (it->low() + 1 <= it->high()) {
            // If the interval contains other value, then update the interval.
            pool_.modify(
                it,
                [&](auto& e) {
                    BOOST_ASSERT(it->low() < highest_);
                    e = value_interval{value_type(it->low() + 1) , it->high()};
                }
            );
        }
        else {
            pool_.erase(it);
        }

        return value;
    }

    /**
     * @brief Get the first vacant value.
     * @return If allocator has at least one vacant value, then returns lowest value, otherwise return nullopt.
     */
    optional<value_type> first_vacant() const {
        if (pool_.empty()) return nullopt;

        // The smallest interval is the target.
        auto it = pool_.begin();
        return it->low();
    }

    /**
     * @brief Dellocate one value.
     * @param value value to deallocate. The value must be gotten by allocate() or declared by use().
     */
    void deallocate(value_type value) {
        BOOST_ASSERT(lowest_ <= value && value <= highest_);
        auto itr = pool_.upper_bound(value_interval{value});
        if (itr == pool_.end()) {

            // ..... v

            if (itr == pool_.begin()) {
                // value is fully allocated
                pool_.emplace(value);
                return;
            }

            auto itl = itr;
            --itl;
            if (itl->high() + 1 == value) { // Can concat to the left interval
                // Concat left
                pool_.modify(
                    itl,
                    [&](auto& e) {
                        e = value_interval{itl->low(), value};
                    }
                );
            }
            else {
                // No concat
                pool_.emplace(value);
            }
        }
        else if (itr == pool_.begin()) {

            // v .....

            if (value + 1 == itr->low()) { // Can concat to the right interval
                // Concat right
                pool_.modify(
                    itr,
                    [&](auto& e) {
                        e = value_interval{value, itr->high()};
                    }
                );
            }
            else {
                // No concat
                pool_.emplace(value);
            }
        }
        else {

            // .. v ..

            auto itl = itr;
            --itl;
            if (itl->high() + 1 == value) { // Can concat to the left interval
                if (value + 1 == itr->low()) { // Can concat to the right interval
                    // Concat both
                    auto right = itr->high();
                    pool_.erase(itr);
                    pool_.modify(
                        itl,
                        [&](auto& e) {
                            e = value_interval{itl->low(), right};
                        }
                    );

                }
                else {
                    // Concat left
                    pool_.modify(
                        itl,
                        [&](auto& e) {
                            e = value_interval{itl->low(), value};
                        }
                    );
                }
            }
            else {
                if (value + 1 == itr->low()) { // Can concat to the right interval
                    // Concat right
                    pool_.modify(
                        itr,
                        [&](auto& e) {
                            e = value_interval{value, itr->high()};
                        }
                    );
                }
                else {
                    // No concat
                    pool_.emplace(value);
                }
            }
        }
    }

    /**
     * @brief Declare the value as used.
     * @param value The value to declare using
     * @return If value is not used or allocated then true, otherwise false
     */
    bool use(value_type value) {
        auto it = pool_.find(value_interval{value});
        if (it == pool_.end()) return false;

        value_interval iv = *it;
        pool_.erase (it);
        if (iv.low() < value) {
            pool_.emplace(iv.low(), value - 1);
        }
        if (value + 1 <= iv.high()) {
            pool_.emplace(value + 1, iv.high());
        }
        return true;
    }

    /**
     * @brief Clear all allocated or used values.
     */
    void clear() {
        pool_.clear();
        pool_.emplace(lowest_, highest_);
    }

    std::size_t interval_count() const {
        return pool_.size();
    }

    std::ostream& dump(std::ostream& o) {
        for (auto const& e : pool_) {
            o << e;
        }
        return o;
    }
private:
    using mi_value_interval = mi::multi_index_container<
        value_interval,
        mi::indexed_by<
            mi::ordered_unique<
                mi::identity<value_interval>
            >
        >
    >;
    mi_value_interval pool_;
    value_type lowest_;
    value_type highest_;
};

} // namespace MQTT_NS

#endif // MQTT_VALUE_ALLOCATOR_HPP

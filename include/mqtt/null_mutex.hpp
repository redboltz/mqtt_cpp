// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_NULL_MUTEX_HPP)
#define MQTT_NULL_MUTEX_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

namespace MQTT_NS {


class null_mutex {
public:
  // construct/copy/destruct
    null_mutex() = default;
    ~null_mutex() = default;

    // public member functions
    void lock() {
    }
    bool try_lock() {
        return true;
    }
    bool timed_lock(const boost::posix_time::ptime &) {
        return true;
    }
    void unlock() {
    }
    void lock_sharable() {
    }
    bool try_lock_sharable() {
        return true;
    }
    bool timed_lock_sharable(const boost::posix_time::ptime &) {
        return true;
    }
    void unlock_sharable() {
    }
    void lock_upgradable() {
    }
    bool try_lock_upgradable() {
        return true;
    }
    bool timed_lock_upgradable(const boost::posix_time::ptime &) {
        return true;
    }
    void unlock_upgradable() {
    }
    void unlock_and_lock_upgradable() {
    }
    void unlock_and_lock_sharable() {
    }
    void unlock_upgradable_and_lock_sharable() {
    }
    void unlock_upgradable_and_lock() {
    }
    bool try_unlock_upgradable_and_lock() {
        return true;
    }
    bool timed_unlock_upgradable_and_lock(const boost::posix_time::ptime &) {
        return true;
    }
    bool try_unlock_sharable_and_lock() {
        return true;
    }
    bool try_unlock_sharable_and_lock_upgradable() {
        return true;
    }
};

} // namespace MQTT_NS

#endif // MQTT_NULL_MUTEX_HPP

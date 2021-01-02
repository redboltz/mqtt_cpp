// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PACKET_ID_MANAGER_HPP)
#define MQTT_PACKET_ID_MANAGER_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

#include <mqtt/optional.hpp>
#include <mqtt/value_allocator.hpp>

namespace MQTT_NS {

template <typename PacketId>
class packet_id_manager {
    using packet_id_t = PacketId;

public:

    /**
     * @brief Acquire the new unique packet id.
     *        If all packet ids are already in use, then returns nullopt
     *        After acquiring the packet id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return packet id
     */
    optional<packet_id_t> acquire_unique_id() {
        return va_.allocate();
    }

    /**
     * @brief Register packet_id to the library.
     *        After registering the packet_id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return If packet_id is successfully registerd then return true, otherwise return false.
     */
    bool register_id(packet_id_t packet_id) {
        return va_.use(packet_id);
    }

    /**
     * @brief Release packet_id.
     * @param packet_id packet id to release.
     *                   only the packet_id gotten by acquire_unique_packet_id, or
     *                   register_packet_id is permitted.
     */
    void release_id(packet_id_t packet_id) {
        va_.deallocate(packet_id);
    }

    /**
     * @brief Clear all packet ids.
     */
    void clear() {
        va_.clear();
    }

private:
    value_allocator<packet_id_t> va_ {1, std::numeric_limits<packet_id_t>::max()};
};

} // namespace MQTT_NS

#endif // MQTT_PACKET_ID_MANAGER_HPP

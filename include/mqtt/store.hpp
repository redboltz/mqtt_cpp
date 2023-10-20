// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_STORE_HPP)
#define MQTT_STORE_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <mqtt/any.hpp>
#include <mqtt/message_variant.hpp>
#include <mqtt/packet_id_type.hpp>

namespace MQTT_NS {

namespace mi = boost::multi_index;

enum class store_insert_update_result {
    inserted,
    updated
};

template <std::size_t PacketIdBytes>
class store {
private:
    struct tag_packet_id {};
    struct tag_packet_id_type {};
    struct tag_seq {};

public:
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    bool insert(
        packet_id_t packet_id,
        control_packet_type expected_type,
        basic_store_message_variant<PacketIdBytes> smv,
        any life_keeper
    ) {
        auto ret = elems_.emplace(
            packet_id,
            expected_type,
            force_move(smv),
            force_move(life_keeper)
        );
        return ret.second;
    }

    store_insert_update_result insert_or_update(
        packet_id_t packet_id,
        control_packet_type expected_type,
        basic_store_message_variant<PacketIdBytes> smv,
        any life_keeper
    ) {
        auto ret = elems_.emplace(
            packet_id,
            expected_type,
            force_move(smv),
            force_move(life_keeper)
        );
        if (ret.second) return store_insert_update_result::inserted;

        // When client want to restore serialized messages,
        // endpoint might keep the message that has the same packet_id.
        // In this case, overwrite the element.
        // entry exists
        elems_.modify(
            ret.first,
            [&] (auto& e) {
                e.packet_id_ = packet_id;
                e.expected_control_packet_type_ = expected_type;
                e.smv_ = force_move(smv);
                e.life_keeper_ = force_move(life_keeper);
            }
        );
        return store_insert_update_result::updated;
    }

    void for_each(
        std::function<
            // if return true, then erase element
            bool(basic_store_message_variant<PacketIdBytes> const&, any const&)
        > const& f
    ) {
        auto& idx = elems_.template get<tag_seq>();
        auto it = idx.begin();
        auto end = idx.end();
        while (it != end) {
            if (f(it->message(), it->life_keeper())) {
                it = idx.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    std::size_t erase(packet_id_t packet_id) {
        auto& idx = elems_.template get<tag_packet_id>();
        return idx.erase(packet_id);
    }

    bool erase(packet_id_t packet_id, control_packet_type type) {
        auto& idx = elems_.template get<tag_packet_id_type>();
        auto ret = idx.equal_range(std::make_tuple(packet_id, type));
        if (ret.first == ret.second) return false;
        idx.erase(ret.first, ret.second);
        return true;
    }

    void clear() {
        elems_.clear();
    }

    bool empty() const {
        return elems_.empty();
    }

private:

    struct elem_t {
        friend class store;

        elem_t(
            packet_id_t id,
            control_packet_type type,
            basic_store_message_variant<PacketIdBytes> smv,
            any life_keeper = any())
            : packet_id_(id)
            , expected_control_packet_type_(type)
            , smv_(force_move(smv))
            , life_keeper_(force_move(life_keeper)) {}
        packet_id_t packet_id() const { return packet_id_; }
        control_packet_type expected_control_packet_type() const { return expected_control_packet_type_; }
        basic_store_message_variant<PacketIdBytes> const& message() const {
            return smv_;
        }
        basic_store_message_variant<PacketIdBytes>& message() {
            return smv_;
        }
        any const& life_keeper() const {
            return life_keeper_;
        }
        bool is_publish() const {
            return
                expected_control_packet_type_ == control_packet_type::puback ||
                expected_control_packet_type_ == control_packet_type::pubrec;
        }

    private:
        packet_id_t packet_id_;
        control_packet_type expected_control_packet_type_;
        basic_store_message_variant<PacketIdBytes> smv_;
        any life_keeper_;
    };


    using mi_elem = mi::multi_index_container<
        elem_t,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_packet_id_type>,
                mi::composite_key<
                    elem_t,
                    mi::const_mem_fun<
                        elem_t, packet_id_t,
                        &elem_t::packet_id
                    >,
                    mi::const_mem_fun<
                        elem_t, control_packet_type,
                        &elem_t::expected_control_packet_type
                    >
                >
            >,
            mi::ordered_unique<
                mi::tag<tag_packet_id>,
                mi::const_mem_fun<
                    elem_t, packet_id_t,
                    &elem_t::packet_id
                >
            >,
            mi::sequenced<
                mi::tag<tag_seq>
            >
        >
    >;

    mi_elem elems_;
};

} // namespace MQTT_NS

#endif // MQTT_STORE_HPP

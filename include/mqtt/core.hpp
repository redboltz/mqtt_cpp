// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CORE_HPP)
#define MQTT_CORE_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/system/error_code.hpp>
#include <boost/assert.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/tls.hpp>
#include <mqtt/namespace.hpp>
#include <mqtt/attributes.hpp>
#include <mqtt/any.hpp>
#include <mqtt/fixed_header.hpp>
#include <mqtt/remaining_length.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/connect_flags.hpp>
#include <mqtt/will.hpp>
#include <mqtt/session_present.hpp>
#include <mqtt/subscribe_options.hpp>
#include <mqtt/publish.hpp>
#include <mqtt/connect_return_code.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/tcp_endpoint.hpp>
#include <mqtt/shared_scope_guard.hpp>
#include <mqtt/message_variant.hpp>
#include <mqtt/two_byte_util.hpp>
#include <mqtt/four_byte_util.hpp>
#include <mqtt/packet_id_type.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/protocol_version.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/shared_ptr_array.hpp>
#include <mqtt/type_erased_socket.hpp>
#include <mqtt/move.hpp>
#include <mqtt/deprecated.hpp>
#include <mqtt/deprecated_msg.hpp>
#include <mqtt/error_code.hpp>
#include <mqtt/log.hpp>
#include <mqtt/variant_visit.hpp>
#include <mqtt/topic_alias_send.hpp>
#include <mqtt/topic_alias_recv.hpp>
#include <mqtt/subscribe_entry.hpp>
#include <mqtt/shared_subscriptions.hpp>
#include <mqtt/packet_id_manager.hpp>
#include <mqtt/store.hpp>
#include <mqtt/event.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

// When https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90415 is fixed,
// update the condition.

// https://gcc.gnu.org/develop.html#timeline
#define MQTT_LIBSTDCXX_GCC_730 20180125 // workaround required
#define MQTT_LIBSTDCXX_GCC_740 20181206 // workaround required
#define MQTT_LIBSTDCXX_GCC_750 20191114 // workaround required
#define MQTT_LIBSTDCXX_GCC_810 20180502 // workaround required
#define MQTT_LIBSTDCXX_GCC_820 20180726
#define MQTT_LIBSTDCXX_GCC_830 20190222
#define MQTT_LIBSTDCXX_GCC_910 20190503 // workaround required
#define MQTT_LIBSTDCXX_GCC_920 20190812 // workaround required

#if !defined(MQTT_DISABLE_LIBSTDCXX_TUPLE_ANY_WORKAROUND)
#if defined(MQTT_STD_ANY) && defined(__GLIBCXX__) && (__GLIBCXX__ != MQTT_LIBSTDCXX_GCC_820) && (__GLIBCXX__ != MQTT_LIBSTDCXX_GCC_830)

template <>
struct std::is_constructible<std::tuple<std::any>> : std::true_type {
};

template <>
struct std::is_constructible<std::tuple<std::any>, std::tuple<std::any> const&> : std::true_type {
};

template <>
struct std::is_copy_constructible<std::tuple<std::any>> : std::true_type {
};

template <>
struct std::is_copy_constructible<std::_Head_base<0, std::any, false>> : std::true_type {
};

template <>
struct std::is_constructible<std::_Head_base<0, std::any, false>, std::_Head_base<0, std::any, false> const&> : std::true_type {
};

#endif // defined(MQTT_STD_ANY) && defined(__GLIBCXX__) && (__GLIBCXX__ != MQTT_LIBSTDCXX_GCC_820) && (__GLIBCXX__ != MQTT_LIBSTDCXX_GCC_830)
#endif // !defined(MQTT_DISABLE_LIBSTDCXX_TUPLE_ANY_WORKAROUND)

#undef MQTT_LIBSTDCXX_GCC_730
#undef MQTT_LIBSTDCXX_GCC_740
#undef MQTT_LIBSTDCXX_GCC_750
#undef MQTT_LIBSTDCXX_GCC_810
#undef MQTT_LIBSTDCXX_GCC_820
#undef MQTT_LIBSTDCXX_GCC_830
#undef MQTT_LIBSTDCXX_GCC_910
#undef MQTT_LIBSTDCXX_GCC_920

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif // defined(__GNUC__)

#include <boost/asio/yield.hpp>

namespace MQTT_NS {

namespace detail {


template <typename T>
constexpr
std::enable_if_t< ! std::is_convertible<std::decay_t<T>, publish_options>::value, bool>
check_qos_value(T const&) {
    return false;
}

constexpr bool check_qos_value(publish_options pubopts) {
    return pubopts.get_qos() != qos::at_most_once;
}

template<typename ... Params>
constexpr bool should_generate_packet_id(Params const& ... params) {
#if __cplusplus >= 201703L
    return (check_qos_value(params) || ...); // defaults to false for empty.
#else  // __cplusplus >= 201703L
    const bool results[] = {false, check_qos_value(params)... };
    bool ret = false;
    for(const bool val : results)
    {
        ret |= val;
    }
    return ret;
#endif // __cplusplus >= 201703L
}

} // namespace detail

namespace as = boost::asio;
namespace mi = boost::multi_index;

template <
    typename Mutex = std::mutex,
    template<typename...> class LockGuard = std::lock_guard,
    std::size_t PacketIdBytes = 2,
    typename Leaf = void
>
class core :
    public std::enable_shared_from_this<
        std::conditional_t<
            std::is_same<Leaf, void>::value,
            core<Mutex, LockGuard, PacketIdBytes, Leaf>,
            Leaf
        >
    >,
    public event<typename packet_id_type<PacketIdBytes>::type> {
public:
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;
private:
    using this_type = core<
        Mutex,
        LockGuard,
        PacketIdBytes,
        std::conditional_t<
            std::is_same<Leaf, void>::value,
            core<Mutex, LockGuard, PacketIdBytes, Leaf>,
            Leaf
        >
    >;
    using this_type_sp = std::shared_ptr<this_type>;
    using event_type = event<packet_id_t>;

    using event_type::on_close;
    using event_type::on_error;
    using event_type::check_is_valid_length;
    using event_type::on_pingreq;
    using event_type::on_pingresp;
    using event_type::on_pre_send;
    using event_type::on_pub_res_sent;
    using event_type::disconnect_on_mqtt_error;
    using event_type::connack_on_mqtt_error;

    using event_type::puback_on_recv_publish;
    using event_type::pubrec_on_recv_publish;
    using event_type::pubrel_on_recv_pubrec;
    using event_type::pubcomp_on_recv_pubrel;

public:
    using async_handler_t = std::function<void(error_code ec)>;

    /**
     * @brief Constructor for client
     * @param ioc io_context
     * @param version protocol_version
     * @param async_operation
     *        This flag effects the following automatic operation.
     *        - puback/pubrec/pubrel/pubcomp if auto_pub_response_ is true.
     *        - send store data (publish QoS1,2 and pubrel) on connack receive.
     *        - disconnect
     *          MQTT protocol requests sending connack/disconnect packet with error reason code if some error happens.<BR>
     *          This function choose sync/async connack/disconnect.<BR>
     */
    core(as::io_context& ioc, protocol_version version = protocol_version::undetermined)
        :version_(version),
         tim_pingresp_(ioc),
         tim_shutdown_(ioc)
    {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "create"
            << " version:" << version;
    }

    /**
     * @brief Constructor for server.
     * @param ioc io_context
     * @param socket connected socket. It should have already been connected with another core.
     * @param version protocol_version
     * @param async_operation
     *        This flag effects the following automatic operation.
     *        - puback/pubrec/pubrel/pubcomp if auto_pub_response_ is true.
     *        - send store data (publish QoS1,2 and pubrel) on connack receive.
     *        - disconnect
     *          MQTT protocol requests sending connack/disconnect packet with error reason code if some error happens.<BR>
     *          This function choose sync/async connack/disconnect.<BR>
     */
    explicit core(as::io_context& ioc, std::shared_ptr<MQTT_NS::socket> socket, protocol_version version = protocol_version::undetermined)
        :socket_(force_move(socket)),
         connected_(true),
         version_(version),
         tim_pingresp_(ioc),
         tim_shutdown_(ioc)
    {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "create"
            << " version:" << version;
    }

protected:
    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     * @param func A callback function that is called when async operation will finish.
     */
    MQTT_ALWAYS_INLINE void on_mqtt_message_processed(any session_life_keeper) override {
        if (async_read_on_message_processed_) {
            async_read_control_packet_type(force_move(session_life_keeper));
        }
    }

public:
    core(this_type const&) = delete;
    core(this_type&&) = delete;
    core& operator=(this_type const&) = delete;
    core& operator=(this_type&&) = delete;

    /**
     * @brief Get clean session.
     *
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349231<BR>
     * 3.1.2.4 Clean Session<BR>
     * After constructing a core, the clean session is set to false.
     * @return clean session
     */
    bool clean_session() const {
        return clean_start();
    }

    /**
     * @brief Get clean start.
     *
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     * 3.1.2.4 Clean Start<BR>
     * After constructing a core, the clean start is set to false.
     * @return clean start
     */
    bool clean_start() const {
        return clean_start_;
    }

    /**
     * @brief Set clean session.
     * @param cs clean session
     *
     * This function should be called before calling connect().<BR>
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349231<BR>
     * 3.1.2.4 Clean Session<BR>
     * After constructing a endpoint, the clean session is set to false.
     */
    void set_clean_session(bool cs) {
        set_clean_start(cs);
    }

    /**
     * @brief Set clean start.
     * @param cs clean start
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     * 3.1.2.4 Clean Start<BR>
     * After constructing a endpoint, the clean start is set to false.
     */
    void set_clean_start(bool cs) {
        clean_start_ = cs;
    }

    /**
     * @brief Get the client id.
     * @return The client id of this client.
     */
    std::string const& get_client_id() const {
        return client_id_;
    }

    /**
     * @brief Set client id.
     * @param id client id
     *
     * This function should be called before calling connect().<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     * 3.1.3.1 Client Identifier
     */
    void set_client_id(std::string id) {
        client_id_ = force_move(id);
    }

    /**
     * @brief get_total_bytes_received
     * @return The total bytes received on the socket.
     */
    std::size_t get_total_bytes_received() const {
        return total_bytes_received_;
    }

    /**
     * @brief Set auto publish response mode.
     * @param b set value
     *
     * When set auto publish response mode to true, puback, pubrec, pubrel,and pub comp automatically send.<BR>
     */
    void set_auto_pub_response(bool b = true) {
        auto_pub_response_ = b;
    }

    void set_packet_bulk_read_limit(std::size_t size) {
        packet_bulk_read_limit_ = size;
    }

    void set_props_bulk_read_limit(std::size_t size) {
        props_bulk_read_limit_ = size;
    }

    /**
     * @brief Set pingresp timeout
     * @param tim timeout value
     *
     * If tim is not zero, when the client sends PINGREQ, set a timer.
     * The timer cancels when PINGRESP is received. If the timer is fired, then force_disconnect
     * from the client side.<BR>
     * https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     * 3.1.2.10 Keep Alive
     */
    void set_pingresp_timeout(std::chrono::steady_clock::duration tim) {
        pingresp_timeout_ = force_move(tim);
    }

    /**
     * @brief set topic alias maximum for receiving
     * @param max maximum value
     *
     */
    void set_topic_alias_maximum(topic_alias_t max) {
        LockGuard<Mutex> lck (topic_alias_recv_mtx_);
        if (max == 0) {
            topic_alias_recv_ = nullopt;
        }
        else {
            topic_alias_recv_.emplace(max);
        }
    }



    /**
     * @brief start session with a connected core.
     * @param func finish handler that is called when the session is finished
     *
     */
    void start_session(any session_life_keeper = any()) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "start_session";
        shutdown_requested_ = false;
        async_read_control_packet_type(force_move(session_life_keeper));
    }

    void start_disconnect() {
        disconnect_requested_ = true;
    }

    /**
     * @brief Disconnect by core
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the core disconnects using force_disconnect(), a will will send.<BR>
     */
    void async_force_disconnect(
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_force_disconnect";
        socket_->post(
            [this, self = this->shared_from_this(), func = force_move(func)] () mutable {
                async_shutdown(socket(), force_move(func));
            }
        );
    }

    /**
     * @brief Clear stored publish message that has packet_id.
     * @param packet_id packet id corresponding to stored publish or pubrel
     */
    void clear_stored_publish(packet_id_t packet_id) {
        LockGuard<Mutex> lck (store_mtx_);
        store_.erase(packet_id);
        pid_man_.release_id(packet_id);
    }

    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(char const*, std::size_t)
     */
    void for_each_store(std::function<void(char const*, std::size_t)> const& f) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "for_each_store(ptr, size)";
        LockGuard<Mutex> lck (store_mtx_);
        store_.for_each(
            [f](
                basic_store_message_variant<PacketIdBytes> const& message,
                any const& /*life_keeper*/
            ) {
                auto cb = continuous_buffer(message);
                f(cb.data(), cb.size());
                return false; // no erase
            }
        );
    }

    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(store_message_variant)
     */
    void for_each_store(std::function<void(basic_store_message_variant<PacketIdBytes>)> const& f) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "for_each_store(store_message_variant)";
        LockGuard<Mutex> lck (store_mtx_);
        store_.for_each(
            [f](
                basic_store_message_variant<PacketIdBytes> const& message,
                any const& /*life_keeper*/
            ) {
                f(message);
                return false; // no erase
            }
        );
    }

    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(store_message_variant, any)
     */
    void for_each_store_with_life_keeper(std::function<void(basic_store_message_variant<PacketIdBytes>, any)> const& f) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)

            << "for_each_store(store_message_variant, life_keeper)";
        LockGuard<Mutex> lck (store_mtx_);
        store_.for_each(
            [f](
                basic_store_message_variant<PacketIdBytes> const& message,
                any const& life_keeper
            ) {
                f(message, life_keeper);
                return false; // no erase
            }
        );
    }

    /**
     * @brief Get processed but not released QoS2 packet ids
     *        This function should be called after disconnection
     * @return set of packet_ids
     */
    std::set<packet_id_t> get_qos2_publish_handled_pids() const {
        return qos2_publish_handled_;
    }

    /**
     * @brief Restore processed but not released QoS2 packet ids
     *        This function should be called before receive the first publish
     * @param pids packet ids
     */
    void restore_qos2_publish_handled_pids(std::set<packet_id_t> pids) {
        qos2_publish_handled_ = force_move(pids);
    }

    // manual packet_id management for advanced users

    /**
     * @brief Acquire the new unique packet id.
     *        If all packet ids are already in use, then throw packet_id_exhausted_error exception.
     *        After acquiring the packet id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return packet id
     */
    packet_id_t acquire_unique_packet_id() {
        if (auto pid = acquire_unique_packet_id_no_except()) return pid.value();
        throw packet_id_exhausted_error();
    }

    /**
     * @brief Acquire the new unique packet id.
     *        If all packet ids are already in use, then returns nullopt
     *        After acquiring the packet id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return packet id
     */
    optional<packet_id_t> acquire_unique_packet_id_no_except() {
        LockGuard<Mutex> lck (store_mtx_);
        return pid_man_.acquire_unique_id();
    }

    /**
     * @brief Register packet_id to the library.
     *        After registering the packet_id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return If packet_id is successfully registerd then return true, otherwise return false.
     */
    bool register_packet_id(packet_id_t packet_id) {
        LockGuard<Mutex> lck (store_mtx_);
        return pid_man_.register_id(packet_id);
    }

    /**
     * @brief Release packet_id.
     * @param packet_id packet id to release.
     *                   only the packet_id gotten by acquire_unique_packet_id, or
     *                   register_packet_id is permitted.
     */
    void release_packet_id(packet_id_t packet_id) {
        LockGuard<Mutex> lck (store_mtx_);
        pid_man_.release_id(packet_id);
    }

    /**
     * @brief Restore serialized publish and pubrel messages.
     *        This function should be called before connect.
     * @param b         iterator begin of the message
     * @param e         iterator end of the message
     */
    template <typename Iterator>
    std::enable_if_t< std::is_convertible<typename Iterator::value_type, char>::value >
    restore_serialized_message(Iterator b, Iterator e) {
        BOOST_ASSERT(version_ == protocol_version::v3_1_1);
        static_assert(
            std::is_same<
                typename std::iterator_traits<Iterator>::iterator_category,
                std::random_access_iterator_tag
            >::value,
            "Iterators provided to restore_serialized_message() must be random access iterators."
        );

        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "restore_serialized_message(b, e)";

        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        auto cpt_opt = get_control_packet_type_with_check(fixed_header);
        if (!cpt_opt) {
            MQTT_LOG("mqtt_api", error)
                << MQTT_ADD_VALUE(address, this)
                << "invalid fixed_header ignored. "
                << std::hex << static_cast<int>(fixed_header);
            throw malformed_packet_error();
        }
        switch (cpt_opt.value()) {
        case control_packet_type::publish: {
            auto buf = allocate_buffer(b, e);
            restore_serialized_message(
                basic_publish_message<PacketIdBytes>(
                    buf
                ),
                buf
            );
        } break;
        case control_packet_type::pubrel: {
            restore_serialized_message(
                basic_pubrel_message<PacketIdBytes>(
                    // basic_pubrel_message have no member variable that type is buffer.
                    // When creating basic_pubrel_message, the constructor just read buffer
                    // and convert to some values.
                    // So the argument buffer(...) doesn't need to hold the lifetime.
                    buffer(string_view(&*b, static_cast<std::size_t>(std::distance(b, e))))
                )
            );
        } break;
        default:
            MQTT_LOG("mqtt_api", error)
                << MQTT_ADD_VALUE(address, this)
                << "invalid control packet type. "
                << std::hex << static_cast<int>(fixed_header);
            throw protocol_error();
            break;
        }
    }

    /**
     * @brief Restore serialized publish message.
     *        This function should be called before connect.
     * @param msg         publish message.
     * @param life_keeper
     *        An object that stays alive (but is moved with force_move()) until the stored message is sent.
     */
    void restore_serialized_message(basic_publish_message<PacketIdBytes> msg, any life_keeper = {}) {
        BOOST_ASSERT(version_ == protocol_version::v3_1_1);
        auto packet_id = msg.packet_id();
        qos qos_value = msg.get_qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (pid_man_.register_id(packet_id)) {
            store_.insert_or_update(
                packet_id,
                ((qos_value == qos::at_least_once) ? control_packet_type::puback
                                                   : control_packet_type::pubrec),
                force_move(msg),
                force_move(life_keeper)
            );
        }
    }

    /**
     * @brief Restore serialized pubrel message.
     *        This function should be called before connect.
     * @param msg pubrel message.
     */
    void restore_serialized_message(basic_pubrel_message<PacketIdBytes> msg, any life_keeper = {}) {
        BOOST_ASSERT(version_ == protocol_version::v3_1_1);
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (pid_man_.register_id(packet_id)) {
            store_.insert_or_update(
                packet_id,
                control_packet_type::pubcomp,
                force_move(msg),
                force_move(life_keeper)
            );
        }
    }

    /**
     * @brief Restore serialized publish and pubrel messages.
     *        This function shouold be called before connect.
     * @param b         iterator begin of the message
     * @param e         iterator end of the message
     * Empty topic_name on MQTT v5 publish message is prohibited.
     */
    template <typename Iterator>
    std::enable_if_t< std::is_convertible<typename Iterator::value_type, char>::value >
    restore_v5_serialized_message(Iterator b, Iterator e) {
        BOOST_ASSERT(version_ == protocol_version::v5);
        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        auto cpt_opt = get_control_packet_type_with_check(fixed_header);
        if (!cpt_opt) {
            MQTT_LOG("mqtt_api", error)
                << MQTT_ADD_VALUE(address, this)
                << "invalid fixed_header ignored. "
                << std::hex << static_cast<int>(fixed_header);
            throw malformed_packet_error();
        }
        switch (cpt_opt.value()) {
        case control_packet_type::publish: {
            auto buf = allocate_buffer(b, e);
            restore_v5_serialized_message(
                v5::basic_publish_message<PacketIdBytes>(buf),
                buf
            );
        } break;
        case control_packet_type::pubrel: {
            auto buf = allocate_buffer(b, e);
            restore_v5_serialized_message(
                v5::basic_pubrel_message<PacketIdBytes>(buf),
                buf
            );
        } break;
        default:
            MQTT_LOG("mqtt_api", error)
                << MQTT_ADD_VALUE(address, this)
                << "invalid control packet type. "
                << std::hex << static_cast<int>(fixed_header);
            throw protocol_error();
            break;
        }
    }

    /**
     * @brief Restore serialized publish message.
     *        This function shouold be called before connect.
     * @param msg  publish message. Empty topic_name is prohibited.
     * @param life_keeper
     *        An object that stays alive (but is moved with force_move()) until the stored message is sent.
     */
    void restore_v5_serialized_message(v5::basic_publish_message<PacketIdBytes> msg, any life_keeper = {}) {
        BOOST_ASSERT(version_ == protocol_version::v5);
        BOOST_ASSERT(!msg.topic().empty());
        auto packet_id = msg.packet_id();
        auto qos = msg.get_qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (pid_man_.register_id(packet_id)) {
            store_.insert_or_update(
                packet_id,
                qos == qos::at_least_once ? control_packet_type::puback
                                          : control_packet_type::pubrec,
                force_move(msg),
                force_move(life_keeper)
            );
        }
    }

    /**
     * @brief Restore serialized pubrel message.
     *        This function shouold be called before connect.
     * @param msg pubrel message.
     * @param life_keeper
     *        An object that stays alive (but is moved with force_move()) until the stored message is sent.
     */
    void restore_v5_serialized_message(v5::basic_pubrel_message<PacketIdBytes> msg, any life_keeper = {}) {
        BOOST_ASSERT(version_ == protocol_version::v5);
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (pid_man_.register_id(packet_id)) {
            store_.insert_or_update(
                packet_id,
                control_packet_type::pubcomp,
                force_move(msg),
                force_move(life_keeper)
            );
        }
    }

private:
    struct restore_basic_message_variant_visitor {
        restore_basic_message_variant_visitor(this_type& ep, any life_keeper):ep_(ep), life_keeper_(force_move(life_keeper)) {}

        void operator()(basic_publish_message<PacketIdBytes>&& msg) {
            ep_.restore_serialized_message(force_move(msg), force_move(life_keeper_));
        }
        void operator()(basic_pubrel_message<PacketIdBytes>&& msg) {
            ep_.restore_serialized_message(force_move(msg), force_move(life_keeper_));
        }
        void operator()(v5::basic_publish_message<PacketIdBytes>&& msg) {
            ep_.restore_v5_serialized_message(force_move(msg), force_move(life_keeper_));
        }
        void operator()(v5::basic_pubrel_message<PacketIdBytes>&& msg) {
            ep_.restore_v5_serialized_message(force_move(msg), force_move(life_keeper_));
        }
        template <typename T>
        void operator()(T&&) const {
            throw restore_type_error();
        }
    private:
        this_type& ep_;
        any life_keeper_;
    };

public:
    void restore_serialized_message(basic_store_message_variant<PacketIdBytes> msg, any life_keeper = {}) {
        MQTT_NS::visit(restore_basic_message_variant_visitor(*this, force_move(life_keeper)), force_move(msg));
    }

    /**
     * @brief Check connection status
     * @return current connection status
     */
    bool connected() const {
        return connected_ && mqtt_connected_;
    }

    /**
     * @brief Check underlying layer connection status
     * @return current connection status
     */
    bool underlying_connected() const {
        return connected_;
    }

    /**
     * @brief Trigger next mqtt message manually.
     *        If you call this function, you need to set manual receive mode
     *        using set_auto_next_read(false);
     */
    void async_read_next_message(any session_life_keeper) {
        async_read_control_packet_type(force_move(session_life_keeper));
    }

    protocol_version get_protocol_version() const {
        return version_;
    }

    MQTT_NS::socket const& socket() const {
        return *socket_;
    }

    MQTT_NS::socket& socket() {
        return *socket_;
    }

    auto get_executor() {
        return socket_->get_executor();
    }

     /**
     * @brief Set maximum packet size that core can receive
     * If the core is client, then it sends as CONNECT packet property.
     * If the core is server, then it sends as CONNACK packet property.
     * If property is manually set, then maximum_packet_size_recv_ is overwritten by the property.
     *
     * @param size maximum packet size
     */
    void set_maximum_packet_size_recv(std::size_t size) {
        BOOST_ASSERT(size > 0 && size <= packet_size_no_limit);
        maximum_packet_size_recv_ = size;
    }

     /**
     * @brief Set receive maximum that core can receive
     * If the core is client, then it sends as CONNECT packet property.
     * If the core is server, then it sends as CONNACK packet property.
     * If property is manually set, then publish_recv_max_ is overwritten by the property.
     *
     * @param size maximum packet size
     */
    void set_receive_maximum(receive_maximum_t val) {
        BOOST_ASSERT(val > 0);
        publish_recv_max_ = val;
    }

protected:

    /**
     * @brief Get shared_ptr of socket
     * @return reference of std::shared_ptr<socket>
     */
    std::shared_ptr<MQTT_NS::socket>& socket_sp_ref() {
        return socket_;
    }

    void async_read_control_packet_type(any session_life_keeper) {
        socket_->async_read(
            as::buffer(buf_.data(), 1),
            [this, self = this->shared_from_this(), session_life_keeper = force_move(session_life_keeper)](
                error_code ec,
                std::size_t bytes_transferred) mutable {
                this->total_bytes_received_ += bytes_transferred;
                if (!check_error_and_transferred_length(ec, bytes_transferred, 1)) return;
                handle_control_packet_type(force_move(session_life_keeper), force_move(self));
            }
        );
    }

    bool handle_close_or_error(error_code ec) {
        auto call_handler =
            [this, ec] () mutable {
                clean_sub_unsub_inflight();
                if (disconnect_requested_) {
                    on_close();
                    disconnect_requested_ = false;
                }
                else {
                    if (!ec) ec = boost::system::errc::make_error_code(boost::system::errc::not_connected);
                    on_error(ec);
                }
            };

        if (connected_) {
            if (!ec) return false;
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "handle_close_or_error call shutdown";
            async_shutdown(
                socket(),
                [call_handler](error_code) mutable {
                    call_handler();
                }
            );
        }
        else {
            call_handler();
        }
        return true;
    }

    void set_connect() {
        connected_ = true;
    }

    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

    void clear_session_data() {
        LockGuard<Mutex> lck (store_mtx_);
        store_.clear();
        pid_man_.clear();
    }

    void async_shutdown(MQTT_NS::socket& s, async_handler_t func) {
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "shutdown";
        if (shutdown_requested_) {
            MQTT_LOG("mqtt_impl", trace)
                << MQTT_ADD_VALUE(address, this)
                << "already shutdowned";
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::success));
            return;
        }
        shutdown_requested_ = true;
        mqtt_connected_ = false;

        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "async_clean_shutdown_and_close";
        s.async_clean_shutdown_and_close(
            [this, func, sp = this->shared_from_this(), ssp = socket_sp_ref()](error_code ec) { // *1
                MQTT_LOG("mqtt_impl", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "async_clean_shutdown_and_close ec:"
                    << ec.message();
                tim_shutdown_.cancel();
                connected_ = false;
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::success));
            }
        );
        // timeout timer set
        tim_shutdown_.expires_after(shutdown_timeout);
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
        tim_shutdown_.async_wait(
            [this, func, wp = force_move(wp), ssp = socket_sp_ref()](error_code ec) mutable {
                if (auto sp = wp.lock()) {
                    MQTT_LOG("mqtt_impl", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "async_shutdown timer ec:"
                        << ec.message();
                    if (!ec) {
                        // timeout
                        // tcp_shutdown indirectly cancel stream.async_shutdown()
                        // and handler is called with error.
                        // So captured sp at *1 is released.

                        // post is for applying strand
                        MQTT_LOG("mqtt_impl", trace)
                            << MQTT_ADD_VALUE(address, this)
                            << "post force_shutdown_and_close";
                        sp->socket().post(
                            [this, func = force_move(func), sp] {
                                if (connected_) {
                                    error_code ec;
                                    socket().force_shutdown_and_close(ec);
                                    MQTT_LOG("mqtt_impl", trace)
                                        << MQTT_ADD_VALUE(address, this)
                                        << "force_shutdown_and_close ec:"
                                        << ec.message();
                                    connected_ = false;
                                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::success));
                                }
                            }
                        );
                    }
                }
            }
        );
    }

private:
    enum class connection_type {
        client,
        server
    };

    void update_values_and_props_on_start_connection(v5::properties& props) {
        // Check properties and overwrite the values by properties
        std::size_t topic_alias_maximum_count = 0;
        std::size_t maximum_packet_size_count = 0;
        std::size_t receive_maximum_count = 0;
        v5::visit_props(
            props,
            [&](v5::property::topic_alias_maximum const& p) {
                if (++topic_alias_maximum_count == 2) {
                    throw protocol_error();
                    return;
                }
                LockGuard<Mutex> lck (topic_alias_recv_mtx_);
                if (p.val() == 0) {
                    topic_alias_recv_ = nullopt;
                }
                else {
                    topic_alias_recv_.emplace(p.val());
                }
            },
            [&](v5::property::maximum_packet_size const& p) {
                if (++maximum_packet_size_count == 2) {
                    throw protocol_error();
                    return;
                }
                if (p.val() == 0) {
                    throw protocol_error();
                    return;
                }
                maximum_packet_size_recv_ = p.val();
            },
            [&](v5::property::receive_maximum const& p) {
                if (++receive_maximum_count == 2) {
                    throw protocol_error();
                    return;
                }
                if (p.val() == 0) {
                    throw protocol_error();
                    return;
                }
                publish_recv_max_ = p.val();
            },
            [](auto&&) {
            }
        );

        // If property is not set, then set property automatically.
        if (topic_alias_maximum_count == 0) {
            LockGuard<Mutex> lck (topic_alias_recv_mtx_);
            if (topic_alias_recv_ && topic_alias_recv_.value().max() != 0) {
                props.emplace_back(
                    MQTT_NS::v5::property::topic_alias_maximum(topic_alias_recv_.value().max())
                );
            }
        }
        if (maximum_packet_size_count == 0) {
            if (maximum_packet_size_recv_ != packet_size_no_limit) {
                props.emplace_back(
                    MQTT_NS::v5::property::maximum_packet_size(static_cast<std::uint32_t>(maximum_packet_size_recv_))
                );
            }
        }
        if (receive_maximum_count == 0) {
            if (publish_recv_max_ != receive_maximum_max) {
                props.emplace_back(
                    MQTT_NS::v5::property::receive_maximum(static_cast<receive_maximum_t>(publish_recv_max_))
                );
            }
        }
    }

    bool set_values_from_props_on_connection(connection_type type, v5::properties const& props) {

#define MQTT_SEND_ERROR(rc) \
        switch (type) {                                                 \
        case connection_type::client:                                   \
            disconnect_on_mqtt_error(v5::disconnect_reason_code::rc);   \
            break;                                                      \
        case connection_type::server:                                   \
            connack_on_mqtt_error(v5::connect_reason_code::rc);         \
            break;                                                      \
        default:                                                        \
            BOOST_ASSERT(false);                                        \
            break;                                                      \
        }

        bool ret = true;
        std::size_t topic_alias_maximum_count = 0;
        std::size_t maximum_packet_size_count = 0;
        std::size_t receive_maximum_count = 0;
        std::size_t assigned_client_identifier_count = 0;
        v5::visit_props(
            props,
            [&](v5::property::topic_alias_maximum const& p) {
                if (++topic_alias_maximum_count == 2) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                if (topic_alias_maximum_count > 2) {
                    ret = false;
                    return;
                }
                if (p.val() > 0) {
                    LockGuard<Mutex> lck (topic_alias_send_mtx_);
                    topic_alias_send_.emplace(p.val());
                }
            },
            [&](v5::property::maximum_packet_size const& p) {
                if (++maximum_packet_size_count == 2) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                if (maximum_packet_size_count > 2) {
                    ret = false;
                    return;
                }
                if (p.val() == 0) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                maximum_packet_size_send_ = p.val();
            },
            [&](v5::property::receive_maximum const& p) {
                if (++receive_maximum_count == 2) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                if (receive_maximum_count > 2) {
                    ret = false;
                    return;
                }
                if (p.val() == 0) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                publish_send_max_ = p.val();
            },
            [&](v5::property::assigned_client_identifier const& p) {
                if (type != connection_type::client) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                if (++assigned_client_identifier_count == 2) {
                    MQTT_SEND_ERROR(protocol_error);
                    ret = false;
                    return;
                }
                if (assigned_client_identifier_count > 2) {
                    ret = false;
                    return;
                }
                set_client_id(std::string(p.val()));
            },
            [](auto&&) {
            }
        );
#undef MQTT_SEND_ERROR

        return ret;
    }

    bool check_transferred_length(
        std::size_t bytes_transferred,
        std::size_t bytes_expected) {
        if (bytes_transferred != bytes_expected) {
            call_bad_message_error_handlers();
            return false;
        }
        return true;
    }

    bool check_error_and_transferred_length(
        error_code ec,
        std::size_t bytes_transferred,
        std::size_t bytes_expected) {
        if (handle_close_or_error(ec)) return false;
        if (!check_transferred_length(bytes_transferred, bytes_expected)) return false;
        return true;
    }

    void call_bad_message_error_handlers() {
        clean_sub_unsub_inflight_on_error(boost::system::errc::make_error_code(boost::system::errc::bad_message));
    }

    void call_protocol_error_handlers() {
        clean_sub_unsub_inflight_on_error(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
    }

    void handle_control_packet_type(any session_life_keeper, this_type_sp self) {
        fixed_header_ = static_cast<std::uint8_t>(buf_.front());
        remaining_length_ = 0;
        remaining_length_multiplier_ = 1;
        socket_->async_read(
            as::buffer(buf_.data(), 1),
            [this, self = force_move(self), session_life_keeper = force_move(session_life_keeper)] (
                error_code ec,
                std::size_t bytes_transferred) mutable {
                this->total_bytes_received_ += bytes_transferred;
                if (!check_error_and_transferred_length(ec, bytes_transferred, 1)) return;
                handle_remaining_length(force_move(session_life_keeper), force_move(self));
            }
        );
    }

    bool calc_variable_length(std::size_t& v, std::size_t& multiplier, char buf) {
        v += (buf & 0b01111111) * multiplier;
        multiplier *= 128;
        return multiplier <= 128 * 128 * 128 * 128;
    }

    void handle_remaining_length(any session_life_keeper, this_type_sp self) {
        if (!calc_variable_length(remaining_length_, remaining_length_multiplier_, buf_.front())) {
            call_bad_message_error_handlers();
            return;
        }
        if (buf_.front() & variable_length_continue_flag) {
            socket_->async_read(
                as::buffer(buf_.data(), 1),
                [this, self = force_move(self), session_life_keeper = force_move(session_life_keeper)](
                    error_code ec,
                    std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (handle_close_or_error(ec)) {
                        return;
                    }
                    if (bytes_transferred != 1) {
                        call_bad_message_error_handlers();
                        return;
                    }
                    handle_remaining_length(force_move(session_life_keeper), force_move(self));
                }
            );
        }
        else {
            auto cpt_opt = get_control_packet_type_with_check(fixed_header_);
            if (!cpt_opt) {
                call_bad_message_error_handlers();
                return;
            }
            auto cpt = cpt_opt.value();
            auto check =
                [&]() -> bool {
                    switch (version_) {
                    case protocol_version::v3_1_1:
                        switch (cpt) {
                        case control_packet_type::connect:
                        case control_packet_type::publish:
                        case control_packet_type::subscribe:
                        case control_packet_type::suback:
                        case control_packet_type::unsubscribe:
                            return check_is_valid_length(cpt, remaining_length_);
                        case control_packet_type::connack:
                            return remaining_length_ == 2;
                        case control_packet_type::puback:
                        case control_packet_type::pubrec:
                        case control_packet_type::pubrel:
                        case control_packet_type::pubcomp:
                        case control_packet_type::unsuback:
                            return remaining_length_ == sizeof(packet_id_t);
                        case control_packet_type::pingreq:
                        case control_packet_type::pingresp:
                        case control_packet_type::disconnect:
                            return remaining_length_ == 0;
                        // Even though there is no auth packet type in v3.1.1
                        // it's included in the switch case to provide a warning
                        // about missing enum values if any are missing.
                        case control_packet_type::auth:
                            return false;
                        }
                        return false;
                    case protocol_version::v5:
                    default:
                        switch (cpt) {
                        case control_packet_type::connect:
                        case control_packet_type::publish:
                        case control_packet_type::subscribe:
                        case control_packet_type::suback:
                        case control_packet_type::unsubscribe:
                        case control_packet_type::connack:
                        case control_packet_type::puback:
                        case control_packet_type::pubrec:
                        case control_packet_type::pubrel:
                        case control_packet_type::pubcomp:
                        case control_packet_type::unsuback:
                        case control_packet_type::disconnect:
                        case control_packet_type::auth:
                            return check_is_valid_length(cpt, remaining_length_);
                        case control_packet_type::pingreq:
                        case control_packet_type::pingresp:
                            return remaining_length_ == 0;
                        }
                        return false;
                    }
                };
            if (!check()) {
                call_protocol_error_handlers();
                return;
            }

            process_payload(force_move(session_life_keeper), force_move(self));
        }
    }

    void process_payload(any session_life_keeper, this_type_sp self) {
        auto control_packet_type = get_control_packet_type(fixed_header_);
        switch (control_packet_type) {
        case control_packet_type::connect:
            (*std::make_shared<process_connect>(*this, remaining_length_ < packet_bulk_read_limit_))
                (force_move(self), force_move(session_life_keeper));
            break;
        case control_packet_type::connack:
            (*std::make_shared<process_connack>(*this, remaining_length_ < packet_bulk_read_limit_))
                (force_move(self), force_move(session_life_keeper));
            break;
        case control_packet_type::publish:
            if (mqtt_connected_) {
                (*std::make_shared<process_publish>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::puback:
            if (mqtt_connected_) {
                (*std::make_shared<process_puback>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubrec:
            if (mqtt_connected_) {
                (*std::make_shared<process_pubrec>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubrel:
            if (mqtt_connected_) {
                (*std::make_shared<process_pubrel>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubcomp:
            if (mqtt_connected_) {
                (*std::make_shared<process_pubcomp>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::subscribe:
            if (mqtt_connected_) {
                (*std::make_shared<process_subscribe>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::suback:
            if (mqtt_connected_) {
                (*std::make_shared<process_suback>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::unsubscribe:
            if (mqtt_connected_) {
                (*std::make_shared<process_unsubscribe>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::unsuback:
            if (mqtt_connected_) {
                (*std::make_shared<process_unsuback>(*this, remaining_length_ < packet_bulk_read_limit_))
                    (force_move(self), force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pingreq:
            if (mqtt_connected_) {
                process_pingreq(force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pingresp:
            if (mqtt_connected_) {
                process_pingresp(force_move(session_life_keeper));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::disconnect:
            (*std::make_shared<process_disconnect>(*this, remaining_length_ < packet_bulk_read_limit_))
                (force_move(self), force_move(session_life_keeper));
            break;
        case control_packet_type::auth:
            (*std::make_shared<process_auth>(*this, remaining_length_ < packet_bulk_read_limit_))
                (force_move(self), force_move(session_life_keeper));
            break;
        default:
            break;
        }
    }

    using parse_handler_variant =
        variant<
        std::conditional_t<sizeof(std::size_t) == 4, std::nullptr_t, std::size_t>,
        std::uint32_t,
        std::uint16_t,
        buffer,
        v5::properties
    >;
    using parse_handler =
        std::function<
            void(
                this_type_sp&& spep,
                any&& session_life_keeper,
                parse_handler_variant,
                buffer
            )
        >;

    // primitive read functions
    void process_nbytes(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        std::size_t size,
        parse_handler&& handler
    ) {
        if (remaining_length_ < size) {
            call_protocol_error_handlers();
            return;
        }
        remaining_length_ -= size;

        if (buf.empty()) {
            auto spa = make_shared_ptr_array(size);
            auto ptr = spa.get();
            socket_->async_read(
                as::buffer(ptr, size),
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    handler = force_move(handler),
                    buf = buffer(string_view(ptr, size), force_move(spa))
                ]
                (error_code ec,
                 std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, buf.size())) return;
                    handler(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        buffer()
                    );
                }
            );
        }
        else {
            if (buf.size() < size) {
                call_protocol_error_handlers();
                return;
            }
            handler(
                force_move(self),
                force_move(session_life_keeper),
                buf.substr(0, size),
                buf.substr(size)
            );
        }
    }

    template <std::size_t Bytes>
    void process_fixed_length(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        if (remaining_length_ < Bytes) {
            call_protocol_error_handlers();
            return;
        }

        remaining_length_ -= Bytes;

        if (buf.empty()) {
            socket_->async_read(
                as::buffer(buf_.data(), Bytes),
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    handler = force_move(handler)
                ]
                (error_code ec,
                 std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, Bytes)) return;
                    handler(
                        force_move(self),
                        force_move(session_life_keeper),
                        make_two_or_four_byte<Bytes>::apply(
                            buf_.data(),
                            std::next(buf_.data(), boost::numeric_cast<buffer::difference_type>(Bytes))
                        ),
                        buffer()
                    );
                }
            );
        }
        else {
            auto val =
                make_two_or_four_byte<Bytes>::apply(
                    buf.data(),
                    std::next(buf.data(), boost::numeric_cast<buffer::difference_type>(Bytes))
                );
            buf.remove_prefix(Bytes);
            handler(
                force_move(self),
                force_move(session_life_keeper),
                val,
                force_move(buf)

            );
        }
    }

    // This function isn't used for remaining lengh.
    void process_variable_length(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        process_variable_length_impl(
            force_move(self),
            force_move(session_life_keeper),
            force_move(buf),
            force_move(handler),
            0,
            1
        );
    }

    void process_variable_length_impl(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler,
        std::size_t size,
        std::size_t multiplier
    ) {
        if (remaining_length_ == 0) {
            call_protocol_error_handlers();
            return;
        }
        --remaining_length_;

        // I use rvalue reference parameter to reduce move constructor calling.
        // This is a local lambda expression invoked from this function, so
        // I can control all callers.
        auto proc =
            [this]
            (
                this_type_sp&& self,
                any&& session_life_keeper,
                buffer&& buf,
                auto&& handler,
                std::size_t size,
                std::size_t multiplier
            ) mutable {
                if (!calc_variable_length(size, multiplier, buf.front())) {
                    call_protocol_error_handlers();
                    return;
                }
                if (buf.front() & variable_length_continue_flag) {
                    BOOST_ASSERT(!buf.empty());
                    buf.remove_prefix(1);
                    process_variable_length_impl(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        std::forward<decltype(handler)>(handler),
                        size,
                        multiplier
                    );
                }
                else {
                    buf.remove_prefix(1);
                    handler(
                        force_move(self),
                        force_move(session_life_keeper),
                        size,
                        force_move(buf)
                    );
                }
            };

        if (buf.empty()) {
            socket_->async_read(
                as::buffer(buf_.data(), 1),
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    handler = force_move(handler),
                    size,
                    multiplier,
                    proc = force_move(proc)
                ]
                (error_code ec,
                 std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, 1)) return;
                    proc(
                        force_move(self),
                        force_move(session_life_keeper),
                        buffer(string_view(buf_.data(), 1)), // buf_'s lifetime is handled by `self`
                        force_move(handler),
                        size,
                        multiplier
                    );
                }
            );
        }
        else {
            proc(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                force_move(handler),
                size,
                multiplier
            );
        }
    }

    void process_packet_id(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        process_fixed_length<sizeof(packet_id_t)>(
            force_move(self),
            force_move(session_life_keeper),
            force_move(buf),
            [
                this,
                handler = force_move(handler)
            ]
            (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                auto packet_id = variant_get<packet_id_t>(var);
                if (packet_id == 0) {
                    call_protocol_error_handlers();
                    return;
                }
                handler(
                    force_move(self),
                    force_move(session_life_keeper),
                    static_cast<packet_id_t>(packet_id),
                    force_move(buf)
                );
            }
        );
    }

    void process_binary(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        if (remaining_length_ < 2) {
            call_protocol_error_handlers();
            return;
        }
        process_fixed_length<2>(
            force_move(self),
            force_move(session_life_keeper),
            force_move(buf),
            [
                this,
                handler = force_move(handler)
            ]
            (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                auto size = variant_get<typename two_or_four_byte_type<2>::type>(var);
                if (remaining_length_ < size) {
                    call_protocol_error_handlers();
                    return;
                }
                process_nbytes(
                    force_move(self),
                    force_move(session_life_keeper),
                    force_move(buf),
                    size,
                    force_move(handler)
                );
            }
        );
    }

    void process_string(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        process_binary(
            force_move(self),
            force_move(session_life_keeper),
            force_move(buf),
            [this, handler = force_move(handler)]
            (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                auto& str = variant_get<buffer>(var);
                auto r = utf8string::validate_contents(str);
                if (r != utf8string::validation::well_formed) {
                    call_bad_message_error_handlers();
                    return;
                }
                handler(
                    force_move(self),
                    force_move(session_life_keeper),
                    force_move(str),
                    force_move(buf)
                );
            }
        );
    }


    void process_properties(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        parse_handler&& handler
    ) {
        process_variable_length(
            force_move(self),
            force_move(session_life_keeper),
            force_move(buf),
            [
                this,
                handler = force_move(handler)
            ]
            (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                auto property_length = variant_get<std::size_t>(var);
                if (property_length > remaining_length_) {
                    disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                    call_protocol_error_handlers();
                    return;
                }
                if (property_length == 0) {
                    handler(
                        force_move(self),
                        force_move(session_life_keeper),
                        v5::properties(),
                        force_move(buf)
                    );
                    return;
                }

                if (buf.empty()) {
                    struct spa_address_len {
                        shared_ptr_array spa;
                        char* address;
                        std::size_t len;
                    };
                    auto result =
                        [&] () -> spa_address_len {
                            if (property_length < props_bulk_read_limit_) {
                                auto spa = make_shared_ptr_array(property_length);
                                auto ptr = spa.get();
                                return
                                    {
                                        force_move(spa),
                                        ptr,
                                        property_length
                                    };
                            }
                            return
                                {
                                    nullptr,
                                    buf_.data(),
                                    1
                                };
                        } ();
                    socket_->async_read(
                        as::buffer(result.address, result.len),
                        [
                            this,
                            handler = force_move(handler),
                            self = force_move(self),
                            session_life_keeper = force_move(session_life_keeper),
                            property_length,
                            result
                        ]
                        (error_code ec, std::size_t bytes_transferred) mutable {
                            this->total_bytes_received_ += bytes_transferred;
                            if (!check_error_and_transferred_length(ec, bytes_transferred, result.len)) return;
                            process_property_id(
                                force_move(self),
                                force_move(session_life_keeper),
                                buffer(string_view(result.address, result.len), result.spa),
                                property_length,
                                v5::properties(),
                                force_move(handler)
                            );
                        }
                    );
                }
                else {
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        property_length,
                        v5::properties(),
                        force_move(handler)
                    );
                }
            }
        );
    }

    void process_property_id(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        std::size_t property_length_rest,
        v5::properties props,
        parse_handler&& handler
    ) {

        if (property_length_rest == 0) {
            handler(
                force_move(self),
                force_move(session_life_keeper),
                force_move(props),
                force_move(buf)
            );
            return;
        }

        --remaining_length_;
        if (buf.empty()) {
            socket_->async_read(
                as::buffer(buf_.data(), 1),
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (error_code ec,
                 std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, 1)) return;
                    process_property_body(
                        force_move(self),
                        force_move(session_life_keeper),
                        buffer(),
                        static_cast<v5::property::id>(buf_.front()),
                        property_length_rest - 1,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        }
        else {
            auto id = static_cast<v5::property::id>(buf.front());
            buf.remove_prefix(1);
            process_property_body(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                id,
                property_length_rest - 1,
                force_move(props),
                force_move(handler)
            );
        }
    }

    void process_property_body(
        this_type_sp&& self,
        any&& session_life_keeper,
        buffer buf,
        v5::property::id id,
        std::size_t property_length_rest,
        v5::properties props,
        parse_handler&& handler
    ) {

        static constexpr std::size_t length_bytes = 2;

        if (property_length_rest == 0) {
            disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
            call_protocol_error_handlers();
            return;
        }

        switch (id) {
        case v5::property::id::payload_format_indicator: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::payload_format_indicator(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::message_expiry_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::message_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::content_type: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::content_type(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::response_topic: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_topic(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::correlation_data: {
            process_binary(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::correlation_data(force_move(body))
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::subscription_identifier: {
            process_variable_length(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest,
                    remaining_length_before = remaining_length_
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto size = variant_get<std::size_t>(var);
                    auto consumed = remaining_length_before - remaining_length_;
                    auto rest = property_length_rest - consumed;
                    props.emplace_back(
                        v5::property::subscription_identifier(size)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::session_expiry_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::session_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::assigned_client_identifier: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::assigned_client_identifier(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );

        } break;
        case v5::property::id::server_keep_alive: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::server_keep_alive(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::authentication_method: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_method(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::authentication_data: {
            process_binary(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_data(force_move(body))
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::request_problem_information: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::request_problem_information(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::will_delay_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::will_delay_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::request_response_information: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::request_response_information(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::response_information: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_information(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::server_reference: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::server_reference(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::reason_string: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::reason_string(force_move(body), true)
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::receive_maximum: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::receive_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::topic_alias_maximum: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::topic_alias_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::topic_alias: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::topic_alias(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::maximum_qos: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::maximum_qos(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::retain_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::retain_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::user_property: {
            process_string(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& key = variant_get<buffer>(var);
                    auto rest = property_length_rest - length_bytes - key.size();
                    process_string(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        [
                            this,
                            props = force_move(props),
                            handler = force_move(handler),
                            key = force_move(key),
                            property_length_rest = rest
                        ]
                        (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                            auto& val = variant_get<buffer>(var);
                            auto rest = property_length_rest - length_bytes - val.size();
                            props.emplace_back(
                                v5::property::user_property(
                                    force_move(key),
                                    force_move(val),
                                    true,
                                    true
                                )
                            );
                            process_property_id(
                                force_move(self),
                                force_move(session_life_keeper),
                                force_move(buf),
                                rest,
                                force_move(props),
                                force_move(handler)
                            );
                        }
                    );
                }
            );
        } break;
        case v5::property::id::maximum_packet_size: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::maximum_packet_size(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::wildcard_subscription_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::wildcard_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::subscription_identifier_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::subscription_identifier_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        case v5::property::id::shared_subscription_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(self),
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (this_type_sp&& self, any&& session_life_keeper, parse_handler_variant var, buffer buf) mutable {
                    auto& body = variant_get<buffer>(var);
                    props.emplace_back(
                        v5::property::shared_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler)
                    );
                }
            );
        } break;
        }
    }

    // process common

    void process_header(
        this_type_sp&& self,
        any&& session_life_keeper,
        bool all_read,
        std::size_t header_len,
        parse_handler&& handler
    ) {

        if (all_read) {
            auto spa = make_shared_ptr_array(remaining_length_);
            auto ptr = spa.get();
            socket_->async_read(
                as::buffer(ptr, remaining_length_),
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    buf = buffer(string_view(ptr, remaining_length_), force_move(spa)),
                    handler = force_move(handler)
                ]
                (error_code ec, std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, remaining_length_)) return;
                    handler(
                        force_move(self),
                        force_move(session_life_keeper),
                        force_move(buf),
                        buffer()
                    );
                }
            );
            return;
        }

        if (header_len == 0) {
            force_move(handler)(
                force_move(self),
                force_move(session_life_keeper),
                buffer(),
                buffer()
            );
            return;
        }

        socket_->async_read(
            as::buffer(buf_.data(), header_len),
            [
                this,
                self = force_move(self),
                session_life_keeper = force_move(session_life_keeper),
                header_len,
                handler = force_move(handler)
            ]
            (error_code ec,
             std::size_t bytes_transferred) mutable {
                this->total_bytes_received_ += bytes_transferred;
                if (!check_error_and_transferred_length(ec, bytes_transferred, header_len)) return;
                handler(
                    force_move(self),
                    force_move(session_life_keeper),
                    buffer(string_view(buf_.data(), header_len)),
                    buffer()
                );
            }
        );
    }

    // process connect

    struct process_connect : as::coroutine, std::enable_shared_from_this<process_connect> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_connect;
        using process_type_sp = std::shared_ptr<process_type>;

        process_connect(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                yield {
                    auto& buf = variant_get<buffer>(var);

                    static constexpr char protocol_name[] = { 0x00, 0x04, 'M', 'Q', 'T', 'T' };
                    if (std::memcmp(buf.data(), protocol_name, sizeof(protocol_name)) != 0) {
                        ep_.call_protocol_error_handlers();
                        return;
                    }
                    std::size_t i = sizeof(protocol_name);
                    auto version = static_cast<protocol_version>(buf[i++]);
                    if (version != protocol_version::v3_1_1 && version != protocol_version::v5) {
                        ep_.call_protocol_error_handlers();
                        return;
                    }

                    if (ep_.version_ == protocol_version::undetermined) {
                        ep_.version_ = version;
                    }
                    else if (ep_.version_ != version) {
                        ep_.call_protocol_error_handlers();
                        return;
                    }

                    connect_flag_ = buf[i++];

                    keep_alive_ = make_uint16_t(buf[i], buf[i + 1]);
                    ep_.clean_start_ = connect_flags::has_clean_start(connect_flag_);

                    buf.remove_prefix(header_len_); // consume buffer
                    if (ep_.version_ == protocol_version::v5) {
                        // properties
                        ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                    }
                    else {
                        (*this)(force_move(spep), force_move(session_life_keeper), v5::properties(), force_move(buf));
                    }
                }
                props_ = force_move(variant_get<v5::properties>(var));

                // client_id
                yield ep_.process_string(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(remain_buf),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                client_id_ = force_move(variant_get<buffer>(var));
                ep_.set_client_id(std::string(client_id_));
                if (connect_flags::has_will_flag(connect_flag_)) {
                    if (ep_.version_ == protocol_version::v5) {
                        // will properties
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        will_props_ = force_move(variant_get<v5::properties>(var));
                    }
                    // will topic
                    yield ep_.process_string(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    will_topic_ = force_move(variant_get<buffer>(var));
                    // will payload
                    yield ep_.process_binary(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    will_payload_ = force_move(variant_get<buffer>(var));
                }
                if (connect_flags::has_user_name_flag(connect_flag_)) {
                    yield ep_.process_string(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    user_name_ = force_move(variant_get<buffer>(var));
                }
                if (connect_flags::has_password_flag(connect_flag_)) {
                    yield ep_.process_binary(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    password_ = force_move(variant_get<buffer>(var));
                }
                ep_.mqtt_connected_ = true;
                {
                    ep_.publish_send_count_ = 0;
                    {
                        LockGuard<Mutex> lck (ep_.resend_pubrel_mtx_);
                        ep_.resend_pubrel_.clear();
                    }
                    {
                        LockGuard<Mutex> lck (ep_.publish_received_mtx_);
                        ep_.publish_received_.clear();
                    }
                    {
                        LockGuard<Mutex> lck (ep_.publish_send_queue_mtx_);
                        ep_.publish_send_queue_.clear();
                    }
                }
                if (!ep_.set_values_from_props_on_connection(connection_type::server, props_)) return;
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    if (ep_.on_connect(
                            force_move(client_id_),
                            force_move(user_name_),
                            force_move(password_),
                            connect_flags::has_will_flag(connect_flag_)
                            ? optional<will>(in_place_init,
                                             force_move(will_topic_),
                                             force_move(will_payload_),
                                             connect_flags::has_will_retain(connect_flag_) | connect_flags::will_qos(connect_flag_))
                            : optional<will>(nullopt),
                            ep_.clean_session(),
                            keep_alive_
                        )
                    ) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                case protocol_version::v5:
                    if (ep_.on_v5_connect(
                            force_move(client_id_),
                            force_move(user_name_),
                            force_move(password_),
                            connect_flags::has_will_flag(connect_flag_)
                            ? optional<will>(in_place_init,
                                             force_move(will_topic_),
                                             force_move(will_payload_),
                                             connect_flags::has_will_retain(connect_flag_) | connect_flags::will_qos(connect_flag_),
                                             force_move(will_props_))
                            : optional<will>(nullopt),
                            ep_.clean_start(),
                            keep_alive_,
                            force_move(props_)
                        )
                    ) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                default:
                    BOOST_ASSERT(false);
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ =
            2 +  // string length
            4 +  // "MQTT" string
            1 +  // ProtocolVersion
            1 +  // ConnectFlag
            2;   // KeepAlive

        ep_t& ep_;
        bool all_read_;

        char connect_flag_;
        std::uint16_t keep_alive_;
        v5::properties props_;
        buffer client_id_;
        v5::properties will_props_;
        buffer will_topic_;
        buffer will_payload_;
        optional<buffer> user_name_;
        optional<buffer> password_;
    };
    friend struct process_connect;

    // process connack

    struct process_connack : as::coroutine, std::enable_shared_from_this<process_connack> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_connack;
        using process_type_sp = std::shared_ptr<process_type>;

        process_connack(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer /*remain_buf*/ = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                yield {
                    auto& buf = variant_get<buffer>(var);
                    session_present_ = is_session_present(buf[0]);
                    switch (ep_.version_) {
                    case protocol_version::v3_1_1:
                        reason_code_ = static_cast<connect_return_code>(buf[1]);
                        break;
                    case protocol_version::v5:
                        reason_code_ = static_cast<v5::connect_reason_code>(buf[1]);
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                    buf.remove_prefix(header_len_); // consume buffer
                    if (ep_.version_ == protocol_version::v5) {
                        // properties
                        ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                    }
                    else {
                        (*this)(force_move(spep), force_move(session_life_keeper), v5::properties(), force_move(buf));
                    }
                }
                props_ = force_move(variant_get<v5::properties>(var));
                ep_.mqtt_connected_ = true;
                {
                    ep_.publish_send_count_ = 0;
                    {
                        LockGuard<Mutex> lck (ep_.resend_pubrel_mtx_);
                        ep_.resend_pubrel_.clear();
                    }
                    {
                        LockGuard<Mutex> lck (ep_.publish_received_mtx_);
                        ep_.publish_received_.clear();
                    }
                    {
                        LockGuard<Mutex> lck (ep_.publish_send_queue_mtx_);
                        ep_.publish_send_queue_.clear();
                    }
                }
                if (!ep_.set_values_from_props_on_connection(connection_type::client, props_)) return;
                {
                    auto connack_proc =
                        [this]
                        (any&& session_life_keeper) mutable {
                            switch (ep_.version_) {
                            case protocol_version::v3_1_1:
                                if (ep_.on_connack(
                                        session_present_,
                                        variant_get<connect_return_code>(reason_code_)
                                    )
                                ) {
                                    ep_.on_mqtt_message_processed(
                                        force_move(
                                            std::get<0>(
                                                any_cast<
                                                    std::tuple<any, process_type_sp>
                                                >(session_life_keeper)
                                            )
                                        )
                                    );
                                }
                                break;
                            case protocol_version::v5:
                                if (ep_.on_v5_connack(
                                        session_present_,
                                        variant_get<v5::connect_reason_code>(reason_code_),
                                        force_move(props_)
                                    )
                                ) {
                                    ep_.on_mqtt_message_processed(
                                        force_move(
                                            std::get<0>(
                                                any_cast<
                                                    std::tuple<any, process_type_sp>
                                                >(session_life_keeper)
                                            )
                                        )
                                    );
                                }
                                break;
                            default:
                                BOOST_ASSERT(false);
                            }
                        };

                    // Note: boost:variant has no featue to query if the variant currently holds a specific type.
                    // MQTT_CPP could create a type traits function to match the provided type to the index in
                    // the boost::variant type list, but for now it does not appear to be needed.
                    if (   (   (0 == variant_idx(reason_code_))
                               && (connect_return_code::accepted == variant_get<connect_return_code>(reason_code_)))
                           || (   (1 == variant_idx(reason_code_))
                                  && (v5::connect_reason_code::success == variant_get<v5::connect_reason_code>(reason_code_)))) {

                        // If session_present is false, then call clear_session_data().
                        // Here is the reason why it works well.
                        // ---
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901078
                        //
                        // If the Server accepts a connection with Clean Start set to 1, the Server
                        // MUST set Session Present to 0 in the CONNACK packet in addition to setting
                        // a 0x00 (Success) Reason Code in the CONNACK packet [MQTT-3.2.2-2].
                        //
                        //
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901048
                        //
                        // The Client can avoid implementing its own Session expiry and instead rely on
                        // the Session Present flag returned from the Server to determine if the Session
                        // had expired. If the Client does implement its own Session expiry, it needs to
                        // store the time at which the Session State will be deleted as part of its
                        // Session State.
                        // ---
                        //
                        // Also it can  avoid the following client side and broker side session state
                        // mismatch autonatically.
                        //
                        // ---
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901078
                        //
                        // If the Client does not have Session State and receives Session Present
                        // set to 1 it MUST close the Network Connection [MQTT-3.2.2-4]. If it
                        // wishes to restart with a new Session the Client can reconnect using
                        // Clean Start set to 1.
                        // If the Client does have Session State and receives Session Present set
                        // to 0 it MUST discard its Session State if it continues with the Network
                        // Connection [MQTT-3.2.2-5].
                        // ---
                        if (session_present_) {
                            // Until all stored messages are written to internal send buffer,
                            // disable further async reading of incoming packets..
                            ep_.async_read_on_message_processed_ = false;
                            auto async_connack_proc =
                                [
                                    this,
                                    spep = force_move(spep),
                                    session_life_keeper = force_move(session_life_keeper),
                                    connack_proc = force_move(connack_proc)
                                ]
                                () mutable {
                                    // All stored messages are sent, so re-enable reading of incoming packets.
                                    // and notify the end user code that the connack packet was received.
                                    ep_.async_read_on_message_processed_ = true;
                                    connack_proc(force_move(session_life_keeper));
                                };
                            ep_.async_send_store(force_move(async_connack_proc));
                        }
                        else {
                            ep_.clear_session_data();
                        }
                    }
                    connack_proc(force_move(session_life_keeper));
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ =
            1 +  // Connect Acknowledge Flags
            1;   // Reason Code

        ep_t& ep_;
        bool all_read_;

        bool session_present_;
        variant<connect_return_code, v5::connect_reason_code> reason_code_;
        v5::properties props_;
    };
    friend struct process_connack;

    // process publish

    struct process_publish : as::coroutine, std::enable_shared_from_this<process_publish> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_publish;
        using process_type_sp = std::shared_ptr<process_type>;

        process_publish(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < min_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    0,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // topic_name
                yield ep_.process_string(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                topic_name_ = force_move(variant_get<buffer>(var));

                qos_value_ = publish::get_qos(ep_.fixed_header_);
                if (qos_value_ != qos::at_most_once &&
                    qos_value_ != qos::at_least_once &&
                    qos_value_ != qos::exactly_once) {
                    ep_.call_bad_message_error_handlers();
                    return;
                }

                if (qos_value_ == qos::at_least_once ||
                    qos_value_ == qos::exactly_once) {
                    yield ep_.process_packet_id(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    packet_id_ = force_move(variant_get<packet_id_t>(var));
                }
                if (ep_.version_ == protocol_version::v5) {
                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));
                }
                yield ep_.process_nbytes(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(remain_buf),
                    ep_.remaining_length_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                {
                    auto handler_call =
                        [&] {
                            auto check_full =
                                [&] {
                                    LockGuard<Mutex> lck (ep_.publish_received_mtx_);
                                    if (ep_.publish_received_.size() == ep_.publish_recv_max_) {
                                        return true;
                                    }
                                    ep_.publish_received_.insert(*packet_id_);
                                    return false;
                                };
                            switch (ep_.version_) {
                            case protocol_version::v3_1_1:
                                return ep_.on_publish(
                                    packet_id_,
                                    publish_options(ep_.fixed_header_),
                                    force_move(topic_name_),
                                    force_move(variant_get<buffer>(var))
                                );
                            case protocol_version::v5:
                                switch (qos_value_) {
                                case qos::at_most_once:
                                    break;
                                    // automatically response error using puback / pubrec
                                    // but the connection continues.
                                    // publish handler is not called
                                case qos::at_least_once:
                                    if (check_full()) {
                                        ep_.puback_on_recv_publish(
                                            *packet_id_,
                                            v5::puback_reason_code::quota_exceeded
                                        );
                                        ep_.on_mqtt_message_processed(
                                            force_move(
                                                std::get<0>(
                                                    any_cast<
                                                    std::tuple<any, process_type_sp>
                                                    >(session_life_keeper)
                                                )
                                            )
                                        );
                                        return false;
                                    }
                                    break;
                                case qos::exactly_once:
                                    if (check_full()) {
                                        ep_.pubrec_on_recv_publish(
                                            *packet_id_,
                                            v5::pubrec_reason_code::quota_exceeded
                                        );
                                        ep_.on_mqtt_message_processed(
                                            force_move(
                                                std::get<0>(
                                                    any_cast<
                                                    std::tuple<any, process_type_sp>
                                                    >(session_life_keeper)
                                                )
                                            )
                                        );
                                        return false;
                                    }
                                    break;
                                }
                                if (topic_name_.empty()) {
                                    if (auto topic_alias = get_topic_alias_from_props(props_)) {
                                        if (topic_alias.value() == 0 ||
                                            topic_alias.value() > ep_.topic_alias_recv_.value().max()) {
                                            ep_.disconnect_on_mqtt_error(v5::disconnect_reason_code::topic_alias_invalid);
                                            ep_.call_protocol_error_handlers();
                                            return false;
                                        }
                                        auto topic_name = [&] {
                                            LockGuard<Mutex> lck (ep_.topic_alias_recv_mtx_);
                                            if (ep_.topic_alias_recv_) {
                                                return ep_.topic_alias_recv_.value().find(topic_alias.value());
                                            }
                                            return std::string();
                                        }();
                                        if (topic_name.empty()) {
                                            MQTT_LOG("mqtt_cb", error)
                                                << MQTT_ADD_VALUE(address, &ep_)
                                                << "no matching topic alias: "
                                                << topic_alias.value();
                                            ep_.disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                                            ep_.call_protocol_error_handlers();
                                            return false;
                                        }
                                        else {
                                            topic_name_ = allocate_buffer(topic_name);
                                        }
                                    }
                                }
                                else {
                                    if (auto topic_alias = ep_.get_topic_alias_from_props(props_)) {
                                        LockGuard<Mutex> lck (ep_.topic_alias_recv_mtx_);
                                        if (ep_.topic_alias_recv_) {
                                            ep_.topic_alias_recv_.value().insert_or_update(topic_name_, topic_alias.value());
                                        }
                                    }
                                }
                                {
                                    auto ret =  ep_.on_v5_publish(
                                        packet_id_,
                                        publish_options(ep_.fixed_header_),
                                        force_move(topic_name_),
                                        force_move(variant_get<buffer>(var)),
                                        force_move(props_)
                                    );
                                    return ret;
                                }
                            default:
                                BOOST_ASSERT(false);
                            }
                            return false;
                        };
                    switch (qos_value_) {
                    case qos::at_most_once:
                        if (handler_call()) {
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                        std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    case qos::at_least_once:
                        if (handler_call()) {
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                        std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                            // TBD ep_.connect_ condition is removed. Maybe it is not required
                            if (ep_.auto_pub_response_) {
                                ep_.puback_on_recv_publish(
                                    *packet_id_,
                                    v5::puback_reason_code::success
                                );
                            }
                        }
                        break;
                    case qos::exactly_once:
                        if (ep_.qos2_publish_handled_.find(*packet_id_) == ep_.qos2_publish_handled_.end()) {
                            if (handler_call()) {
                                ep_.on_mqtt_message_processed(
                                    force_move(
                                        std::get<0>(
                                            any_cast<
                                            std::tuple<any, process_type_sp>
                                            >(session_life_keeper)
                                        )
                                    )
                                );
                                ep_.qos2_publish_handled_.emplace(*packet_id_);
                                // TBD ep_.connect_ condition is removed. Maybe it is not required
                                if (ep_.auto_pub_response_) {
                                    ep_.pubrec_on_recv_publish(
                                        *packet_id_,
                                        v5::pubrec_reason_code::success
                                    );
                                }
                            }
                        }
                        else {
                            // publish has already been handled
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                        std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                            ep_.pubrec_on_recv_publish(
                                *packet_id_,
                                v5::pubrec_reason_code::success
                            );
                        }
                        break;
                    }
                }
            }
        }

    private:
        static constexpr std::size_t min_len_ = 2; // topic name length

        ep_t& ep_;
        bool all_read_;

        buffer topic_name_;
        qos qos_value_;
        optional<packet_id_t> packet_id_;
        v5::properties props_;
        buffer payload_;
    };
    friend struct process_publish;

    // process puback

    struct process_puback : as::coroutine, std::enable_shared_from_this<process_puback> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_puback;
        using process_type_sp = std::shared_ptr<process_type>;

        process_puback(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.remaining_length_ == 0) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    reason_code_ = v5::puback_reason_code::success;
                }
                else {
                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::puback_reason_code>(variant_get<buffer>(var)[0]);

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used

                    if (ep_.remaining_length_ > 0) {
                        // properties
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        props_ = force_move(variant_get<v5::properties>(var));
                    }
                }
                auto erased =
                    [&] {
                        LockGuard<Mutex> lck (ep_.store_mtx_);
                        if (!ep_.store_.erase(packet_id_, control_packet_type::puback)) {
                            // puback packet_id is not matched to publish
                            return false;
                        }

                        ep_.pid_man_.release_id(packet_id_);
                        return true;
                    } ();
                if (erased) ep_.on_serialize_remove(packet_id_);
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    if (ep_.on_puback(packet_id_)) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                case protocol_version::v5:
                    if (erased) ep_.send_publish_queue_one();
                    if (ep_.on_v5_puback(packet_id_, reason_code_, force_move(props_))) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                default:
                    BOOST_ASSERT(false);
                }


            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::puback_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_puback;

    // process pubrec

    struct process_pubrec : as::coroutine, std::enable_shared_from_this<process_pubrec> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_pubrec;
        using process_type_sp = std::shared_ptr<process_type>;

        process_pubrec(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.remaining_length_ == 0) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    reason_code_ = v5::pubrec_reason_code::success;
                }
                else {
                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::pubrec_reason_code>(variant_get<buffer>(var)[0]);

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used

                    if (ep_.remaining_length_ > 0) {
                        // properties
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        props_ = force_move(variant_get<v5::properties>(var));
                    }
                }
                auto erased =
                    [&] {
                        LockGuard<Mutex> lck (ep_.store_mtx_);
                        if (!ep_.store_.erase(packet_id_, control_packet_type::pubrec)) {
                            // pubrec packet_id is not matched to publish
                            return false;
                        }

                        // packet_id should be erased here only if reason_code is error.
                        // Otherwise the packet_id is continue to be used for pubrel/pubcomp.
                        if (is_error(reason_code_)) ep_.pid_man_.release_id(packet_id_);
                        return true;
                    } ();
                {
                    auto res =
                        [&] {
                            if (ep_.auto_pub_response_) {
                                auto rc =
                                    [&] {
                                        if (erased) return v5::pubrel_reason_code::success;
                                        return v5::pubrel_reason_code::packet_identifier_not_found;
                                    } ();
                                ep_.pubrel_on_recv_pubrec(
                                    packet_id_,
                                    rc
                                );
                            }
                        };
                    switch (ep_.version_) {
                    case protocol_version::v3_1_1:
                        if (ep_.on_pubrec(packet_id_)) {
                            res();
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    case protocol_version::v5:
                        if (erased && is_error(reason_code_)) {
                            ep_.on_serialize_remove(packet_id_);
                            ep_.send_publish_queue_one();
                        }
                        if (ep_.on_v5_pubrec(packet_id_, reason_code_, force_move(props_))) {
                            if (!is_error(reason_code_)) res();
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::pubrec_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_pubrec;

    struct process_pubrel : as::coroutine, std::enable_shared_from_this<process_pubrel> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_pubrel;
        using process_type_sp = std::shared_ptr<process_type>;

        process_pubrel(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.remaining_length_ == 0) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    reason_code_ = v5::pubrel_reason_code::success;
                }
                else {
                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::pubrel_reason_code>(variant_get<buffer>(var)[0]);

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used

                    if (ep_.remaining_length_ > 0) {
                        // properties
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        props_ = force_move(variant_get<v5::properties>(var));
                    }
                }

                {
                    auto res =
                        [&] {
                            if (ep_.auto_pub_response_) {
                                ep_.pubcomp_on_recv_pubrel(
                                    packet_id_,
                                    static_cast<v5::pubcomp_reason_code>(reason_code_)
                                );
                            }
                        };
                    ep_.qos2_publish_handled_.erase(packet_id_);
                    switch (ep_.version_) {
                    case protocol_version::v3_1_1:
                        if (ep_.on_pubrel(packet_id_)) {
                            res();
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    case protocol_version::v5:
                        if (ep_.on_v5_pubrel(packet_id_, reason_code_, force_move(props_))) {
                            res();
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::pubrel_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_pubrel;

    // process pubcomp

    struct process_pubcomp : as::coroutine, std::enable_shared_from_this<process_pubcomp> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_pubcomp;
        using process_type_sp = std::shared_ptr<process_type>;

        process_pubcomp(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.remaining_length_ == 0) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    reason_code_ = v5::pubcomp_reason_code::success;
                }
                else {
                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::pubcomp_reason_code>(variant_get<buffer>(var)[0]);

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used

                    if (ep_.remaining_length_ > 0) {
                        // properties
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        props_ = force_move(variant_get<v5::properties>(var));
                    }
                }
                auto erased =
                    [&] {
                        LockGuard<Mutex> lck (ep_.store_mtx_);
                        if (!ep_.store_.erase(packet_id_, control_packet_type::pubcomp)) {
                            // pubcomp packet_id is not matched to pubrel
                            return false;
                        }

                        ep_.pid_man_.release_id(packet_id_);
                        return true;
                    } ();
                if (erased) ep_.on_serialize_remove(packet_id_);
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    if (ep_.on_pubcomp(packet_id_)) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                case protocol_version::v5:
                    if (
                        erased &&
                        [&] {
                            LockGuard<Mutex> lck (ep_.resend_pubrel_mtx_);
                            return ep_.resend_pubrel_.find(packet_id_) == ep_.resend_pubrel_.end();
                        } ()
                    ) {
                        ep_.send_publish_queue_one();
                    }
                    if (ep_.on_v5_pubcomp(packet_id_, reason_code_, force_move(props_))) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                default:
                    BOOST_ASSERT(false);
                }


            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::pubcomp_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_pubcomp;

    // process subscribe

    struct process_subscribe : as::coroutine, std::enable_shared_from_this<process_subscribe> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_subscribe;
        using process_type_sp = std::shared_ptr<process_type>;

        process_subscribe(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.version_ == protocol_version::v5) {
                    // properties
                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));
                }

                while (true) {
                    // topic_filter including share_name
                    yield ep_.process_string(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    sn_tf_opt_ = parse_shared_subscription(variant_get<buffer>(var));
                    if (!sn_tf_opt_) {
                        MQTT_LOG("mqtt_impl", error)
                            << MQTT_ADD_VALUE(address, &ep_)
                            << "topic_filter parse error"
                            << " whole_topic_filter: "
                            << variant_get<buffer>(var);
                        ep_.disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                        ep_.call_protocol_error_handlers();
                        return;
                    }

                    // subscribe options
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    sub_opts_opt_.emplace(static_cast<std::uint8_t>(variant_get<buffer>(var)[0]));
                    if (sub_opts_opt_.value().get_qos() != qos::at_most_once &&
                        sub_opts_opt_.value().get_qos() != qos::at_least_once &&
                        sub_opts_opt_.value().get_qos() != qos::exactly_once) {
                        ep_.call_bad_message_error_handlers();
                        return;
                    }

                    entries_.emplace_back(
                        force_move(sn_tf_opt_.value().share_name),
                        force_move(sn_tf_opt_.value().topic_filter),
                        sub_opts_opt_.value()
                    );

                    if (ep_.remaining_length_ == 0) {
                        switch (ep_.version_) {
                        case protocol_version::v3_1_1:
                            if (ep_.on_subscribe(packet_id_, force_move(entries_))) {
                                ep_.on_mqtt_message_processed(
                                    force_move(
                                        std::get<0>(
                                            any_cast<
                                                std::tuple<any, process_type_sp>
                                            >(session_life_keeper)
                                        )
                                    )
                                );
                            }
                            break;
                        case protocol_version::v5:
                            if (ep_.on_v5_subscribe(packet_id_, force_move(entries_), force_move(props_))) {
                                ep_.on_mqtt_message_processed(
                                    force_move(
                                        std::get<0>(
                                            any_cast<
                                                std::tuple<any, process_type_sp>
                                            >(session_life_keeper)
                                        )
                                    )
                                );
                            }
                            break;
                        default:
                            BOOST_ASSERT(false);
                        }
                        return;
                    }
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::properties props_;
        optional<share_name_topic_filter> sn_tf_opt_;
        optional<subscribe_options> sub_opts_opt_;
        std::vector<subscribe_entry> entries_;
    };
    friend struct process_subscribe;

    // process suback

    struct process_suback : as::coroutine, std::enable_shared_from_this<process_suback> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_suback;
        using process_type_sp = std::shared_ptr<process_type>;

        process_suback(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.version_ == protocol_version::v5) {
                    // properties
                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));
                }

                // suback reason codes
                yield ep_.process_nbytes(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(remain_buf),
                    ep_.remaining_length_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                {
                    LockGuard<Mutex> lck_store (ep_.store_mtx_);
                    LockGuard<Mutex> lck_sub_unsub (ep_.sub_unsub_inflight_mtx_);
                    ep_.pid_man_.release_id(packet_id_);
                    ep_.sub_unsub_inflight_.erase(packet_id_);
                }
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    {
                        // TODO: We can avoid an allocation by casting the raw bytes of the
                        // MQTT_NS::buffer that is being parsed, and instead call the suback
                        // handler with an std::span and the MQTT_NS::buffer (as lifekeeper)
                        std::vector<suback_return_code> results;
                        auto& body = variant_get<buffer>(var);
                        results.resize(body.size());
                        std::transform(
                            body.begin(),
                            body.end(),
                            results.begin(),
                            // http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html#_Toc442180880
                            // The SUBACK Packet sent by the Server to the Client MUST
                            // contain a return code for each Topic Filter/QoS pair.
                            // This return code MUST either show the maximum QoS that
                            // was granted for that Subscription or indicate that the
                            // subscription failed [MQTT-3.8.4-5].
                            [&](auto const& e) -> suback_return_code {
                                return static_cast<suback_return_code>(e);
                            }
                        );
                        if (ep_.on_suback(packet_id_, force_move(results))) {
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    }
                case protocol_version::v5:
                    {
                        // TODO: We can avoid an allocation by casting the raw bytes of the
                        // MQTT_NS::buffer that is being parsed, and instead call the suback
                        // handler with an std::span and the MQTT_NS::buffer (as lifekeeper)
                        std::vector<v5::suback_reason_code> reasons;
                        auto& body = variant_get<buffer>(var);
                        reasons.resize(body.size());
                        std::transform(
                            body.begin(),
                            body.end(),
                            reasons.begin(),
                            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901170
                            // The SUBACK packet sent by the Server to the Client MUST
                            // contain a Reason Code for each Topic Filter/Subscription
                            // Option pair [MQTT-3.8.4-6].
                            // This Reason Code MUST either show the maximum QoS that
                            // was granted for that Subscription or indicate that the
                            // subscription failed [MQTT-3.8.4-7].
                            [&](auto const& e) {
                                return static_cast<v5::suback_reason_code>(e);
                            }
                        );
                        if (ep_.on_v5_suback(packet_id_, force_move(reasons), force_move(props_))) {
                            ep_.on_mqtt_message_processed(
                                force_move(
                                    std::get<0>(
                                        any_cast<
                                            std::tuple<any, process_type_sp>
                                        >(session_life_keeper)
                                    )
                                )
                            );
                        }
                        break;
                    }
                default:
                    BOOST_ASSERT(false);
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::properties props_;
    };
    friend struct process_suback;

    // process unsubscribe

    struct process_unsubscribe : as::coroutine, std::enable_shared_from_this<process_unsubscribe> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_unsubscribe;
        using process_type_sp = std::shared_ptr<process_type>;

        process_unsubscribe(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.version_ == protocol_version::v5) {
                    // properties
                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));
                }

                while (true) {
                    // topic_filter including share_name
                    yield ep_.process_string(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    sn_tf_opt_ = parse_shared_subscription(variant_get<buffer>(var));
                    if (!sn_tf_opt_) {
                        MQTT_LOG("mqtt_impl", error)
                            << MQTT_ADD_VALUE(address, &ep_)
                            << "topic_filter parse error"
                            << " whole_topic_filter: "
                            << variant_get<buffer>(var);
                        ep_.disconnect_on_mqtt_error(v5::disconnect_reason_code::protocol_error);
                        ep_.call_protocol_error_handlers();
                        return;
                    }

                    entries_.emplace_back(
                        force_move(sn_tf_opt_.value().share_name),
                        force_move(sn_tf_opt_.value().topic_filter)
                    );

                    if (ep_.remaining_length_ == 0) {
                        switch (ep_.version_) {
                        case protocol_version::v3_1_1:
                            if (ep_.on_unsubscribe(packet_id_, force_move(entries_))) {
                                ep_.on_mqtt_message_processed(
                                    force_move(
                                        std::get<0>(
                                            any_cast<
                                                std::tuple<any, process_type_sp>
                                            >(session_life_keeper)
                                        )
                                    )
                                );
                            }
                            break;
                        case protocol_version::v5:
                            if (ep_.on_v5_unsubscribe(packet_id_, force_move(entries_), force_move(props_))) {
                                ep_.on_mqtt_message_processed(
                                    force_move(
                                        std::get<0>(
                                            any_cast<
                                                std::tuple<any, process_type_sp>
                                            >(session_life_keeper)
                                        )
                                    )
                                );
                            }
                            break;
                        default:
                            BOOST_ASSERT(false);
                        }
                        return;
                    }
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::properties props_;
        optional<share_name_topic_filter> sn_tf_opt_;
        std::vector<unsubscribe_entry> entries_;
    };
    friend struct process_unsubscribe;

    // process unsuback

    struct process_unsuback : as::coroutine, std::enable_shared_from_this<process_unsuback> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_unsuback;
        using process_type_sp = std::shared_ptr<process_type>;

        process_unsuback(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ < header_len_) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                // header
                yield ep_.process_header(
                    force_move(spep),
                    std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                    all_read_,
                    header_len_,
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );

                // packet_id
                yield ep_.process_packet_id(
                    force_move(spep),
                    force_move(session_life_keeper),
                    force_move(variant_get<buffer>(var)),
                    [this]
                    (auto&&... args ) {
                        (*this)(std::forward<decltype(args)>(args)...);
                    }
                );
                packet_id_ = force_move(variant_get<packet_id_t>(var));

                if (ep_.version_ == protocol_version::v5) {
                    // properties
                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));

                    // unsuback reason codes
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        ep_.remaining_length_,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    auto body = variant_get<buffer>(var);
                    reasons_.resize(body.size());
                    std::transform(
                        body.begin(),
                        body.end(),
                        reasons_.begin(),
                        [&](auto const& e) {
                            return static_cast<v5::unsuback_reason_code>(e);
                        }
                    );
                }
                {
                    LockGuard<Mutex> lck_store (ep_.store_mtx_);
                    LockGuard<Mutex> lck_sub_unsub (ep_.sub_unsub_inflight_mtx_);
                    ep_.pid_man_.release_id(packet_id_);
                    ep_.sub_unsub_inflight_.erase(packet_id_);
                }
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    if (ep_.on_unsuback(packet_id_)) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                case protocol_version::v5:
                    if (ep_.on_v5_unsuback(packet_id_, force_move(reasons_), force_move(props_))) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    break;
                default:
                    BOOST_ASSERT(false);
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        packet_id_t packet_id_;
        v5::properties props_;
        std::vector<v5::unsuback_reason_code> reasons_;
    };
    friend struct process_unsuback;

    // process pingreq

    void process_pingreq(
        any session_life_keeper
    ) {
        static constexpr std::size_t header_len = 0;

        if (remaining_length_ != header_len) {
            call_protocol_error_handlers();
            return;
        }
        if (on_pingreq()) {
            on_mqtt_message_processed(force_move(session_life_keeper));
        }
    }

    // process pingresp

    void process_pingresp(
        any session_life_keeper
    ) {
        static constexpr std::size_t header_len = 0;

        if (remaining_length_ != header_len) {
            call_protocol_error_handlers();
            return;
        }
        if (on_pingresp()) {
            on_mqtt_message_processed(force_move(session_life_keeper));
        }
        if (pingresp_timeout_ != std::chrono::steady_clock::duration::zero()) tim_pingresp_.cancel();
    }

    // process disconnect

    struct process_disconnect : as::coroutine, std::enable_shared_from_this<process_disconnect> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_disconnect;
        using process_type_sp = std::shared_ptr<process_type>;

        process_disconnect(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.remaining_length_ > 0) {
                    if (ep_.version_ != protocol_version::v5) {
                        ep_.call_protocol_error_handlers();
                        return;
                    }

                    // header
                    yield ep_.process_header(
                        force_move(spep),
                        std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                        all_read_,
                        header_len_,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );

                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(variant_get<buffer>(var)),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::disconnect_reason_code>(variant_get<buffer>(var)[0]);

                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901210
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used
                    if (ep_.remaining_length_ > 0) {
                        yield ep_.process_properties(
                            force_move(spep),
                            force_move(session_life_keeper),
                            force_move(remain_buf),
                            [this]
                            (auto&&... args ) {
                                (*this)(std::forward<decltype(args)>(args)...);
                            }
                        );
                        props_ = force_move(variant_get<v5::properties>(var));
                    }
                    switch (ep_.version_) {
                    case protocol_version::v3_1_1:
                        ep_.on_disconnect();
                        break;
                    case protocol_version::v5:
                        ep_.on_v5_disconnect(reason_code_, force_move(props_));
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                    MQTT_LOG("mqtt_impl", trace)
                        << MQTT_ADD_VALUE(address, this)
                        << "receive DISCONNECT call shutdown";
                    ep_.async_shutdown(*ep_.socket_, [](auto){});
                    ep_.on_mqtt_message_processed(
                        force_move(
                            std::get<0>(
                                any_cast<
                                    std::tuple<any, process_type_sp>
                                >(session_life_keeper)
                            )
                        )
                    );
                    return;
                }
                switch (ep_.version_) {
                case protocol_version::v3_1_1:
                    ep_.on_disconnect();
                    break;
                case protocol_version::v5:
                    ep_.on_v5_disconnect(reason_code_, force_move(props_));
                    break;
                default:
                    BOOST_ASSERT(false);
                }
                MQTT_LOG("mqtt_impl", trace)
                    << MQTT_ADD_VALUE(address, this)
                    << "receive DISCONNECT call shutdown";
                ep_.async_shutdown(*ep_.socket_, [](auto){});
                ep_.on_mqtt_message_processed(
                    force_move(
                        session_life_keeper
                    )
                );
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        ep_t& ep_;
        bool all_read_;

        v5::disconnect_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_disconnect;

    // process auth

    struct process_auth : as::coroutine, std::enable_shared_from_this<process_auth> {
        using ep_t = this_type;
        using ep_t_sp = this_type_sp;
        using process_type = process_auth;
        using process_type_sp = std::shared_ptr<process_type>;

        process_auth(
            ep_t& ep,
            bool all_read
        ):ep_{ep},
          all_read_{all_read} {
          }
        void operator()(
            ep_t_sp spep,
            any&& session_life_keeper,
            parse_handler_variant var = std::size_t(0),
            buffer remain_buf = buffer()
        ) {
            reenter(this) {
                if (ep_.version_ != protocol_version::v5) {
                    ep_.call_protocol_error_handlers();
                    return;
                }
                if (ep_.remaining_length_ > 0) {
                    // header
                    yield ep_.process_header(
                        force_move(spep),
                        std::make_tuple(force_move(session_life_keeper), this->shared_from_this()),
                        all_read_,
                        header_len_,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );

                    // reason_code
                    yield ep_.process_nbytes(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(variant_get<buffer>(var)),
                        1,
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    reason_code_ = static_cast<v5::auth_reason_code>(variant_get<buffer>(var)[0]);

                    yield ep_.process_properties(
                        force_move(spep),
                        force_move(session_life_keeper),
                        force_move(remain_buf),
                        [this]
                        (auto&&... args ) {
                            (*this)(std::forward<decltype(args)>(args)...);
                        }
                    );
                    props_ = force_move(variant_get<v5::properties>(var));
                    BOOST_ASSERT(ep_.version_ == protocol_version::v5);
                    if (ep_.on_v5_auth(reason_code_, force_move(props_))) {
                        ep_.on_mqtt_message_processed(
                            force_move(
                                std::get<0>(
                                    any_cast<
                                        std::tuple<any, process_type_sp>
                                    >(session_life_keeper)
                                )
                            )
                        );
                    }
                    return;
                }
                BOOST_ASSERT(ep_.version_ == protocol_version::v5);
                if (ep_.on_v5_auth(reason_code_, force_move(props_))) {
                    ep_.on_mqtt_message_processed(
                        force_move(
                            session_life_keeper
                        )
                    );
                }
            }
        }

    private:
        static constexpr std::size_t header_len_ = sizeof(packet_id_t);    // Packet Id

        this_type& ep_;
        bool all_read_;

        v5::auth_reason_code reason_code_;
        v5::properties props_;
    };
    friend struct process_auth;

    static constexpr std::uint16_t make_uint16_t(char b1, char b2) {
        return
            static_cast<std::uint16_t>(
                ((static_cast<std::uint16_t>(b1) & 0xff)) << 8 |
                (static_cast<std::uint16_t>(b2) & 0xff)
            );
    }

    void clean_sub_unsub_inflight() {
        LockGuard<Mutex> lck_store (store_mtx_);
        LockGuard<Mutex> lck_sub_unsub (sub_unsub_inflight_mtx_);
        auto it = sub_unsub_inflight_.begin();
        auto end = sub_unsub_inflight_.end();
        while (it != end) {
            pid_man_.release_id(*it);
            it = sub_unsub_inflight_.erase(it);
        }
    }

    void clean_sub_unsub_inflight_on_error(error_code ec) {
        clean_sub_unsub_inflight();
        on_error(ec);
    }

    void erase_publish_received(packet_id_t packet_id) {
        LockGuard<Mutex> lck (publish_received_mtx_);
        publish_received_.erase(packet_id);
    }

    static optional<topic_alias_t> get_topic_alias_from_prop(v5::property_variant const& prop) {
        optional<topic_alias_t> val;
        v5::visit_prop(
            prop,
            [&val](v5::property::topic_alias const& p) {
                val = p.val();
            },
            [](auto&&) {
            }
        );
        return val;
    }

    static optional<topic_alias_t> get_topic_alias_from_props(v5::properties const& props) {
        optional<topic_alias_t> val;
        v5::visit_props(
            props,
            [&val](v5::property::topic_alias const& p) {
                val = p.val();
            },
            [](auto&&) {
            }
        );
        return val;
    }

public:
    void set_preauthed_user_name(optional<std::string> const& user_name) {
        preauthed_user_name_ = user_name;
    }
    optional<std::string> get_preauthed_user_name() const {
        return preauthed_user_name_;
    }
private:
    optional<std::string> preauthed_user_name_;

protected:
    // Ensure that only code that knows the *exact* type of an object
    // inheriting from this abstract base class can destruct it.
    // This avoids issues of the destructor not triggering destruction
    // of derived classes, and any member variables contained in them.
    // Note: Not virtual to avoid need for a vtable when possible.
    ~core() {
        MQTT_LOG("mqtt_impl", trace)
            << MQTT_ADD_VALUE(address, this)
            << "core destroy";
    }

    Mutex const& get_store_mtx() const {
        return store_mtx_;
    }

    Mutex& get_store_mtx() {
        return store_mtx_;
    }

    store<PacketIdBytes> const& get_store() const {
        return store_;
    }

    store<PacketIdBytes>& get_store() {
        return store_;
    }

    packet_id_manager<packet_id_t> const& get_pid_man() const {
        return pid_man_;
    }

    packet_id_manager<packet_id_t>& get_pid_man() {
        return pid_man_;
    }

    Mutex const& get_sub_unsub_inflight_mtx() const {
        return sub_unsub_inflight_mtx_;
    }

    Mutex& get_sub_unsub_inflight_mtx() {
        return sub_unsub_inflight_mtx_;
    }

    std::set<packet_id_t>const & get_sub_unsub_inflight() const {
        return sub_unsub_inflight_;
    }

    std::set<packet_id_t>& get_sub_unsub_inflight() {
        return sub_unsub_inflight_;
    }

    bool shutdown_requested() const {
        return shutdown_requested_;
    }

    void set_pingresp_timer() {
        if (pingresp_timeout_ == std::chrono::steady_clock::duration::zero()) return;
        if (tim_pingresp_set_) return;
        tim_pingresp_set_ = true;
        tim_pingresp_.expires_after(pingresp_timeout_);
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
        tim_pingresp_.async_wait(
            [wp = force_move(wp)](error_code ec) mutable {
                if (auto sp = wp.lock()) {
                    sp->tim_pingresp_set_ = false;
                    if (!ec) {
                        sp->socket().post(
                            [sp = force_move(sp)] () mutable{
                                sp->async_force_disconnect(
                                    [sp = force_move(sp)]
                                    (error_code) {
                                    }
                                );
                            }
                        );
                    }
                }
            }
        );
    }

private:
    bool clean_start_{false};
    std::shared_ptr<MQTT_NS::socket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> mqtt_connected_{false};
    std::atomic<bool> shutdown_requested_{false};

    std::string client_id_;

    std::array<char, 10>  buf_;
    std::uint8_t fixed_header_;
    std::size_t remaining_length_multiplier_;
    std::size_t remaining_length_;
    std::vector<char> payload_;

    Mutex store_mtx_;
    store<PacketIdBytes> store_;
    std::set<packet_id_t> qos2_publish_handled_;

    packet_id_manager<packet_id_t> pid_man_;

    Mutex sub_unsub_inflight_mtx_;
    std::set<packet_id_t> sub_unsub_inflight_;
    bool auto_pub_response_{true};
    bool async_read_on_message_processed_ { true };
    bool disconnect_requested_{false};
    protocol_version version_{protocol_version::undetermined};
    std::size_t packet_bulk_read_limit_ = 256;
    std::size_t props_bulk_read_limit_ = packet_bulk_read_limit_;
    std::size_t total_bytes_received_ = 0;
    static constexpr std::uint8_t variable_length_continue_flag = 0b10000000;

    std::chrono::steady_clock::duration pingresp_timeout_ = std::chrono::steady_clock::duration::zero();
    as::steady_timer tim_pingresp_;
    bool tim_pingresp_set_ = false;

    as::steady_timer tim_shutdown_;

    mutable Mutex topic_alias_send_mtx_;
    optional<topic_alias_send> topic_alias_send_;

    mutable Mutex topic_alias_recv_mtx_;
    optional<topic_alias_recv> topic_alias_recv_;

    std::size_t maximum_packet_size_send_ = packet_size_no_limit;
    std::size_t maximum_packet_size_recv_ = packet_size_no_limit;

    std::atomic<receive_maximum_t> publish_send_count_{0};
    receive_maximum_t publish_send_max_ = receive_maximum_max;
    receive_maximum_t publish_recv_max_ = receive_maximum_max;
    Mutex publish_received_mtx_;
    std::set<packet_id_t> publish_received_;
    struct publish_send_queue_elem {
        publish_send_queue_elem(
            basic_message_variant<PacketIdBytes> message,
            bool async,
            any life_keeper = any()
        ): message{force_move(message)},
           life_keeper{force_move(life_keeper)},
           async{async}
        {}
        basic_message_variant<PacketIdBytes> message;
        any life_keeper;
        bool async;
    };
    Mutex publish_send_queue_mtx_;
    std::deque<publish_send_queue_elem> publish_send_queue_;

    mutable Mutex resend_pubrel_mtx_;
    std::set<packet_id_t> resend_pubrel_;
};

} // namespace MQTT_NS

#include <boost/asio/unyield.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__)

#endif // MQTT_CORE_HPP

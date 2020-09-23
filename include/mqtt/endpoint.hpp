// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_ENDPOINT_HPP)
#define MQTT_ENDPOINT_HPP

#include <mqtt/variant.hpp> // should be top to configure variant limit

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>

#include <boost/any.hpp>
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
#include <mqtt/topic_alias_recv.hpp>

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
#if __cplusplus >= 201703L // C++20 date is not determined yet
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

template <typename Mutex = std::mutex, template<typename...> class LockGuard = std::lock_guard, std::size_t PacketIdBytes = 2>
class endpoint : public std::enable_shared_from_this<endpoint<Mutex, LockGuard, PacketIdBytes>> {
    using this_type = endpoint<Mutex, LockGuard, PacketIdBytes>;
    using this_type_sp = std::shared_ptr<this_type>;

public:
    using async_handler_t = std::function<void(error_code ec)>;
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    /**
     * @brief Constructor for client
     */
    endpoint(as::io_context& ioc, protocol_version version = protocol_version::undetermined, bool async_send_store = false)
        :async_send_store_{async_send_store},
         version_(version),
         tim_pingresp_(ioc)
    {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "create"
            << " version:" << version
            << " async_send_store:" << std::boolalpha << async_send_store;
    }

    /**
     * @brief Constructor for server.
     *        socket should have already been connected with another endpoint.
     */
    template <typename Socket>
    explicit endpoint(as::io_context& ioc, std::shared_ptr<Socket> socket, protocol_version version = protocol_version::undetermined, bool async_send_store = false)
        :socket_(force_move(socket)),
         connected_(true),
         async_send_store_{async_send_store},
         version_(version),
         tim_pingresp_(ioc)
    {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "create"
            << " version:" << version
            << " async_send_store:" << std::boolalpha << async_send_store;
    }

    // MQTT Common handlers

private:
    /**
     * @brief Pingreq handler
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718086<BR>
     *        3.13 PINGREQ – PING request
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pingreq() noexcept = 0;

    /**
     * @brief Pingresp handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200<BR>
     *        3.13 PINGRESP – PING response
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pingresp() noexcept = 0;


    // MQTT v3_1_1 handlers

    /**
     * @brief Connect handler
     * @param client_id
     *        Client Identifier.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349245<BR>
     *        3.1.3.1 Client Identifier
     * @param user_name
     *        User Name.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349245<BR>
     *        3.1.3.4 User Name
     * @param password
     *        Password.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349246<BR>
     *        3.1.3.5 Password
     * @param will
     *        Will. It contains retain, QoS, topic, and message.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349232<BR>
     *        3.1.2.5 Will Flag<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349233<BR>
     *        3.1.2.6 Will QoS<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349234<BR>
     *        3.1.2.7 Will Retain<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349243<BR>
     *        3.1.3.2 Will Topic<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349244<BR>
     *        3.1.3.3 Will Message<BR>
     * @param clean_session
     *        Clean Session<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349231<BR>
     *        3.1.2.4 Clean Session
     * @param keep_alive
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349237<BR>
     *        3.1.2.10 Keep Alive
     * @return if the handler returns true, then continue receiving, otherwise quit.
     *
     */
    virtual bool on_connect(MQTT_NS::buffer client_id,
                            MQTT_NS::optional<MQTT_NS::buffer> user_name,
                            MQTT_NS::optional<MQTT_NS::buffer> password,
                            MQTT_NS::optional<will> will,
                            bool clean_session,
                            std::uint16_t keep_alive) noexcept = 0;

    /**
     * @brief Connack handler
     * @param session_present
     *        Session present flag.<BR>
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718036<BR>
     *        3.2.2.2 Session Present
     * @param return_code
     *        connect_return_code<BR>
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718036<BR>
     *        3.2.2.3 Connect Return code
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_connack(bool session_present, connect_return_code return_code) noexcept = 0;

    /**
     * @brief Publish handler
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is MQTT_NS::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718039<BR>
     *        3.3.2  Variable header
     * @param pubopts
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718038<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using MQTT_NS::publish functions.
     * @param topic_name
     *        Topic name
     * @param contents
     *        Published contents
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_publish(MQTT_NS::optional<packet_id_t> packet_id,
                            MQTT_NS::publish_options pubopts,
                            MQTT_NS::buffer topic_name,
                            MQTT_NS::buffer contents) noexcept = 0;

    /**
     * @brief Puback handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.4.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_puback(packet_id_t packet_id) noexcept = 0;

    /**
     * @brief Pubrec handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718050<BR>
     *        3.5.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubrec(packet_id_t packet_id) noexcept = 0;

    /**
     * @brief Pubrel handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349791<BR>
     *        3.6.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubrel(packet_id_t packet_id) noexcept = 0;

    /**
     * @brief Pubcomp handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718060<BR>
     *        3.7.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubcomp(packet_id_t packet_id) noexcept = 0;

    /**
     * @brief Subscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349801<BR>
     *        3.8.2 Variable header
     * @param entries
     *        Collection of a pair of Topic Filter and QoS.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349802<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_subscribe(packet_id_t packet_id,
                              std::vector<std::tuple<MQTT_NS::buffer, subscribe_options>> entries) noexcept = 0;

    /**
     * @brief Suback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718070<BR>
     *        3.9.2 Variable header
     * @param qoss
     *        Collection of QoS that is corresponding to subscribed topic order.<BR>
     *        If subscription is failure, the value is MQTT_NS::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718071<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_suback(packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> returns) noexcept = 0;

    /**
     * @brief Unsubscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349810<BR>
     *        3.10.2 Variable header
     * @param topics
     *        Collection of Topic Filters<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800448<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_unsubscribe(packet_id_t packet_id, std::vector<MQTT_NS::buffer> topics) noexcept = 0;

    /**
     * @brief Unsuback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.11.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_unsuback(packet_id_t) noexcept = 0;

    /**
     * @brief Disconnect handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800463<BR>
     *        3.14 DISCONNECT – Disconnect notification
     */
    virtual void on_disconnect() noexcept = 0;

    // MQTT v5 handlers

    /**
     * @brief Connect handler
     * @param client_id
     *        Client Identifier.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     *        3.1.3.1 Client Identifier
     * @param user_name
     *        User Name.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     *        3.1.3.4 User Name
     * @param password
     *        Password.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     *        3.1.3.5 Password
     * @param will
     *        Will. It contains retain, QoS, propertied, topic, and message.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901040<BR>
     *        3.1.2.5 Will Flag<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901041<BR>
     *        3.1.2.6 Will QoS<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901042<BR>
     *        3.1.2.7 Will Retain<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901060<BR>
     *        3.1.3.2 Will Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901069<BR>
     *        3.1.3.3 Will Topic<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901070<BR>
     *        3.1.3.3 Will Payload<BR>
     * @param clean_start
     *        Clean Start<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     *        3.1.2.4 Clean Session
     * @param keep_alive
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     *        3.1.2.10 Keep Alive
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     *
     */
    virtual bool on_v5_connect(MQTT_NS::buffer client_id,
                               MQTT_NS::optional<MQTT_NS::buffer> user_name,
                               MQTT_NS::optional<MQTT_NS::buffer> password,
                               MQTT_NS::optional<will> will,
                               bool clean_start,
                               std::uint16_t keep_alive,
                               v5::properties props) noexcept = 0;

    /**
     * @brief Connack handler
     * @param session_present
     *        Session present flag.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901078<BR>
     *        3.2.2.1.1 Session Present
     * @param reason_code
     *        Connect Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901079<BR>
     *        3.2.2.2 Connect Reason code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901080<BR>
     *        3.2.2.3 CONNACK Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_connack(bool session_present,
                               v5::connect_reason_code reason_code,
                               v5::properties props) noexcept = 0;

    /**
     * @brief Publish handler
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is MQTT_NS::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901108<BR>
     *        3.3.2.2 Packet Identifier
     * @param pubopts
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901101<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using MQTT_NS::publish functions.
     * @param topic_name
     *        Topic name<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901107<BR>
     *        3.3.2.1 Topic Name<BR>
     * @param contents
     *        Publish Payload<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901119<BR>
     *        3.3.3 PUBLISH Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_publish(MQTT_NS::optional<packet_id_t> packet_id,
                               MQTT_NS::publish_options pubopts,
                               MQTT_NS::buffer topic_name,
                               MQTT_NS::buffer contents,
                               v5::properties props) noexcept = 0;

    /**
     * @brief Puback handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901123<BR>
     *        3.4.2 Variable header
     * @param reason_code
     *        PUBACK Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124<BR>
     *        3.4.2.1 PUBACK Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901125<BR>
     *        3.4.2.2 PUBACK Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_puback(packet_id_t packet_id,
                              v5::puback_reason_code reason_code,
                              v5::properties props) noexcept = 0;

    /**
     * @brief Pubrec handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901133<BR>
     *        3.5.2 Variable header
     * @param reason_code
     *        PUBREC Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134<BR>
     *        3.5.2.1 PUBREC Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901135<BR>
     *        3.5.2.2 PUBREC Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_pubrec(packet_id_t packet_id,
                              v5::pubrec_reason_code reason_code,
                              v5::properties props) noexcept = 0;

    /**
     * @brief Pubrel handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901143<BR>
     *        3.6.2 Variable header
     * @param reason_code
     *        PUBREL Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144<BR>
     *        3.6.2.1 PUBREL Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901145<BR>
     *        3.6.2.2 PUBREL Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_pubrel(packet_id_t packet_id,
                              v5::pubrel_reason_code reason_code,
                              v5::properties props) noexcept = 0;

    /**
     * @brief Pubcomp handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901153<BR>
     *        3.7.2 Variable header
     * @param reason_code
     *        PUBCOMP Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154<BR>
     *        3.7.2.1 PUBCOMP Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901155<BR>
     *        3.7.2.2 PUBCOMP Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_pubcomp(packet_id_t packet_id,
                               v5::pubcomp_reason_code reason_code,
                               v5::properties props) noexcept = 0;

    /**
     * @brief Subscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901163<BR>
     *        3.8.2 Variable header
     * @param entries
     *        Collection of a pair of Topic Filter and QoS.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901168<BR>
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_subscribe(packet_id_t packet_id,
                                 std::vector<std::tuple<MQTT_NS::buffer, subscribe_options>> entries,
                                 v5::properties props) noexcept = 0;

    /**
     * @brief Suback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901173<BR>
     *        3.9.2 Variable header
     * @param reasons
     *        Collection of reason_code.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_suback(packet_id_t packet_id,
                              std::vector<MQTT_NS::v5::suback_reason_code> reasons,
                              v5::properties props) noexcept = 0;

    /**
     * @brief Unsubscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901181<BR>
     *        3.10.2 Variable header
     * @param topics
     *        Collection of Topic Filters<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901185<BR>
     *        3.10.3 UNSUBSCRIBE Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_unsubscribe(packet_id_t packet_id,
                                   std::vector<MQTT_NS::buffer> topics,
                                   v5::properties props) noexcept = 0;

    /**
     * @brief Unsuback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901189<BR>
     *        3.11.2 Variable header
     * @param reasons
     *        Collection of reason_code.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_unsuback(packet_id_t,
                                std::vector<v5::unsuback_reason_code> reasons,
                                v5::properties props) noexcept = 0;

    /**
     * @brief Disconnect handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     *        3.14 DISCONNECT – Disconnect notification
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     */
    virtual void on_v5_disconnect(v5::disconnect_reason_code reason_code,
                                  v5::properties props) noexcept = 0;

    /**
     * @brief Auth handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901217<BR>
     *        3.15 AUTH – Authentication exchange
     * @param reason_code
     *        AUTH Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220<BR>
     *        3.15.2.1 Authenticate Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901221<BR>
     *        3.15.2.2 AUTH Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_auth(v5::auth_reason_code reason_code,
                            v5::properties props) noexcept = 0;

    // Original handlers

protected:
    /**
     * @brief Close handler
     *
     * This handler is called if the client called `disconnect()` and the server closed the socket cleanly.
     * If the socket is closed by other reasons, error_handler is called.
     */
    virtual void on_close() noexcept = 0;

    /**
     * @brief Error handler
     *
     * This handler is called if the socket is closed without client's `disconnect()` call.
     *
     * @param ec error code
     */
    virtual void on_error(error_code ec) noexcept = 0;

private:
    /**
     * @brief Publish response sent handler
     *        This function is called just after puback sent on QoS1, or pubcomp sent on QoS2.
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901026<BR>
     *        2.2.1 Packet Identifier
     */
    virtual void on_pub_res_sent(packet_id_t packet_id) noexcept = 0;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg publish message
     */
    virtual void on_serialize_publish_message(basic_publish_message<sizeof(packet_id_t)> msg) noexcept = 0;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg v5::publish message
     */
    virtual void on_serialize_v5_publish_message(v5::basic_publish_message<sizeof(packet_id_t)> msg) noexcept = 0;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    virtual void on_serialize_pubrel_message(basic_pubrel_message<sizeof(packet_id_t)> msg) noexcept = 0;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    virtual void on_serialize_v5_pubrel_message(v5::basic_pubrel_message<sizeof(packet_id_t)> msg) noexcept = 0;

    /**
     * @brief Remove serialized message
     * @param packet_id packet identifier of the removing message
     */
    virtual void on_serialize_remove(packet_id_t packet_id) noexcept = 0;

protected:
    /**
     * @brief Pre-send handler
     *        This handler is called when any mqtt control packet is decided to send.
     */
    virtual void on_pre_send() noexcept = 0;

private:
    /**
     * @brief is valid length handler
     *        This handler is called when remaining length is received.
     * @param control_packet_type control_packet_type that has variable length
     * @param remaining length
     * @return true if check is success, otherwise false
     */
    virtual bool check_is_valid_length(control_packet_type packet_type, std::size_t remaining_length) noexcept = 0;

protected:
    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     * @param func A callback function that is called when async operation will finish.
     */
    MQTT_ALWAYS_INLINE virtual void on_mqtt_message_processed(MQTT_NS::any session_life_keeper) noexcept
    {
        if (async_read_on_message_processed_) {
            async_read_control_packet_type(force_move(session_life_keeper));
        }
    }

public:
    endpoint(this_type const&) = delete;
    endpoint(this_type&&) = delete;
    endpoint& operator=(this_type const&) = delete;
    endpoint& operator=(this_type&&) = delete;

    /**
     * @brief Get clean session.
     *
     * See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349231<BR>
     * 3.1.2.4 Clean Session<BR>
     * After constructing a endpoint, the clean session is set to false.
     * @return clean session
     */
    bool clean_session() const {
        return clean_session_;
    }

    /**
     * @brief Get clean start.
     *
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     * 3.1.2.4 Clean Start<BR>
     * After constructing a endpoint, the clean start is set to false.
     * @return clean start
     */
    bool clean_start() const {
        return clean_session();
    }

    /**
     * @brief get_total_bytes_received
     * @return The total bytes received on the socket.
     */
    std::size_t get_total_bytes_received() const {
        return total_bytes_received_;
    }

    /**
     * @brief get_total_bytes_sent
     * @return The total bytes sent on the socket.
     */
    std::size_t get_total_bytes_sent() const {
        return total_bytes_sent_;
    }

    /**
     * @brief Set auto publish response mode.
     * @param b set value
     * @param async auto publish ressponse send asynchronous
     *
     * When set auto publish response mode to true, puback, pubrec, pubrel,and pub comp automatically send.<BR>
     */
    void set_auto_pub_response(bool b = true, bool async = true) {
        auto_pub_response_ = b;
        auto_pub_response_async_ = async;
    }

    void set_packet_bulk_read_limit(std::size_t size) {
        packet_bulk_read_limit_ = size;
    }

    void set_props_bulk_read_limit(std::size_t size) {
        props_bulk_read_limit_ = size;
    }

    /**
     * @brief start session with a connected endpoint.
     * @param func finish handler that is called when the session is finished
     *
     */
    void start_session(any session_life_keeper = any()) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "start_session";
        async_read_control_packet_type(force_move(session_life_keeper));
    }

    // Blocking APIs

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props (optional)
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper (optional)
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value, packet_id_t >
    publish(T&& t, Params&&... params) {
        if(detail::should_generate_packet_id(params...)) {
            packet_id_t packet_id = acquire_unique_packet_id();
            publish(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
            return packet_id;
        }
        else {
            publish(0, std::forward<T>(t), std::forward<Params>(params)...);
            return 0;
        }
    }

    /**
     * @brief Subscribe
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value, packet_id_t >
    subscribe(T&& t, Params&&... params) {
        packet_id_t packet_id = acquire_unique_packet_id();
        subscribe(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value, packet_id_t >
    unsubscribe(T&& t, Params&&... params) {
        packet_id_t packet_id = acquire_unique_packet_id();
        unsubscribe(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
        return packet_id;
    }

    /**
     * @brief Disconnect
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901205<BR>
     * @param reason_code
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     */
    void disconnect(
        v5::disconnect_reason_code reason = v5::disconnect_reason_code::normal_disconnection,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "disconnect"
            << " reason:" << reason;

        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            send_disconnect(reason, force_move(props));
        }
    }

    /**
     * @brief Disconnect by endpoint
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    void force_disconnect() {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "force_disconnect";

        shutdown(socket());
    }

    /**
     * @brief Publish with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        publish_options pubopts = {},
        v5::properties props = {},
        any life_keeper = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        if (pubopts.get_qos() == qos::at_most_once) {
            // In the at_most_once case, we know a priori that send_publish won't track the lifetime.
            send_publish(packet_id,
                         as::buffer(topic_name),
                         as::buffer(contents),
                         pubopts,
                         force_move(props),
                         any{});
        }
        else {
            auto sp_topic_name = std::make_shared<std::string>(force_move(topic_name));
            auto sp_contents   = std::make_shared<std::string>(force_move(contents));
            auto topic_buf     = as::buffer(*sp_topic_name);
            auto contents_buf  = as::buffer(*sp_contents);

            send_publish(
                packet_id,
                topic_buf,
                contents_buf,
                pubopts,
                force_move(props),
                std::make_tuple(
                    force_move(life_keeper),
                    force_move(sp_topic_name),
                    force_move(sp_contents)
                )
            );
        }
    }

    /**
     * @brief Publish with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        publish_options pubopts,
        v5::properties props,
        any life_keeper
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "publish"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        send_publish(
            packet_id,
            topic_name,
            contents,
            pubopts,
            force_move(props),
            force_move(life_keeper)
        );
    }

    /**
     * @brief Publish with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        publish_options pubopts,
        any life_keeper
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "publish"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        send_publish(
            packet_id,
            topic_name,
            contents,
            pubopts,
            v5::properties{},
            force_move(life_keeper)
        );
    }

    /**
     * @brief Publish with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     *       If topic_name and contents don't manage their lifetimes, then life_keeper should be used to keep
     *       their lifetimes.
     */
    void publish(
        packet_id_t packet_id,
        buffer topic_name,
        buffer contents,
        publish_options pubopts = {},
        any life_keeper = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto topic_name_buf = as::buffer(topic_name);
        auto contents_buf   = as::buffer(contents);
        send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            v5::properties{},
            std::make_tuple(
                force_move(life_keeper),
                force_move(topic_name),
                force_move(contents)
            )
        );
    }

    /**
     * @brief Publish with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void publish(
        packet_id_t packet_id,
        buffer topic_name,
        buffer contents,
        publish_options pubopts,
        v5::properties props,
        any life_keeper = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto topic_name_buf = as::buffer(topic_name);
        auto contents_buf   = as::buffer(contents);
        send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            force_move(props),
            std::make_tuple(
                force_move(life_keeper),
                force_move(topic_name),
                force_move(contents)
            )
        );
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    void subscribe(
        packet_id_t packet_id,
        string_view topic_name,
        subscribe_options option,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "subscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { as::buffer(topic_name.data(), topic_name.size()), option } },
            packet_id,
            force_move(props)
        );
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    void subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        subscribe_options option,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "subscribe"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name, option } },
            packet_id,
            force_move(props)
        );
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params a vector of the topic_filter and qos pair.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    void subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<string_view, subscribe_options>> params,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> cb_params;
        cb_params.reserve(params.size());
        for (auto const& e : params) {
            cb_params.emplace_back(as::buffer(std::get<0>(e).data(), std::get<0>(e).size()), std::get<1>(e));
        }
        send_subscribe(force_move(cb_params), packet_id, force_move(props));
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params a vector of the topic_filter and qos pair.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    void subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<buffer, subscribe_options>> params,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> buffers;
        buffers.reserve(params.size());
        for (auto const& tup : params) {
            buffers.emplace_back(as::buffer(std::get<0>(tup)), std::get<1>(tup));
        }
        send_subscribe(force_move(buffers), packet_id, force_move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void unsubscribe(
        packet_id_t packet_id,
        string_view topic_name,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsubscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name;

        send_unsubscribe(std::vector<as::const_buffer>{ as::buffer(topic_name.data(), topic_name.size()) }, packet_id, force_move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void unsubscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsubscribe"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name));

        send_unsubscribe(std::vector<as::const_buffer>{ topic_name }, packet_id, force_move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params a collection of topic_filter
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void unsubscribe(
        packet_id_t packet_id,
        std::vector<string_view> params,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsubscribe"
            << " pid:" << packet_id;

            std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        for (auto&& e : params) {
            cb_params.emplace_back(as::buffer(e.data(), e.size()));
        }
        send_unsubscribe(force_move(cb_params), packet_id, force_move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params a collection of topic_filter
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsubscribe"
            << " pid:" << packet_id;

        std::vector<buffer> cb_params;
        cb_params.reserve(params.size());

        for (auto&& e : params) {
            cb_params.emplace_back(buffer(string_view(get_pointer(e), get_size(e))));
        }
        send_unsubscribe(params, packet_id, force_move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params a collection of topic_filter
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void unsubscribe(
        packet_id_t packet_id,
        std::vector<buffer> params,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsubscribe"
            << " pid:" << packet_id;

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        for (auto&& e : params) {
            cb_params.emplace_back(as::buffer(e));
        }
        send_unsubscribe(force_move(cb_params), packet_id, force_move(props));
    }

    /**
     * @brief Send pingreq packet.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901195
     */
    void pingreq() {
        // pingreq might frequently send, so SEV is set to trace
        MQTT_LOG("mqtt_api", trace)
            << MQTT_ADD_VALUE(address, this)
            << "pingreq";

        if (connected_ && mqtt_connected_) send_pingreq();
    }

    /**
     * @brief Send pingresp packet. This function is for broker.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200
     */
    void pingresp() {
        // pingresp might frequently send, so SEV is set to trace
        MQTT_LOG("mqtt_api", trace)
            << MQTT_ADD_VALUE(address, this)
            << "pingrsp";

        send_pingresp();
    }

    /**
     * @brief Send auth packet.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718086
     * @param reason_code
     *        AUTH Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220<BR>
     *        3.15.2.1 Authenticate Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901221<BR>
     *        3.15.2.2 AUTH Properties
     */
    void auth(
        v5::auth_reason_code reason_code = v5::auth_reason_code::success,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "auth"
            << " reason:" << reason_code;

        send_auth(reason_code, force_move(props));
    }

    /**
     * @brief Send connect packet.
     * @param client_id
     *        The client id to use for this connection<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     *        3.1.3.1 Client Identifier (ClientID)
     * @param user_name
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     *        3.1.3.5 User Name
     * @param password
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     *        3.1.3.6 Password
     * @param w
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc479576982<BR>
     *        3.1.2.5 Will Flag
     * @param clean_session
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     *        3.1.2.4 Clean Start<BR>
     * @param keep_alive_sec
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349238
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718028
     * @param keep_alive_sec
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     *        3.1.2.10 Keep Alive
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     */
    void connect(
        std::string const& client_id,
        optional<std::string> const& user_name,
        optional<std::string> const& password,
        optional<will> w,
        std::uint16_t keep_alive_sec,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "connect"
            << " client_id:" << client_id
            << " user_name:" << (user_name ? user_name.value() : "none")
            << " keep_alive:" << std::dec << keep_alive_sec;

        connect_requested_ = true;
        send_connect(
            buffer(string_view(client_id)),
            [&] {
                if (user_name) {
                    return buffer(string_view(user_name.value()));
                }
                else {
                    return buffer();
                }
            } (),
            [&] {
                if (password) {
                    return buffer(string_view(password.value()));
                }
                else {
                    return buffer();
                }
            } (),
            force_move(w),
            keep_alive_sec,
            force_move(props)
        );
    }

    /**
     * @brief Send connect packet.
     * @param client_id
     *        The client id to use for this connection<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     *        3.1.3.1 Client Identifier (ClientID)
     * @param user_name
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     *        3.1.3.5 User Name
     * @param password
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     *        3.1.3.6 Password
     * @param w
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc479576982<BR>
     *        3.1.2.5 Will Flag
     * @param clean_session
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901039<BR>
     *        3.1.2.4 Clean Start<BR>
     * @param keep_alive_sec
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349238
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718028
     * @param keep_alive_sec
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     *        3.1.2.10 Keep Alive
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     */
    void connect(
        buffer client_id,
        optional<buffer> user_name,
        optional<buffer> password,
        optional<will> w,
        std::uint16_t keep_alive_sec,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "connect"
            << " client_id:" << client_id
            << " user_name:" << (user_name ? string_view(user_name.value()) : string_view("none"))
            << " keep_alive:" << std::dec << keep_alive_sec;

        connect_requested_ = true;
        send_connect(
            force_move(client_id),
            force_move(user_name),
            force_move(password),
            force_move(w),
            keep_alive_sec,
            force_move(props)
        );
    }

    /**
     * @brief Send connack packet. This function is for broker.
     * @param session_present See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349255
     * @param reason_code See reason_code.hpp and https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349256
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901080<BR>
     *        3.2.2.3 CONNACK Properties
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718033
     */
    void connack(
        bool session_present,
        variant<connect_return_code, v5::connect_reason_code> reason_code,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "connack"
            << " session_present:" << std::boolalpha << session_present
            << " reason:" << reason_code;

            send_connack(session_present, reason_code, force_move(props));
    }

    /**
     * @brief Send puback packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBACK Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124<BR>
     *        3.4.2.1 PUBACK Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901125<BR>
     *        3.4.2.2 PUBACK Properties
     */
    void puback(
        packet_id_t packet_id,
        v5::puback_reason_code reason_code = v5::puback_reason_code::success,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "puback"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        send_puback(packet_id, reason_code, force_move(props));
    }

    /**
     * @brief Send  packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *         Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134<BR>
     *        3.5.2.1  Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901135<BR>
     *        3.5.2.2  Properties
     */
    void pubrec(
        packet_id_t packet_id,
        v5::pubrec_reason_code reason_code = v5::pubrec_reason_code::success,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "pubrec"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        send_pubrec(packet_id, reason_code, force_move(props));
    }

    /**
     * @brief Send pubrel packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBREL Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144<BR>
     *        3.6.2.1 PUBREL Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901145<BR>
     *        3.6.2.2 PUBREL Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If your props are not using built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note The library may store this message while it communicates with the server for several round trips.
     *       As such, the life_keeper paramter is important.
     */
    void pubrel(
        packet_id_t packet_id,
        v5::pubrel_reason_code reason_code = v5::pubrel_reason_code::success,
        v5::properties props = {},
        any life_keeper = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "pubrel"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        send_pubrel(packet_id, reason_code, force_move(props), force_move(life_keeper));
    }

    /**
     * @brief Send pubcomp packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBCOMP Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154<BR>
     *        3.7.2.1 PUBCOMP Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901155<BR>
     *        3.7.2.2 PUBCOMP Properties
     */
    void pubcomp(
        packet_id_t packet_id,
        v5::pubcomp_reason_code reason_code = v5::pubcomp_reason_code::success,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "pubcomp"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        send_pubcomp(packet_id, reason_code, force_move(props));
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     */
    void suback(
        packet_id_t packet_id,
        variant<suback_return_code, v5::suback_reason_code> reason,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "suback"
            << " pid:" << packet_id
            << " reason:" < reason;

        if (variant_idx(reason) == 0) {
            send_suback(std::vector<suback_return_code>{ variant_get<suback_return_code>(reason) }, packet_id, force_move(props));
        }
        else {
            send_suback(std::vector<v5::suback_reason_code>{ variant_get<v5::suback_reason_code>(reason) }, packet_id, force_move(props));
        }
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reasons
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     */
    void suback(
        packet_id_t packet_id,
        variant<std::vector<suback_return_code>, std::vector<v5::suback_reason_code>> reasons,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "suback"
            << " pid:" << packet_id;

        send_suback(force_move(reasons), packet_id, force_move(props));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     */
    void unsuback(
        packet_id_t packet_id
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsuback"
            << " pid:" << packet_id;

        send_unsuback(packet_id);
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     */
    void unsuback(
        packet_id_t packet_id,
        v5::unsuback_reason_code reason,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsuback"
            << " pid:" << packet_id
            << " reason:" << reason;

        send_unsuback(std::vector<v5::unsuback_reason_code>{ reason }, packet_id, force_move(props));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reasons
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     */
    void unsuback(
        packet_id_t packet_id,
        std::vector<v5::unsuback_reason_code> reasons,
        v5::properties props = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "unsuback"
            << " pid:" << packet_id;

        send_unsuback(force_move(reasons), packet_id, force_move(props));
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @return packet_id. If qos is set to at_most_once, return 0.
     *                    packet_id is automatically generated.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value >
    async_publish(T&& t, Params&&... params) {
        if(detail::should_generate_packet_id(params...)) {
            packet_id_t packet_id = acquire_unique_packet_id();
            async_publish(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
        }
        else {
            async_publish(0, std::forward<T>(t), std::forward<Params>(params)...);
        }
    }

    /**
     * @brief Disconnect
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     */
    void async_disconnect(
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_disconnect";

        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            // The reason code and property vector are only used if we're using mqttv5.
            async_send_disconnect(v5::disconnect_reason_code::normal_disconnection,
                                  v5::properties{},
                                  force_move(func));
        }
    }

    /**
     * @brief Disconnect
     * @param reason
     *        DISCONNECT Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901208<BR>
     *        3.14.2.1 Disconnect Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901209<BR>
     *        3.14.2.2 DISCONNECT Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * Send a disconnect packet to the connected broker. It is a clean disconnecting sequence.
     * The broker disconnects the endpoint after receives the disconnect packet.<BR>
     * When the endpoint disconnects using disconnect(), a will won't send.<BR>
     */
    void async_disconnect(
        v5::disconnect_reason_code reason,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_disconnect"
            << " reason:" << reason;

        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            async_send_disconnect(reason, force_move(props), force_move(func));
        }
    }

    // packet_id manual setting version

    /**
     * @brief Subscribe
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value >
    async_subscribe(T&& t, Params&&... params) {
        packet_id_t packet_id = acquire_unique_packet_id();
        async_subscribe(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename T, typename... Params>
    std::enable_if_t< ! std::is_convertible<std::decay_t<T>, packet_id_t>::value >
    async_unsubscribe(T&& t, Params&&... params) {
        packet_id_t packet_id = acquire_unique_packet_id();
        async_unsubscribe(packet_id, std::forward<T>(t), std::forward<Params>(params)...);
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        publish_options pubopts = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto sp_topic_name  = std::make_shared<std::string>(force_move(topic_name));
        auto sp_contents    = std::make_shared<std::string>(force_move(contents));
        auto topic_name_buf = as::buffer(*sp_topic_name);
        auto contents_buf   = as::buffer(*sp_contents);

        async_send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            v5::properties{},
            std::make_pair(force_move(sp_topic_name), force_move(sp_contents)),
            force_move(func)
        );
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        publish_options pubopts,
        v5::properties props,
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto sp_topic_name  = std::make_shared<std::string>(force_move(topic_name));
        auto sp_contents    = std::make_shared<std::string>(force_move(contents));
        auto topic_name_buf = as::buffer(*sp_topic_name);
        auto contents_buf   = as::buffer(*sp_contents);

        async_send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            force_move(props),
            std::make_tuple(
                force_move(life_keeper),
                force_move(sp_topic_name),
                force_move(sp_contents)
            ),
            force_move(func)
        );
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        publish_options pubopts = {},
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        async_send_publish(
            packet_id,
            topic_name,
            contents,
            pubopts,
            v5::properties{},
            force_move(life_keeper),
            force_move(func)
        );
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        publish_options pubopts,
        v5::properties props,
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        async_send_publish(
            packet_id,
            topic_name,
            contents,
            pubopts,
            force_move(props),
            force_move(life_keeper),
            force_move(func)
        );
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish(
        packet_id_t packet_id,
        buffer topic_name,
        buffer contents,
        publish_options pubopts = {},
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto topic_name_buf = as::buffer(topic_name);
        auto contents_buf   = as::buffer(contents);

        async_send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            v5::properties{},
            std::make_tuple(
                force_move(life_keeper),
                force_move(topic_name),
                force_move(contents)
            ),
            force_move(func)
        );
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param pubopts
     *        qos, retain flag, and dup flag.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *
     * @note If your QOS level is exactly_once or at_least_once, then the library will store this publish
     *       internally until the broker has confirmed delivery, which may involve resends, and as such the
     *       life_keeper parameter is important.
     */
    void async_publish(
        packet_id_t packet_id,
        buffer topic_name,
        buffer contents,
        publish_options pubopts,
        v5::properties props,
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_publish"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << pubopts.get_qos()
            << " retain:" << pubopts.get_retain()
            << " dup:" << pubopts.get_dup();

        BOOST_ASSERT((pubopts.get_qos() == qos::at_most_once && packet_id == 0) || (pubopts.get_qos() != qos::at_most_once && packet_id != 0));

        auto topic_name_buf = as::buffer(topic_name);
        auto contents_buf   = as::buffer(contents);

        async_send_publish(
            packet_id,
            topic_name_buf,
            contents_buf,
            pubopts,
            force_move(props),
            std::make_tuple(
                force_move(life_keeper),
                force_move(topic_name),
                force_move(contents)
            ),
            force_move(func)
        );
    }
    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param args
     *        The format of args is `[topic_name, option, topicname, option, ...,][props,][func]`<BR>
     *        args should be zero or more pairs of topic_name and option.
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        subscribe_options option,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        auto sp_topic_name  = std::make_shared<std::string>(force_move(topic_name));
        auto topic_name_buf = as::buffer(*sp_topic_name);

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name_buf, option } },
            packet_id,
            v5::properties{},
            [life_keeper = force_move(sp_topic_name), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        subscribe_options option,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        auto sp_topic_name  = std::make_shared<std::string>(force_move(topic_name));
        auto topic_name_buf = as::buffer(*sp_topic_name);

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name_buf, option } },
            packet_id,
            force_move(props),
            [life_keeper = force_move(sp_topic_name), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffers for topic_name.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        subscribe_options option,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name, option } },
            packet_id,
            v5::properties{},
            force_move(func)
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffers for topic_name, and properties.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        subscribe_options option,
        v5::properties props,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name))
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name, option } },
            packet_id,
            force_move(props),
            force_move(func)
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        buffer topic_name,
        subscribe_options option,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        auto topic_name_buf = as::buffer(topic_name);
        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name_buf, option } },
            packet_id,
            v5::properties{},
            [life_keeper = force_move(topic_name), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        buffer topic_name,
        subscribe_options option,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name
            << " qos:" << option.get_qos()
            << " rh:" << option.get_retain_handling()
            << " nl:" << option.get_nl()
            << " rap:" << option.get_rap();

        auto topic_name_buf = as::buffer(topic_name);
        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, subscribe_options>>{ { topic_name_buf, option } },
            packet_id,
            force_move(props),
            [life_keeper = force_move(topic_name), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, subscribe_options>> params,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto&& e : params) {
            auto sp_topic_name = std::make_shared<std::string>(force_move(std::get<0>(e)));
            cb_params.emplace_back(as::buffer(*sp_topic_name), std::get<1>(e));
            life_keepers.emplace_back(force_move(sp_topic_name));
        }

        async_send_subscribe(
            force_move(cb_params),
            packet_id,
            v5::properties{},
            [life_keeper = force_move(life_keepers), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, subscribe_options>> params,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto&& e : params) {
            auto sp_topic_name = std::make_shared<std::string>(force_move(std::get<0>(e)));
            cb_params.emplace_back(as::buffer(*sp_topic_name), std::get<1>(e));
            life_keepers.emplace_back(force_move(sp_topic_name));
        }
        async_send_subscribe(
            force_move(cb_params),
            packet_id,
            force_move(props),
            [life_keeper = force_move(life_keepers), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params A collection of the pair of topic_name and qos to subscribe.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffers for params.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        async_send_subscribe(
            force_move(params),
            packet_id,
            v5::properties{},
            force_move(func)
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffers for params.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        v5::properties props,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        async_send_subscribe(
            force_move(params),
            packet_id,
            force_move(props),
            force_move(func)
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params A collection of the pair of topic_name and qos to subscribe.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<buffer, subscribe_options>> params,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> cb_params;
        cb_params.reserve(params.size());

        for (auto&& e : params) {
            cb_params.emplace_back(
                as::buffer(std::get<0>(e)),
                std::get<1>(e)
            );
        }

        async_send_subscribe(
            force_move(cb_params),
            packet_id,
            v5::properties{},
            [life_keeper = force_move(params), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<buffer, subscribe_options>> params,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_subscribe"
            << " pid:" << packet_id;

        std::vector<std::tuple<as::const_buffer, subscribe_options>> cb_params;
        cb_params.reserve(params.size());

        for (auto&& e : params) {
            cb_params.emplace_back(
                as::buffer(std::get<0>(e)),
                std::get<1>(e)
            );
        }

        async_send_subscribe(
            force_move(cb_params),
            packet_id,
            force_move(props),
            [life_keeper = force_move(params), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::string topic_name,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name;

        auto sp_topic_name = std::make_shared<std::string>(force_move(topic_name));
        auto topic_name_buf = as::buffer(*sp_topic_name);
        async_send_unsubscribe(
            std::vector<as::const_buffer>{ topic_name_buf },
            packet_id,
            v5::properties{},
            [life_keeper = force_move(sp_topic_name), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffer for topic_name.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id
            << " topic:" << string_view(get_pointer(topic_name), get_size(topic_name));

        async_send_unsubscribe(std::vector<as::const_buffer>{ topic_name }, packet_id, v5::properties{}, force_move(func));
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        buffer topic_name,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name;

        auto topic_name_buf = as::buffer(topic_name);
        async_send_unsubscribe(std::vector<as::const_buffer>{ topic_name_buf },
                               packet_id,
                               v5::properties{},
                               [life_keeper = force_move(topic_name), func = force_move(func)]
                               (error_code ec) {
                                   if(func) func(ec);
                               });
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        buffer topic_name,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id
            << " topic:" << topic_name;

        auto topic_name_buf = as::buffer(topic_name);
        async_send_unsubscribe(std::vector<as::const_buffer>{ topic_name_buf },
                               packet_id,
                               force_move(props),
                               [life_keeper = force_move(topic_name), func = force_move(func)]
                               (error_code ec) {
                                   if(func) func(ec);
                               });
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> params,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto&& e : params) {
            life_keepers.emplace_back(std::make_shared<std::string>(force_move(e)));
            cb_params.emplace_back(as::buffer(*life_keepers.back()));
        }

        async_send_unsubscribe(
            force_move(cb_params),
            packet_id,
            v5::properties{},
            [life_keeper = force_move(life_keepers), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> params,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        // TOOD: Theoretically, std::vector's storage remains stationary
        // as long as it's never copied...
        // Perhaps a wrapper that disables copies, and forces move only?
        // that would allow us to avoid the std::shared_ptr<std::string> vector.
        // TODO: Does vector do short-buffer-optimization? If so, this is an invalid idea.
        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto&& e : params) {
            life_keepers.emplace_back(std::make_shared<std::string>(force_move(e)));
            cb_params.emplace_back(as::buffer(*life_keepers.back()));
        }

        async_send_unsubscribe(
            force_move(cb_params),
            packet_id,
            force_move(props),
            [life_keeper = force_move(life_keepers), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object may hold the lifetime of the buffers for topic_name and contents.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        async_send_unsubscribe(
            force_move(params),
            packet_id,
            v5::properties{},
            force_move(func)
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        This object should hold the lifetime of the buffers for params.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        v5::properties props,
        async_handler_t func
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        async_send_unsubscribe(
            force_move(params),
            packet_id,
            force_move(props),
            force_move(func)
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<buffer> params,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());
        for (auto const& buf : params) {
            cb_params.emplace_back(as::buffer(buf));
        }

        async_send_unsubscribe(
            force_move(cb_params),
            packet_id,
            v5::properties{},
            [life_keeper = force_move(params), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    void async_unsubscribe(
        packet_id_t packet_id,
        std::vector<buffer> params,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsubscribe"
            << " pid:" << packet_id;

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());
        for (auto const& buf : params) {
            cb_params.emplace_back(as::buffer(buf));
        }

        async_send_unsubscribe(
            force_move(cb_params),
            packet_id,
            force_move(props),
            [life_keeper = force_move(params), func = force_move(func)]
            (error_code ec) {
                if(func) func(ec);
            }
        );
    }

    /**
     * @brief Send pingreq packet.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901195
     */
    void async_pingreq(async_handler_t func = {}) {
        MQTT_LOG("mqtt_api", trace)
            << MQTT_ADD_VALUE(address, this)
            << "async_pingreq";

        if (connected_ && mqtt_connected_) async_send_pingreq(force_move(func));
    }

    /**
     * @brief Send pingresp packet. This function is for broker.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200
     */
    void async_pingresp(async_handler_t func = {}) {
        MQTT_LOG("mqtt_api", trace)
            << MQTT_ADD_VALUE(address, this)
            << "async_pingrsp";

        async_send_pingresp(force_move(func));
    }

    /**
     * @brief Send auth packet.
     * @param reason_code
     *        AUTH Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220<BR>
     *        3.15.2.1 Authenticate Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901221<BR>
     *        3.15.2.2 AUTH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718086
     */
    void async_auth(
        v5::auth_reason_code reason_code = v5::auth_reason_code::success,
        v5::properties props = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_auth"
            << " reason:" << reason_code;

        async_send_auth(reason_code, force_move(props), force_move(func));
    }

    /**
     * @brief Send connect packet.
     * @param client_id
     *        The client id to use for this connection<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     *        3.1.3.1 Client Identifier (ClientID)
     * @param user_name
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     *        3.1.3.5 User Name
     * @param password
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     *        3.1.3.6 Password
     * @param w
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc479576982<BR>
     *        3.1.2.5 Will Flag
     * @param keep_alive_sec
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     *        3.1.2.10 Keep Alive
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718028
     */
    void async_connect(
        buffer client_id,
        optional<buffer> user_name,
        optional<buffer> password,
        optional<will> w,
        std::uint16_t keep_alive_sec,
        async_handler_t func = {}
    ) {
        async_connect(
            force_move(client_id),
            force_move(user_name),
            force_move(password),
            force_move(w),
            keep_alive_sec,
            v5::properties{},
            force_move(func));
    }

    /**
     * @brief Send connect packet.
     * @param client_id
     *        The client id to use for this connection<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059<BR>
     *        3.1.3.1 Client Identifier (ClientID)
     * @param user_name
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901071<BR>
     *        3.1.3.5 User Name
     * @param password
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901072<BR>
     *        3.1.3.6 Password
     * @param w
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc479576982<BR>
     *        3.1.2.5 Will Flag
     * @param keep_alive_sec
     *        Keep Alive<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901045<BR>
     *        3.1.2.10 Keep Alive
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901046<BR>
     *        3.1.2.11 CONNECT Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718028
     */
    void async_connect(
        buffer client_id,
        optional<buffer> user_name,
        optional<buffer> password,
        optional<will> w,
        std::uint16_t keep_alive_sec,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_connect"
            << " client_id:" << client_id
            << " user_name:" << (user_name ? string_view(user_name.value()) : string_view("none"))
            << " keep_alive:" << std::dec << keep_alive_sec;

        connect_requested_ = true;
        async_send_connect(
            force_move(client_id),
            force_move(user_name),
            force_move(password),
            force_move(w),
            keep_alive_sec,
            force_move(props),
            force_move(func));
    }

    /**
     * @brief Send connack packet. This function is for broker.
     * @param session_present See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349255
     * @param return_code See connect_return_code.hpp and https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349256
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718033
     */
    void async_connack(
        bool session_present,
        variant<connect_return_code, v5::connect_reason_code> reason_code,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_connack"
            << " session_present:" << std::boolalpha << session_present
            << " reason:" << reason_code;

        async_send_connack(session_present, force_move(reason_code), v5::properties{}, force_move(func));
    }

    /**
     * @brief Send connack packet. This function is for broker.
     * @param session_present See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349255
     * @param return_code See connect_return_code.hpp and https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349256
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901080<BR>
     *        3.2.2.3 CONNACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718033
     */
    void async_connack(
        bool session_present,
        variant<connect_return_code, v5::connect_reason_code> reason_code,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_connack"
            << " session_present:" << std::boolalpha << session_present
            << " reason:" << reason_code;

        async_send_connack(session_present, force_move(reason_code), force_move(props), force_move(func));
    }

    /**
     * @brief Send puback packet.
     * @param packet_id packet id corresponding to publish
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_puback(
        packet_id_t packet_id,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_puback"
            << " pid:" << packet_id;

        async_send_puback(packet_id, v5::puback_reason_code::success, v5::properties{}, force_move(func));
    }

    /**
     * @brief Send puback packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBACK Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124<BR>
     *        3.4.2.1 PUBACK Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901125<BR>
     *        3.4.2.2 PUBACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_puback(
        packet_id_t packet_id,
        v5::puback_reason_code reason_code,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_puback"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        async_send_puback(packet_id, reason_code, force_move(props), force_move(func));
    }

    /**
     * @brief Send pubrec packet.
     * @param packet_id packet id corresponding to publish
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubrec(
        packet_id_t packet_id,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubrec"
            << " pid:" << packet_id;

        async_send_pubrec(packet_id, v5::pubrec_reason_code::success, v5::properties{}, force_move(func));
    }

    /**
     * @brief Send pubrec packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBREC Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134<BR>
     *        3.5.2.1 PUBREC Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901135<BR>
     *        3.5.2.2 PUBREC Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubrec(
        packet_id_t packet_id,
        v5::pubrec_reason_code reason_code,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubrec"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        async_send_pubrec(packet_id, reason_code, force_move(props), force_move(func));
    }

    /**
     * @brief Send pubrel packet.
     * @param packet_id packet id corresponding to publish
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubrel(
        packet_id_t packet_id,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubrel"
            << " pid:" << packet_id;

        async_send_pubrel(packet_id, v5::pubrel_reason_code::success, v5::properties{}, any(), force_move(func));
    }

    /**
     * @brief Send pubrel packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBREL Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144<BR>
     *        3.6.2.1 PUBREL Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901145<BR>
     *        3.6.2.2 PUBREL Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     *
     * @param life_keeper
     *        An object that stays alive as long as the library holds a reference to any other parameters.
     *        If topic_name, contents, or props do not have built-in lifetime management, (e.g. MQTT_NS::buffer)
     *        use this parameter to manage their lifetime.
     *
     * @note The library may store this message while it communicates with the server for several round trips.
     *       As such, the life_keeper paramter is important.
     */
    void async_pubrel(
        packet_id_t packet_id,
        v5::pubrel_reason_code reason_code,
        v5::properties props = {},
        any life_keeper = {},
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubrel"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        async_send_pubrel(packet_id, reason_code, force_move(props), force_move(life_keeper), force_move(func));
    }

    /**
     * @brief Send pubcomp packet.
     * @param packet_id packet id corresponding to publish
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubcomp(
        packet_id_t packet_id,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubcomp"
            << " pid:" << packet_id;

        async_send_pubcomp(packet_id, v5::pubcomp_reason_code::success, v5::properties{}, force_move(func));
    }

    /**
     * @brief Send pubcomp packet.
     * @param packet_id packet id corresponding to publish
     * @param reason_code
     *        PUBCOMP Reason Code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154<BR>
     *        3.7.2.1 PUBCOMP Reason Code
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901155<BR>
     *        3.7.2.2 PUBCOMP Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubcomp(
        packet_id_t packet_id,
        v5::pubcomp_reason_code reason_code,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_pubcomp"
            << " pid:" << packet_id
            << " reason:" << reason_code;

        async_send_pubcomp(packet_id, reason_code, force_move(props), force_move(func));
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_suback(
        packet_id_t packet_id,
        variant<suback_return_code, v5::suback_reason_code> reason,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_suback"
            << " pid:" << packet_id
            << " reason:" < reason;

        if (variant_idx(reason) == 0) {
            async_send_suback(std::vector<suback_return_code>{ variant_get<suback_return_code>(reason) }, packet_id, v5::properties{}, force_move(func));
        }
        else {
            async_send_suback(std::vector<v5::suback_reason_code>{ variant_get<v5::suback_reason_code>(reason) }, packet_id, v5::properties{}, force_move(func));
        }
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_suback(
        packet_id_t packet_id,
        variant<suback_return_code, v5::suback_reason_code> reason,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_suback"
            << " pid:" << packet_id
            << " reason:" < reason;

        if (variant_idx(reason) == 0) {
            async_send_suback(std::vector<suback_return_code>{ variant_get<suback_return_code>(reason) }, packet_id, force_move(props), force_move(func));
        }
        else {
            async_send_suback(std::vector<v5::suback_reason_code>{ variant_get<v5::suback_reason_code>(reason) }, packet_id, force_move(props), force_move(func));
        }
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_suback(
        packet_id_t packet_id,
        variant<std::vector<suback_return_code>, std::vector<v5::suback_reason_code>> reasons,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_suback"
            << " pid:" << packet_id;

        async_send_suback(force_move(reasons), packet_id, v5::properties{}, force_move(func));
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_suback(
        packet_id_t packet_id,
        variant<std::vector<suback_return_code>, std::vector<v5::suback_reason_code>> reasons,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_suback"
            << " pid:" << packet_id;

        async_send_suback(force_move(reasons), packet_id, force_move(props), force_move(func));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_unsuback(
        packet_id_t packet_id,
        v5::unsuback_reason_code reason,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsuback"
            << " pid:" << packet_id
            << " reason:" < reason;

        async_send_unsuback(std::vector<v5::unsuback_reason_code>{ reason }, packet_id, force_move(func));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_unsuback(
        packet_id_t packet_id,
        v5::unsuback_reason_code reason,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsuback"
            << " pid:" << packet_id
            << " reason:" < reason;

        async_send_unsuback(std::vector<v5::unsuback_reason_code>{ reason }, packet_id, force_move(props), force_move(func));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reasons
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_unsuback(
        packet_id_t packet_id,
        std::vector<v5::unsuback_reason_code> reasons,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsuback"
            << " pid:" << packet_id;

        async_send_unsuback(force_move(reasons), packet_id, v5::properties{}, force_move(func));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reasons
     *        a collection of reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    void async_unsuback(
        packet_id_t packet_id,
        std::vector<v5::unsuback_reason_code> reasons,
        v5::properties props,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsuback"
            << " pid:" << packet_id;

        async_send_unsuback(force_move(reasons), packet_id, force_move(props), force_move(func));
    }

    /**
     * @brief Send ununsuback packet. This function is for broker.
     * @param packet_id
     *        packet id corresponding to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718077
     */
    void async_unsuback(
        packet_id_t packet_id,
        async_handler_t func = {}
    ) {
        MQTT_LOG("mqtt_api", info)
            << MQTT_ADD_VALUE(address, this)
            << "async_unsuback"
            << " pid:" << packet_id;

        async_send_unsuback(packet_id, force_move(func));
    }

    /**
     * @brief Clear stored publish message that has packet_id.
     * @param packet_id packet id corresponding to stored publish
     */
    void clear_stored_publish(packet_id_t packet_id) {
        LockGuard<Mutex> lck (store_mtx_);
        auto& idx = store_.template get<tag_packet_id>();
        auto r = idx.equal_range(packet_id);
        idx.erase(std::get<0>(r), std::get<1>(r));
        packet_id_.erase(packet_id);
    }

    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(char const*, std::size_t)
     */
    void for_each_store(std::function<void(char const*, std::size_t)> const& f) {
        LockGuard<Mutex> lck (store_mtx_);
        auto const& idx = store_.template get<tag_seq>();
        for (auto const & e : idx) {
            auto const& m = e.message();
            auto cb = continuous_buffer(m);
            f(cb.data(), cb.size());
        }
    }

    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(message_variant)
     */
    void for_each_store(std::function<void(basic_message_variant<PacketIdBytes>)> const& f) {
        LockGuard<Mutex> lck (store_mtx_);
        auto const& idx = store_.template get<tag_seq>();
        for (auto const & e : idx) {
            f(e.message());
        }
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
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.size() == std::numeric_limits<packet_id_t>::max()) throw packet_id_exhausted_error();
        if (packet_id_master_ == std::numeric_limits<packet_id_t>::max()) {
            packet_id_master_ = 1U;
        }
        else {
            ++packet_id_master_;
        }
        auto ret = packet_id_.insert(packet_id_master_);
        if (ret.second) return packet_id_master_;

        auto last = packet_id_.end();
        auto e = last;
        --last;

        if (*last != std::numeric_limits<packet_id_t>::max()) {
            packet_id_master_ = static_cast<packet_id_t>(*last + 1U);
            packet_id_.insert(e, packet_id_master_);
            return packet_id_master_;
        }

        auto b = packet_id_.begin();
        auto prev = *b;
        if (prev != 1U) {
            packet_id_master_ = 1U;
            packet_id_.insert(b, packet_id_master_);
            return packet_id_master_;
        }
        ++b;
        while (*b - 1U == prev && b != e) {
            prev = *b;
            ++b;
        }
        packet_id_master_ = static_cast<packet_id_t>(prev + 1U);
        packet_id_.insert(b, packet_id_master_);
        return packet_id_master_;
    }

    /**
     * @brief Register packet_id to the library.
     *        After registering the packet_id, you can call acquired_* functions.
     *        The ownership of packet id is moved to the library.
     *        Or you can call release_packet_id to release it.
     * @return If packet_id is successfully registerd then return true, otherwise return false.
     */
    bool register_packet_id(packet_id_t packet_id) {
        if (packet_id == 0) return false;
        LockGuard<Mutex> lck (store_mtx_);
        return packet_id_.insert(packet_id).second;
    }

    /**
     * @brief Release packet_id.
     * @param packet_id packet id to release.
     *                   only the packet_id gotten by acquire_unique_packet_id, or
     *                   register_packet_id is permitted.
     * @return If packet_id is successfully released then return true, otherwise return false.
     */
    bool release_packet_id(packet_id_t packet_id) {
        LockGuard<Mutex> lck (store_mtx_);
        return packet_id_.erase(packet_id);
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
        static_assert(
            std::is_same<
                typename std::iterator_traits<Iterator>::iterator_category,
                std::random_access_iterator_tag
            >::value,
            "Iterators provided to restore_serialized_message() must be random access iterators."
        );

        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        switch (get_control_packet_type(fixed_header)) {
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
        auto packet_id = msg.packet_id();
        qos qos_value = msg.get_qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                ((qos_value == qos::at_least_once) ? control_packet_type::puback
                                                   : control_packet_type::pubrec),
                force_move(msg),
                force_move(life_keeper)
            );
            // When client want to restore serialized messages,
            // endpoint might keep the message that has the same packet_id.
            // In this case, overwrite store_.
            if (!ret.second) {
                store_.modify(
                    ret.first,
                    [&] (auto& e) {
                        e = store(
                            packet_id,
                            ((qos_value == qos::at_least_once) ? control_packet_type::puback
                                                               : control_packet_type::pubrec),
                            force_move(msg),
                            force_move(life_keeper)
                        );
                    }
                );
            }
        }
    }

    /**
     * @brief Restore serialized pubrel message.
     *        This function should be called before connect.
     * @param msg pubrel message.
     */
    void restore_serialized_message(basic_pubrel_message<PacketIdBytes> msg, any life_keeper = {}) {
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                control_packet_type::pubcomp,
                force_move(msg),
                force_move(life_keeper)
            );
            // When client want to restore serialized messages,
            // endpoint might keep the message that has the same packet_id.
            // In this case, overwrite store_.
            if (!ret.second) {
                store_.modify(
                    ret.first,
                    [&] (auto& e) {
                        e = store(
                            packet_id,
                            control_packet_type::pubcomp,
                            force_move(msg),
                            force_move(life_keeper)
                        );
                    }
                );
            }
        }
    }

    /**
     * @brief Restore serialized publish and pubrel messages.
     *        This function shouold be called before connect.
     * @param b         iterator begin of the message
     * @param e         iterator end of the message
     */
    template <typename Iterator>
    std::enable_if_t< std::is_convertible<typename Iterator::value_type, char>::value >
    restore_v5_serialized_message(Iterator b, Iterator e) {
        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        switch (get_control_packet_type(fixed_header)) {
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
            throw protocol_error();
            break;
        }
    }

    /**
     * @brief Restore serialized publish message.
     *        This function shouold be called before connect.
     * @param msg         publish message.
     * @param life_keeper
     *        An object that stays alive (but is moved with force_move()) until the stored message is sent.
     */
    void restore_v5_serialized_message(v5::basic_publish_message<PacketIdBytes> msg, any life_keeper = {}) {
        auto packet_id = msg.packet_id();
        auto qos = msg.get_qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                qos == qos::at_least_once ? control_packet_type::puback
                                          : control_packet_type::pubrec,
                force_move(msg),
                force_move(life_keeper)
            );
            // When client want to restore serialized messages,
            // endpoint might keep the message that has the same packet_id.
            // In this case, overwrite store_.
            if (!ret.second) {
                store_.modify(
                    ret.first,
                    [&] (auto& e) {
                        e = store(
                            packet_id,
                            qos == qos::at_least_once ? control_packet_type::puback
                                                      : control_packet_type::pubrec,
                            force_move(msg),
                            force_move(life_keeper)
                        );
                    }
                );
            }
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
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                control_packet_type::pubcomp,
                force_move(msg),
                force_move(life_keeper)
            );
            // When client want to restore serialized messages,
            // endpoint might keep the message that has the same packet_id.
            // In this case, overwrite store_.
            if (!ret.second) {
                store_.modify(
                    ret.first,
                    [&] (auto& e) {
                        e = store(
                            packet_id,
                            control_packet_type::pubcomp,
                            force_move(msg)
                        );
                    }
                );
            }
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
    void restore_serialized_message(basic_message_variant<PacketIdBytes> msg, any life_keeper = {}) {
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
     * @brief Trigger next mqtt message manually.
     *        If you call this function, you need to set manual receive mode
     *        using set_auto_next_read(false);
     */
    void async_read_next_message(any session_life_keeper) {
        async_read_control_packet_type(force_move(session_life_keeper));
    }

     /**
     * @brief Set maximum number of queued message sending.
     *        When async message sending function called during asynchronous
     *        processing, the message is enqueued. When current asynchronous
     *        message is processed, then concatenate queued messages and
     *        send it.
     *        This value limits the maximum number of concatenating messages.
     *        The default value is 1.
     *
     * @param count maximum number of queued message sending. 0 means infinity.
     *
     */
    void set_max_queue_send_count(std::size_t count) {
        max_queue_send_count_ = count;
    }

     /**
     * @brief Set maximum size of queued message sending.
     *        When async message sending function called during asynchronous
     *        processing, the message is enqueued. When current asynchronous
     *        message is processed, then concatenate queued messages and
     *        send it.
     *        This value limits the maximum size of concatenating messages.
     *        The default value is 0.
     *
     * @param size maximum size of queued message sending. 0 means infinity.
     *
     */
    void set_max_queue_send_size(std::size_t size) {
        max_queue_send_size_ = size;
    }

    protocol_version get_protocol_version() const {
        return version_;
    }

    MQTT_NS::socket const& socket() const {
        return socket_.value();
    }

    MQTT_NS::socket& socket() {
        return socket_.value();
    }

    auto get_executor() {
        return socket_.value().get_executor();
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
        pingresp_timeout_ = MQTT_NS::force_move(tim);
    }

    /**
     * @brief get topic alias recv container.
     * @return a copy of topic alias recv container
     *
     * This function for dump/restore topic alias recv container.
     */
    topic_alias_recv_map_t get_topic_alias_recv_container() const {
        LockGuard<Mutex> lck (topic_alias_recv_mtx_);
        return topic_alias_recv_;
    }

    /**
     * @brief restore topic alias recv container.
     * @param con topic alias recv container to restore
     *
     * This function for dump/restore topic alias recv container.
     */
    void restore_topic_alias_recv_container(topic_alias_recv_map_t con) {
        LockGuard<Mutex> lck (topic_alias_recv_mtx_);
        topic_alias_recv_ = MQTT_NS::force_move(con);
    }

protected:

    /**
     * @brief Get shared_any of socket
     * @return reference of shared_any socket
     */
    optional<MQTT_NS::socket>& socket_optional() {
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
        if (!ec) return false;
        if (connected_) {
            connected_ = false;
            mqtt_connected_ = false;
            {
                boost::system::error_code ignored_ec;
                socket_->close(ignored_ec);
            }
        }
        if (   (ec == as::error::eof)
            || (ec == as::error::connection_reset)
            || (ec == as::error::operation_aborted)
#if defined(MQTT_USE_WS)
            || (ec == boost::beast::websocket::error::closed)
#endif // defined(MQTT_USE_WS)
#if defined(MQTT_USE_TLS)
#if defined(SSL_R_SHORT_READ)
            || (ERR_GET_REASON(ec.value()) == SSL_R_SHORT_READ)
#else  // defined(SSL_R_SHORT_READ)
            || (ERR_GET_REASON(ec.value()) == boost::asio::ssl::error::stream_truncated)
#endif // defined(SSL_R_SHORT_READ)
#endif // defined(MQTT_USE_TLS)
        ) {
            if (disconnect_requested_) {
                disconnect_requested_ = false;
                connect_requested_ = false;
                clean_sub_unsub_inflight();
                on_close();
                return true;
            }
        }
        disconnect_requested_ = false;
        connect_requested_ = false;
        clean_sub_unsub_inflight_on_error(ec);
        return true;
    }

    void set_connect() {
        connected_ = true;
    }

    void set_protocol_version(protocol_version version) {
        version_ = version;
    }

    void clear_session_data() {
        {
            LockGuard<Mutex> lck (store_mtx_);
            store_.clear();
            packet_id_.clear();
        }
        {
            LockGuard<Mutex> lck (topic_alias_recv_mtx_);
            clear_topic_alias(topic_alias_recv_);
        }
    }

private:
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

    template <typename T>
    void shutdown(T& socket) {
        connected_ = false;
        mqtt_connected_ = false;

        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }

    class send_buffer {
    public:
        send_buffer():buf_(std::make_shared<std::string>(static_cast<int>(payload_position_), 0)) {}

        std::shared_ptr<std::string> const& buf() const {
            return buf_;
        }

        std::shared_ptr<std::string>& buf() {
            return buf_;
        }

        std::pair<char*, std::size_t> finalize(std::uint8_t fixed_header) {
            auto rb = remaining_bytes(buf_->size() - payload_position_);
            std::size_t start_position = payload_position_ - rb.size() - 1;
            (*buf_)[start_position] = fixed_header;
            buf_->replace(start_position + 1, rb.size(), rb);
            return std::make_pair(
                &(*buf_)[start_position],
                buf_->size() - start_position);
        }
    private:
        static constexpr std::size_t payload_position_ = 5;
        std::shared_ptr<std::string> buf_;
    };

    struct store {
        store(
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
        basic_message_variant<PacketIdBytes> message() const {
            return get_basic_message_variant<PacketIdBytes>(smv_);
        }
    private:
        packet_id_t packet_id_;
        control_packet_type expected_control_packet_type_;
        basic_store_message_variant<PacketIdBytes> smv_;
        any life_keeper_;
    };

    struct tag_packet_id {};
    struct tag_packet_id_type {};
    struct tag_seq {};
    using mi_store = mi::multi_index_container<
        store,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_packet_id_type>,
                mi::composite_key<
                    store,
                    mi::const_mem_fun<
                        store, packet_id_t,
                        &store::packet_id
                    >,
                    mi::const_mem_fun<
                        store, control_packet_type,
                        &store::expected_control_packet_type
                    >
                >
            >,
            mi::ordered_non_unique<
                mi::tag<tag_packet_id>,
                mi::const_mem_fun<
                    store, packet_id_t,
                    &store::packet_id
                >
            >,
            mi::sequenced<
                mi::tag<tag_seq>
            >
        >
    >;

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
            call_protocol_error_handlers();
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
            auto check =
                [&]() -> bool {
                    auto cpt = get_control_packet_type(fixed_header_);
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
            process_connect(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            break;
        case control_packet_type::connack:
            process_connack(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            break;
        case control_packet_type::publish:
            if (mqtt_connected_) {
                process_publish(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::puback:
            if (mqtt_connected_) {
                process_puback(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubrec:
            if (mqtt_connected_) {
                process_pubrec(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubrel:
            if (mqtt_connected_) {
                process_pubrel(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::pubcomp:
            if (mqtt_connected_) {
                process_pubcomp(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::subscribe:
            if (mqtt_connected_) {
                process_subscribe(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::suback:
            if (mqtt_connected_) {
                process_suback(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::unsubscribe:
            if (mqtt_connected_) {
                process_unsubscribe(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            }
            else {
                call_protocol_error_handlers();
            }
            break;
        case control_packet_type::unsuback:
            if (mqtt_connected_) {
                process_unsuback(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
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
            process_disconnect(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            break;
        case control_packet_type::auth:
            process_auth(force_move(session_life_keeper), remaining_length_ < packet_bulk_read_limit_, force_move(self));
            break;
        default:
            break;
        }
    }

    // primitive read functions
    void process_nbytes(
        any session_life_keeper,
        buffer buf,
        std::size_t size,
        std::function<void(buffer, buffer, any, this_type_sp)> handler,
        this_type_sp self
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
                        force_move(buf),
                        buffer(),
                        force_move(session_life_keeper),
                        force_move(self)
                    );
                }
            );
        }
        else {
            if (buf.size() < size) {
                call_protocol_error_handlers();
                return;
            }
            socket_->post(
                [
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    buf = force_move(buf),
                    size,
                    handler = force_move(handler)
                ]
                () mutable {
                    handler(
                        buf.substr(0, size),
                        buf.substr(size),
                        force_move(session_life_keeper),
                        force_move(self)
                    );
                }
            );
        }
    }

    template <std::size_t Bytes>
    void process_fixed_length(
        any session_life_keeper,
        buffer buf,
        std::function<void(std::size_t, buffer, any, this_type_sp)> handler,
        this_type_sp self
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
                        make_packet_id<Bytes>::apply(
                            buf_.data(),
                            std::next(buf_.data(), boost::numeric_cast<buffer::difference_type>(Bytes))
                        ),
                        buffer(),
                        force_move(session_life_keeper),
                        force_move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
               [
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    buf = force_move(buf),
                    handler = force_move(handler)
               ]
               () mutable {
                    auto packet_id =
                        make_packet_id<Bytes>::apply(
                            buf.data(),
                            std::next(buf.data(), boost::numeric_cast<buffer::difference_type>(Bytes))
                        );
                    buf.remove_prefix(Bytes);
                    handler(
                        packet_id,
                        force_move(buf),
                        force_move(session_life_keeper),
                        force_move(self)
                    );
               }
            );
        }
    }

    // This function isn't used for remaining lengh.
    void process_variable_length(
        any session_life_keeper,
        buffer buf,
        std::function<void(std::size_t, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_variable_length_impl(
            force_move(session_life_keeper),
            force_move(buf),
            force_move(handler),
            0,
            1,
            force_move(self)
        );
    }

    void process_variable_length_impl(
        any session_life_keeper,
        buffer buf,
        std::function<void(std::size_t, buffer, any, this_type_sp)> handler,
        std::size_t size,
        std::size_t multiplier,
        this_type_sp self
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
                any&& session_life_keeper,
                buffer&& buf,
                std::function<void(std::size_t, buffer, any, this_type_sp)>&& handler,
                std::size_t size,
                std::size_t multiplier,
                this_type_sp&& self
            ) mutable {
                if (!calc_variable_length(size, multiplier, buf.front())) {
                    call_protocol_error_handlers();
                    return;
                }
                if (buf.front() & variable_length_continue_flag) {
                    BOOST_ASSERT(!buf.empty());
                    buf.remove_prefix(1);
                    process_variable_length_impl(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(handler),
                        size,
                        multiplier,
                        force_move(self)
                    );
                }
                else {
                    buf.remove_prefix(1);
                    handler(
                        size,
                        force_move(buf),
                        force_move(session_life_keeper),
                        force_move(self)
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
                        force_move(session_life_keeper),
                        buffer(string_view(buf_.data(), 1)), // buf_'s lifetime is handled by `self`
                        force_move(handler),
                        size,
                        multiplier,
                        force_move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
                [
                    session_life_keeper = force_move(session_life_keeper),
                    handler = force_move(handler),
                    buf = force_move(buf),
                    size,
                    multiplier,
                    proc = force_move(proc),
                    self = force_move(self)
                ]
                () mutable {
                    proc(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(handler),
                        size,
                        multiplier,
                        force_move(self)
                    );
                }
            );
        }
    }

    void process_packet_id(
        any session_life_keeper,
        buffer buf,
        std::function<void(packet_id_t, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_fixed_length<sizeof(packet_id_t)>(
            force_move(session_life_keeper),
            force_move(buf),
            [
                handler = force_move(handler)
            ]
            (std::size_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                handler(static_cast<packet_id_t>(packet_id), force_move(buf), force_move(session_life_keeper), force_move(self));
            },
            force_move(self)
        );
    }

    void process_binary(
        any session_life_keeper,
        buffer buf,
        std::function<void(buffer, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {
        if (remaining_length_ < 2) {
            call_protocol_error_handlers();
            return;
        }
        process_fixed_length<2>(
            force_move(session_life_keeper),
            force_move(buf),
            [
                this,
                handler = force_move(handler)
            ]
            (std::size_t size,
             buffer buf,
             any session_life_keeper,
             this_type_sp self) mutable {
                if (remaining_length_ < size) {
                    call_protocol_error_handlers();
                    return;
                }
                process_nbytes(
                    force_move(session_life_keeper),
                    force_move(buf),
                    size,
                    force_move(handler),
                    force_move(self)
                );
            },
            force_move(self)
        );
    }

    void process_string(
        any session_life_keeper,
        buffer buf,
        std::function<void(buffer, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_binary(
            force_move(session_life_keeper),
            force_move(buf),
            [this, handler = force_move(handler)]
            (buffer str, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                auto r = utf8string::validate_contents(str);
                if (r != utf8string::validation::well_formed) {
                    call_protocol_error_handlers();
                    return;
                }
                handler(force_move(str), force_move(buf), force_move(session_life_keeper), force_move(self));
            },
            force_move(self)
        );
    }


    void process_properties(
        any session_life_keeper,
        buffer buf,
        std::function<void(v5::properties, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_variable_length(
            force_move(session_life_keeper),
            force_move(buf),
            [
                this,
                handler = force_move(handler)
            ]
            (std::size_t property_length, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                if (property_length > remaining_length_) {
                    call_protocol_error_handlers();
                    return;
                }
                if (property_length == 0) {
                    handler(v5::properties(), force_move(buf), force_move(session_life_keeper), force_move(self));
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
                            self = force_move(self),
                            session_life_keeper = force_move(session_life_keeper),
                            handler = force_move(handler),
                            property_length,
                            result
                        ]
                        (error_code ec, std::size_t bytes_transferred) mutable {
                            this->total_bytes_received_ += bytes_transferred;
                            if (!check_error_and_transferred_length(ec, bytes_transferred, result.len)) return;
                            process_property_id(
                                force_move(session_life_keeper),
                                buffer(string_view(result.address, result.len), result.spa),
                                property_length,
                                v5::properties(),
                                force_move(handler),
                                force_move(self)
                            );
                        }
                    );
                }
                else {
                    socket_->post(
                        [
                            this,
                            self = force_move(self),
                            session_life_keeper = force_move(session_life_keeper),
                            buf = force_move(buf),
                            handler = force_move(handler),
                            property_length
                        ]
                        () mutable {
                            process_property_id(
                                force_move(session_life_keeper),
                                force_move(buf),
                                property_length,
                                v5::properties(),
                                force_move(handler),
                                force_move(self)
                            );
                        }
                    );
                }
            },
            force_move(self)
        );
    }

    void process_property_id(
        any session_life_keeper,
        buffer buf,
        std::size_t property_length_rest,
        v5::properties props,
        std::function<void(v5::properties, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {

        if (property_length_rest == 0) {
            handler(force_move(props), force_move(buf), force_move(session_life_keeper), force_move(self));
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
                        force_move(session_life_keeper),
                        buffer(),
                        static_cast<v5::property::id>(buf_.front()),
                        property_length_rest - 1,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
                [
                    this,
                    self = force_move(self),
                    session_life_keeper = force_move(session_life_keeper),
                    buf = force_move(buf),
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                () mutable {
                    auto id = static_cast<v5::property::id>(buf.front());
                    buf.remove_prefix(1);
                    process_property_body(
                        force_move(session_life_keeper),
                        force_move(buf),
                        id,
                        property_length_rest - 1,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                }
            );
        }
    }

    void process_property_body(
        any session_life_keeper,
        buffer buf,
        v5::property::id id,
        std::size_t property_length_rest,
        v5::properties props,
        std::function<void(v5::properties, buffer, any, this_type_sp)> handler,
        this_type_sp self
    ) {

        static constexpr std::size_t length_bytes = 2;

        if (property_length_rest == 0) {
            call_protocol_error_handlers();
            return;
        }

        switch (id) {
        case v5::property::id::payload_format_indicator: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::payload_format_indicator(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::message_expiry_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::message_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::content_type: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::content_type(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::response_topic: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_topic(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::correlation_data: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::correlation_data(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::subscription_identifier: {
            process_variable_length(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest,
                    remaining_length_before = remaining_length_
                ]
                (std::size_t size, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto consumed = remaining_length_before - remaining_length_;
                    auto rest = property_length_rest - consumed;
                    props.emplace_back(
                        v5::property::subscription_identifier(size)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::session_expiry_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::session_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::assigned_client_identifier: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::assigned_client_identifier(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );

        } break;
        case v5::property::id::server_keep_alive: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::server_keep_alive(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::authentication_method: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_method(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::authentication_data: {
            process_binary(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_data(force_move(body))
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::request_problem_information: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::request_problem_information(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::will_delay_interval: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::will_delay_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::request_response_information: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::request_response_information(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::response_information: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_information(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::server_reference: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::server_reference(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::reason_string: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::reason_string(force_move(body), true)
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::receive_maximum: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::receive_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::topic_alias_maximum: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::topic_alias_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::topic_alias: {
            static constexpr std::size_t len = 2;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::topic_alias(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::maximum_qos: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::maximum_qos(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::retain_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::retain_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::user_property: {
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    property_length_rest
                ]
                (buffer key, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - key.size();
                    process_string(
                        force_move(session_life_keeper),
                        force_move(buf),
                        [
                            this,
                            props = force_move(props),
                            handler = force_move(handler),
                            key = force_move(key),
                            property_length_rest = rest
                        ]
                        (buffer val, buffer buf, any session_life_keeper, this_type_sp self) mutable {
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
                                force_move(session_life_keeper),
                                force_move(buf),
                                rest,
                                force_move(props),
                                force_move(handler),
                                force_move(self)
                            );
                        },
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::maximum_packet_size: {
            static constexpr std::size_t len = 4;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::maximum_packet_size(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::wildcard_subscription_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::wildcard_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::subscription_identifier_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::subscription_identifier_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        case v5::property::id::shared_subscription_available: {
            static constexpr std::size_t len = 1;
            if (property_length_rest < len) {
                call_protocol_error_handlers();
                return;
            }
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                len,
                [
                    this,
                    props = force_move(props),
                    handler = force_move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::shared_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        force_move(session_life_keeper),
                        force_move(buf),
                        rest,
                        force_move(props),
                        force_move(handler),
                        force_move(self)
                    );
                },
                force_move(self)
            );
        } break;
        }
    }

    // process common

    template <typename InfoType,
              void (this_type::*NextFunc)(any, buffer, InfoType&&, this_type_sp)>
    void process_header(
        any session_life_keeper,
        bool all_read,
        std::size_t header_len,
        InfoType&& info,
        this_type_sp self
    ) {

        if (all_read) {
            auto spa = make_shared_ptr_array(remaining_length_);
            auto ptr = spa.get();
            socket_->async_read(
                as::buffer(ptr, remaining_length_),
                [
                    this,
                    session_life_keeper = force_move(session_life_keeper),
                    buf = buffer(string_view(ptr, remaining_length_), force_move(spa)),
                    info = std::forward<InfoType>(info),
                    self = force_move(self)
                ]
                (error_code ec, std::size_t bytes_transferred) mutable {
                    this->total_bytes_received_ += bytes_transferred;
                    if (!check_error_and_transferred_length(ec, bytes_transferred, remaining_length_)) return;
                    (this->*NextFunc)(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                }
            );
            return;
        }

        if (header_len == 0) {
            (this->*NextFunc)(
                force_move(session_life_keeper),
                buffer(),
                force_move(info),
                force_move(self)
            );
            return;
        }

        socket_->async_read(
            as::buffer(buf_.data(), header_len),
            [
                this,
                session_life_keeper = force_move(session_life_keeper),
                header_len,
                info = std::forward<InfoType>(info),
                self = force_move(self)
            ]
            (error_code ec,
             std::size_t bytes_transferred) mutable {
                this->total_bytes_received_ += bytes_transferred;
                if (!check_error_and_transferred_length(ec, bytes_transferred, header_len)) return;
                (this->*NextFunc)(
                    force_move(session_life_keeper),
                    buffer(string_view(buf_.data(), header_len)),
                    force_move(info),
                    force_move(self)
                );
            }
        );
    }

    // process connect

    enum class connect_phase {
        header,
        properties,
        client_id,
        will,
        user_name,
        password,
        finish,
    };

    struct connect_info {
        std::size_t header_len;
        char connect_flag;
        std::uint16_t keep_alive;
        v5::properties props;
        buffer client_id;
        v5::properties will_props;
        buffer will_topic;
        buffer will_payload;
        optional<buffer> user_name;
        optional<buffer> password;
    };

    void process_connect(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            2 +  // string length
            4 +  // "MQTT" string
            1 +  // ProtocolVersion
            1 +  // ConnectFlag
            2;   // KeepAlive

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        connect_info info;
        info.header_len = header_len;

        process_header<connect_info,
                       &this_type::process_connect_impl<connect_phase::header>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            force_move(info),
            force_move(self)
        );
    }

    template<connect_phase Phase>
    void process_connect_impl(
        any session_life_keeper,
        buffer buf,
        connect_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case connect_phase::header: {
            static constexpr char protocol_name[] = { 0x00, 0x04, 'M', 'Q', 'T', 'T' };
            if (std::memcmp(buf.data(), protocol_name, sizeof(protocol_name)) != 0) {
                call_protocol_error_handlers();
                return;
            }
            std::size_t i = sizeof(protocol_name);
            auto version = static_cast<protocol_version>(buf[i++]);
            if (version != protocol_version::v3_1_1 && version != protocol_version::v5) {
                call_protocol_error_handlers();
                return;
            }

            if (version_ == protocol_version::undetermined) {
                version_ = version;
            }
            else if (version_ != version) {
                call_protocol_error_handlers();
                return;
            }

            info.connect_flag = buf[i++];

            info.keep_alive = make_uint16_t(buf[i], buf[i + 1]);
            clean_session_ = connect_flags::has_clean_session(info.connect_flag);

            buf.remove_prefix(info.header_len); // consume buffer
            if(version_ == protocol_version::v5) {
                process_connect_impl<connect_phase::properties>(
                    force_move(session_life_keeper),
                    force_move(buf),
                    force_move(info),
                    force_move(self)
                );
            }
            else {
                process_connect_impl<connect_phase::client_id>(
                    force_move(session_life_keeper),
                    force_move(buf),
                    force_move(info),
                    force_move(self)
                );
            }
        } break;
        case connect_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (
                    v5::properties props,
                    buffer buf,
                    any session_life_keeper,
                    this_type_sp self
                ) mutable {
                    info.props = force_move(props);
                    process_connect_impl<connect_phase::client_id>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case connect_phase::client_id:
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer client_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.client_id = force_move(client_id);
                    auto connect_flag = info.connect_flag;
                    if (connect_flags::has_will_flag(connect_flag)) {
                        process_connect_impl<connect_phase::will>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else if (connect_flags::has_user_name_flag(connect_flag)) {
                        process_connect_impl<connect_phase::user_name>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else if (connect_flags::has_password_flag(connect_flag)) {
                        process_connect_impl<connect_phase::password>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_connect_impl<connect_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case connect_phase::will: {
            // I use rvalue reference parameter to reduce move constructor calling.
            // This is a local lambda expression invoked from this function, so
            // I can control all callers.
            auto topic_message_proc =
                [this]
                (
                    any&& session_life_keeper,
                    buffer&& buf,
                    connect_info&& info,
                    this_type_sp&& self
                ) mutable {
                    process_string(
                        force_move(session_life_keeper),
                        force_move(buf),
                        [
                            this,
                            info = force_move(info)
                        ]
                        (buffer will_topic, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                            info.will_topic = force_move(will_topic);
                            process_binary(
                                force_move(session_life_keeper),
                                force_move(buf),
                                [
                                    this,
                                    info = force_move(info)
                                ]
                                (buffer will_payload, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                                    info.will_payload = force_move(will_payload);
                                    if (connect_flags::has_user_name_flag(info.connect_flag)) {
                                        process_connect_impl<connect_phase::user_name>(
                                            force_move(session_life_keeper),
                                            force_move(buf),
                                            force_move(info),
                                            force_move(self)
                                        );
                                    }
                                    else if (connect_flags::has_password_flag(info.connect_flag)) {
                                        process_connect_impl<connect_phase::password>(
                                            force_move(session_life_keeper),
                                            force_move(buf),
                                            force_move(info),
                                            force_move(self)
                                        );
                                    }
                                    else {
                                        process_connect_impl<connect_phase::finish>(
                                            force_move(session_life_keeper),
                                            force_move(buf),
                                            force_move(info),
                                            force_move(self)
                                        );
                                    }
                                },
                                force_move(self)
                            );
                        },
                        force_move(self)
                    );
                };

            if (version_ == protocol_version::v5) {
                process_properties(
                    force_move(session_life_keeper),
                    force_move(buf),
                    [
                        info = force_move(info),
                        topic_message_proc
                    ]
                    (
                         v5::properties will_props,
                         buffer buf,
                         any session_life_keeper,
                         this_type_sp self
                    ) mutable {
                        info.will_props = force_move(will_props);
                        topic_message_proc(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    },
                    force_move(self)
                );
                return;
            }
            topic_message_proc(
                force_move(session_life_keeper),
                force_move(buf),
                force_move(info),
                force_move(self)
            );
        } break;
        case connect_phase::user_name:
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer user_name, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.user_name = force_move(user_name);
                    if (connect_flags::has_password_flag(info.connect_flag)) {
                        process_connect_impl<connect_phase::password>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_connect_impl<connect_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case connect_phase::password:
            process_binary(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer password, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.password = force_move(password);
                    process_connect_impl<connect_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case connect_phase::finish:
            mqtt_connected_ = true;
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_connect(
                        force_move(info.client_id),
                        force_move(info.user_name),
                        force_move(info.password),
                          connect_flags::has_will_flag(info.connect_flag)
                        ? optional<will>(in_place_init,
                                         force_move(info.will_topic),
                                         force_move(info.will_payload),
                                         connect_flags::has_will_retain(info.connect_flag) | connect_flags::will_qos(info.connect_flag))
                        : optional<will>(nullopt),
                        clean_session_,
                        info.keep_alive
                    )
                ) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_connect(
                        force_move(info.client_id),
                        force_move(info.user_name),
                        force_move(info.password),
                          connect_flags::has_will_flag(info.connect_flag)
                        ? optional<will>(in_place_init,
                                         force_move(info.will_topic),
                                         force_move(info.will_payload),
                                         connect_flags::has_will_retain(info.connect_flag) | connect_flags::will_qos(info.connect_flag),
                                         force_move(info.will_props))
                        : optional<will>(nullopt),
                        clean_session_,
                        info.keep_alive,
                        force_move(info.props)
                    )
                ) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
            break;
        }
    }

    // process connack

    enum class connack_phase {
        header,
        properties,
        finish,
    };

    struct connack_info {
        std::size_t header_len;
        bool session_present;
        variant<connect_return_code, v5::connect_reason_code> reason_code;
        v5::properties props;
    };

    void process_connack(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            1 +  // Connect Acknowledge Flags
            1;   // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        connack_info info;
        info.header_len = header_len;

        process_header<connack_info,
                       &this_type::process_connack_impl<connack_phase::header>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            force_move(info),
            force_move(self)
        );
    }

    template<connack_phase Phase>
    void process_connack_impl(
        any session_life_keeper,
        buffer buf,
        connack_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case connack_phase::header:
            info.session_present = is_session_present(buf[0]);
            switch (version_) {
            case protocol_version::v3_1_1:
                info.reason_code = static_cast<connect_return_code>(buf[1]);
                break;
            case protocol_version::v5:
                info.reason_code = static_cast<v5::connect_reason_code>(buf[1]);
                break;
            default:
                BOOST_ASSERT(false);
            }

            buf.remove_prefix(info.header_len); // consume buffer
            if (version_ == protocol_version::v5) {
                process_connack_impl<connack_phase::properties>(
                    force_move(session_life_keeper),
                    force_move(buf),
                    force_move(info),
                    force_move(self)
                );
            }
            else {
                process_connack_impl<connack_phase::finish>(
                    force_move(session_life_keeper),
                    force_move(buf),
                    force_move(info),
                    force_move(self)
                );
            }
            break;
        case connack_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (
                    v5::properties props,
                    buffer buf,
                    any session_life_keeper,
                    this_type_sp self
                ) mutable {
                    info.props = force_move(props);
                    process_connack_impl<connack_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case connack_phase::finish: {
            mqtt_connected_ = true;
            // I use rvalue reference parameter to reduce move constructor calling.
            // This is a local lambda expression invoked from this function, so
            // I can control all callers.
            auto connack_proc =
                [this]
                (
                    any&& session_life_keeper,
                    connack_info&& info
                ) mutable {
                    switch (version_) {
                    case protocol_version::v3_1_1:
                        if(on_connack(info.session_present,
                                      variant_get<connect_return_code>(info.reason_code))) {
                            on_mqtt_message_processed(force_move(session_life_keeper));
                        }
                        break;
                    case protocol_version::v5:
                        if (on_v5_connack(info.session_present,
                                          variant_get<v5::connect_reason_code>(info.reason_code),
                                          force_move(info.props))) {
                            on_mqtt_message_processed(force_move(session_life_keeper));
                        }
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                };

            // Note: boost:variant has no featue to query if the variant currently holds a specific type.
            // MQTT_CPP could create a type traits function to match the provided type to the index in
            // the boost::variant type list, but for now it does not appear to be needed.
            if (   (   (0 == variant_idx(info.reason_code))
                    && (connect_return_code::accepted == variant_get<connect_return_code>(info.reason_code)))
                || (   (1 == variant_idx(info.reason_code))
                    && (v5::connect_reason_code::success == variant_get<v5::connect_reason_code>(info.reason_code)))) {
                if (clean_session_) {
                    clear_session_data();
                }
                else {
                    if (async_send_store_) {
                        // Until all stored messages are written to internal send buffer,
                        // disable further async reading of incoming packets..
                        async_read_on_message_processed_ = false;
                        auto async_connack_proc =
                            [
                                this,
                                self = force_move(self),
                                session_life_keeper = force_move(session_life_keeper),
                                connack_proc = force_move(connack_proc),
                                info = force_move(info)
                            ]
                            () mutable {
                                // All stored messages are sent, so re-enable reading of incoming packets.
                                // and notify the end user code that the connack packet was received.
                                async_read_on_message_processed_ = true;
                                connack_proc(force_move(session_life_keeper), force_move(info));
                            };
                        async_send_store(force_move(async_connack_proc));
                        return;
                    }
                    send_store();
                }
            }
            connack_proc(force_move(session_life_keeper), force_move(info));
        } break;
        }
    }

    // process publish

    enum class publish_phase {
        topic_name,
        packet_id,
        properties,
        payload,
    };

    struct publish_info {
        buffer topic_name;
        optional<packet_id_t> packet_id;
        v5::properties props;
    };

    void process_publish(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t min_len =
            2; // topic name length

        if (remaining_length_ < min_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<publish_info,
                       &this_type::process_publish_impl<publish_phase::topic_name>>(
            force_move(session_life_keeper),
            all_read,
            0,
            publish_info(),
            force_move(self)
        );
    }

    template<publish_phase Phase>
    void process_publish_impl(
        any session_life_keeper,
        buffer buf,
        publish_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case publish_phase::topic_name:
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer topic_name, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.topic_name = force_move(topic_name);
                    qos qos_value = publish::get_qos(fixed_header_);
                    if (qos_value != qos::at_most_once &&
                        qos_value != qos::at_least_once &&
                        qos_value != qos::exactly_once) {
                        call_protocol_error_handlers();
                        return;
                    }
                     if(qos_value == qos::at_most_once) {
                         if(version_ == protocol_version::v5) {
                             process_publish_impl<publish_phase::properties>(
                                 force_move(session_life_keeper),
                                 force_move(buf),
                                 force_move(info),
                                 force_move(self)
                             );
                         }
                         else {
                             process_publish_impl<publish_phase::payload>(
                                 force_move(session_life_keeper),
                                 force_move(buf),
                                 force_move(info),
                                 force_move(self)
                             );
                         }
                     }
                     else {
                         process_publish_impl<publish_phase::packet_id>(
                             force_move(session_life_keeper),
                             force_move(buf),
                             force_move(info),
                             force_move(self)
                         );
                     }
                },
                force_move(self)
            );
            break;
        case publish_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    if (version_ == protocol_version::v5) {
                        process_publish_impl<publish_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_publish_impl<publish_phase::payload>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case publish_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_publish_impl<publish_phase::payload>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case publish_phase::payload:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                remaining_length_,
                [
                    this,
                    info = force_move(info)
                ]
                (buffer payload, buffer /*buf*/, any session_life_keeper, this_type_sp /*self*/) mutable {
                    auto handler_call =
                        [&] {
                            switch (version_) {
                            case protocol_version::v3_1_1:
                                if (on_publish(
                                            info.packet_id,
                                            publish_options(fixed_header_),
                                            force_move(info.topic_name),
                                            force_move(payload))) {
                                    on_mqtt_message_processed(force_move(session_life_keeper));
                                    return true;
                                }
                                break;
                            case protocol_version::v5:
                                if (info.topic_name.empty()) {
                                    if (auto topic_alias = get_topic_alias_by_props(info.props)) {
                                        auto topic_name = [&] {
                                            LockGuard<Mutex> lck (topic_alias_recv_mtx_);
                                            return find_topic_by_alias(topic_alias_recv_, topic_alias.value());
                                        }();
                                        if (topic_name.empty()) {
                                            MQTT_LOG("mqtt_cb", error)
                                                << MQTT_ADD_VALUE(address, this)
                                                << "no matching topic alias: "
                                                << topic_alias.value();
                                            call_protocol_error_handlers();
                                            return false;
                                        }
                                        else {
                                            info.topic_name = allocate_buffer(topic_name);
                                        }
                                    }
                                }
                                else {
                                    if (auto topic_alias = get_topic_alias_by_props(info.props)) {
                                        LockGuard<Mutex> lck (topic_alias_recv_mtx_);
                                        register_topic_alias(topic_alias_recv_, info.topic_name, topic_alias.value());
                                    }
                                }
                                if (on_v5_publish(
                                            info.packet_id,
                                            publish_options(fixed_header_),
                                            force_move(info.topic_name),
                                            force_move(payload),
                                            force_move(info.props)
                                    )
                                ) {
                                    on_mqtt_message_processed(force_move(session_life_keeper));
                                    return true;
                                }
                                break;
                            default:
                                BOOST_ASSERT(false);
                            }
                            return false;
                        };
                    switch (publish::get_qos(fixed_header_)) {
                    case qos::at_most_once:
                        handler_call();
                        break;
                    case qos::at_least_once:
                        if (handler_call()) {
                            auto_pub_response(
                                [this, &info] {
                                    if (connected_) {
                                        send_puback(*info.packet_id,
                                                    v5::puback_reason_code::success,
                                                    v5::properties{});
                                    }
                                },
                                [this, &info, &session_life_keeper] {
                                    if (connected_) {
                                        async_send_puback(
                                            *info.packet_id,
                                            v5::puback_reason_code::success,
                                            v5::properties{},
                                            [session_life_keeper](auto){}
                                        );
                                    }
                                }
                            );
                        }
                        break;
                    case qos::exactly_once:
                        if (handler_call()) {
                            qos2_publish_handled_.emplace(*info.packet_id);
                            auto_pub_response(
                                [this, &info] {
                                    if (connected_) {
                                        send_pubrec(*info.packet_id,
                                                    v5::pubrec_reason_code::success,
                                                    v5::properties{});
                                    }
                                },
                                [this, &info, &session_life_keeper] {
                                    if (connected_) {
                                        async_send_pubrec(
                                            *info.packet_id,
                                            v5::pubrec_reason_code::success,
                                            v5::properties{},
                                            [session_life_keeper](auto){}
                                        );
                                    }
                                }
                            );
                        }
                        break;
                    }
                },
                force_move(self)
            );
            break;
        }
    }

    // process puback

    enum class puback_phase {
        packet_id,
        reason_code,
        properties,
        finish,
    };

    struct puback_info {
        packet_id_t packet_id;
        v5::puback_reason_code reason_code;
        v5::properties props;
    };

    void process_puback(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<puback_info,
                       &this_type::process_puback_impl<puback_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            puback_info(),
            force_move(self)
        );
    }

    template<puback_phase Phase>
    void process_puback_impl(
        any session_life_keeper,
        buffer buf,
        puback_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case puback_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    if (remaining_length_ == 0) {
                        info.reason_code = v5::puback_reason_code::success;
                        process_puback_impl<puback_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_puback_impl<puback_phase::reason_code>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case puback_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::puback_reason_code>(body[0]);
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used
                    if (remaining_length_ == 0) {
                        process_puback_impl<puback_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_puback_impl<puback_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case puback_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_puback_impl<puback_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case puback_phase::finish:
            {
                LockGuard<Mutex> lck (store_mtx_);
                auto& idx = store_.template get<tag_packet_id_type>();
                auto r = idx.equal_range(std::make_tuple(info.packet_id, control_packet_type::puback));
                idx.erase(std::get<0>(r), std::get<1>(r));
                packet_id_.erase(info.packet_id);
            }
            on_serialize_remove(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_puback(info.packet_id)) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_puback(info.packet_id, info.reason_code, force_move(info.props))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
            break;
        }
    }

    // process pubrec

    enum class pubrec_phase {
        packet_id,
        reason_code,
        properties,
        finish,
    };

    struct pubrec_info {
        packet_id_t packet_id;
        v5::pubrec_reason_code reason_code;
        v5::properties props;
    };

    void process_pubrec(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<pubrec_info,
                       &this_type::process_pubrec_impl<pubrec_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            pubrec_info(),
            force_move(self)
        );
    }

    template<pubrec_phase Phase>
    void process_pubrec_impl(
        any session_life_keeper,
        buffer buf,
        pubrec_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case pubrec_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    if(remaining_length_ == 0) {
                        info.reason_code = v5::pubrec_reason_code::success;
                        process_pubrec_impl<pubrec_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubrec_impl<pubrec_phase::reason_code>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubrec_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::pubrec_reason_code>(body[0]);
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used
                    if(remaining_length_ == 0) {
                        process_pubrec_impl<pubrec_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubrec_impl<pubrec_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubrec_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_pubrec_impl<pubrec_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case pubrec_phase::finish: {
            {
                LockGuard<Mutex> lck (store_mtx_);
                auto& idx = store_.template get<tag_packet_id_type>();
                auto r = idx.equal_range(std::make_tuple(info.packet_id, control_packet_type::pubrec));
                idx.erase(std::get<0>(r), std::get<1>(r));
                // packet_id shouldn't be erased here.
                // It is reused for pubrel/pubcomp.
            }
            auto res =
                [&] {
                    auto_pub_response(
                        [&] {
                            if (connected_) {
                                send_pubrel(info.packet_id,
                                            v5::pubrel_reason_code::success,
                                            v5::properties{},
                                            any{});
                            }
                            else {
                                store_pubrel(info.packet_id,
                                             v5::pubrel_reason_code::success,
                                             v5::properties{},
                                             any{});
                            }
                        },
                        [&] {
                            if (connected_) {
                                async_send_pubrel(
                                    info.packet_id,
                                    v5::pubrel_reason_code::success,
                                    v5::properties{},
                                    any{},
                                    [session_life_keeper](auto){}
                                );
                            }
                            else {
                                store_pubrel(info.packet_id,
                                             v5::pubrel_reason_code::success,
                                             v5::properties{},
                                             any{});
                            }
                        }
                    );
                };
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_pubrec(info.packet_id)) {
                    res();
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_pubrec(info.packet_id, info.reason_code, force_move(info.props))) {
                    res();
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
        } break;
        }
    }

    // process pubrel

    enum class pubrel_phase {
        packet_id,
        reason_code,
        properties,
        finish,
    };

    struct pubrel_info {
        packet_id_t packet_id;
        v5::pubrel_reason_code reason_code;
        v5::properties props;
    };

    void process_pubrel(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<pubrel_info,
                       &this_type::process_pubrel_impl<pubrel_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            pubrel_info(),
            force_move(self)
        );
    }

    template<pubrel_phase Phase>
    void process_pubrel_impl(
        any session_life_keeper,
        buffer buf,
        pubrel_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case pubrel_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    if (remaining_length_ == 0) {
                        info.reason_code = v5::pubrel_reason_code::success;
                        process_pubrel_impl<pubrel_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubrel_impl<pubrel_phase::reason_code>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubrel_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::pubrel_reason_code>(body[0]);
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used
                    if (remaining_length_ == 0) {
                        process_pubrel_impl<pubrel_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubrel_impl<pubrel_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubrel_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_pubrel_impl<pubrel_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case pubrel_phase::finish: {
            auto res =
                [&] {
                    auto_pub_response(
                        [&] {
                            if (connected_) {
                                send_pubcomp(info.packet_id,
                                             v5::pubcomp_reason_code::success,
                                             v5::properties{});
                            }
                        },
                        [&] {
                            if (connected_) {
                                async_send_pubcomp(
                                    info.packet_id,
                                    v5::pubcomp_reason_code::success,
                                    v5::properties{},
                                    [session_life_keeper](auto){}
                                );
                            }
                        }
                    );
                };
            qos2_publish_handled_.erase(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_pubrel(info.packet_id)) {
                    res();
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_pubrel(info.packet_id, info.reason_code, force_move(info.props))) {
                    res();
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
        } break;
        }
    }

    // process pubcomp

    enum class pubcomp_phase {
        packet_id,
        reason_code,
        properties,
        finish,
    };

    struct pubcomp_info {
        packet_id_t packet_id;
        v5::pubcomp_reason_code reason_code;
        v5::properties props;
    };

    void process_pubcomp(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<pubcomp_info,
                       &this_type::process_pubcomp_impl<pubcomp_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            pubcomp_info(),
            force_move(self)
        );
    }

    template<pubcomp_phase Phase>
    void process_pubcomp_impl(
        any session_life_keeper,
        buffer buf,
        pubcomp_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case pubcomp_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                    // If the Remaining Length is 0, there is no reason code & property length
                    // the value of success is used for reason code, the value of 0 is used for property length
                    if (remaining_length_ == 0) {
                        info.reason_code = v5::pubcomp_reason_code::success;
                        process_pubcomp_impl<pubcomp_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubcomp_impl<pubcomp_phase::reason_code>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubcomp_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::pubcomp_reason_code>(body[0]);
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                    // If the Remaining Length is 0, there is no property length and the value of 0 is used
                    if (remaining_length_ == 0) {
                        process_pubcomp_impl<pubcomp_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_pubcomp_impl<pubcomp_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case pubcomp_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_pubcomp_impl<pubcomp_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case pubcomp_phase::finish:
            {
                LockGuard<Mutex> lck (store_mtx_);
                auto& idx = store_.template get<tag_packet_id_type>();
                auto r = idx.equal_range(std::make_tuple(info.packet_id, control_packet_type::pubcomp));
                idx.erase(std::get<0>(r), std::get<1>(r));
                packet_id_.erase(info.packet_id);
            }
            on_serialize_remove(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_pubcomp(info.packet_id)) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_pubcomp(info.packet_id, info.reason_code, force_move(info.props))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
            break;
        }
    }

    // process subscribe

    enum class subscribe_phase {
        packet_id,
        properties,
        topic,
        finish,
    };

    struct subscribe_info {
        packet_id_t packet_id;
        v5::properties props;
        std::vector<std::tuple<buffer, subscribe_options>> entries;
    };

    void process_subscribe(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<subscribe_info,
                       &this_type::process_subscribe_impl<subscribe_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            subscribe_info(),
            force_move(self)
        );
    }

    template<subscribe_phase Phase>
    void process_subscribe_impl(
        any session_life_keeper,
        buffer buf,
        subscribe_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case subscribe_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    if (version_ == protocol_version::v5) {
                        process_subscribe_impl<subscribe_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_subscribe_impl<subscribe_phase::topic>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case subscribe_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_subscribe_impl<subscribe_phase::topic>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case subscribe_phase::topic:
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer topic_filter, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    process_nbytes(
                        force_move(session_life_keeper),
                        force_move(buf),
                        1, // requested_qos
                        [
                            this,
                            info = force_move(info),
                            topic_filter = force_move(topic_filter)
                        ]
                        (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                            subscribe_options option(static_cast<std::uint8_t>(body[0]));
                            qos requested_qos = option.get_qos();
                            if (requested_qos != qos::at_most_once &&
                                requested_qos != qos::at_least_once &&
                                requested_qos != qos::exactly_once) {
                                call_protocol_error_handlers();
                                return;
                            }
                            info.entries.emplace_back(force_move(topic_filter), option);
                            if (remaining_length_ == 0) {
                                process_subscribe_impl<subscribe_phase::finish>(
                                    force_move(session_life_keeper),
                                    force_move(buf),
                                    force_move(info),
                                    force_move(self)
                                );
                            }
                            else {
                                process_subscribe_impl<subscribe_phase::topic>(
                                    force_move(session_life_keeper),
                                    force_move(buf),
                                    force_move(info),
                                    force_move(self)
                                );
                            }
                        },
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case subscribe_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_subscribe(info.packet_id, force_move(info.entries))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_subscribe(info.packet_id, force_move(info.entries), force_move(info.props))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
            break;
        }
    }

    // process suback

    enum class suback_phase {
        packet_id,
        properties,
        reasons,
    };

    struct suback_info {
        packet_id_t packet_id;
        v5::properties props;
    };

    void process_suback(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<suback_info,
                       &this_type::process_suback_impl<suback_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            suback_info(),
            force_move(self)
        );
    }

    template<suback_phase Phase>
    void process_suback_impl(
        any session_life_keeper,
        buffer buf,
        suback_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case suback_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    if (version_ == protocol_version::v5) {
                        process_suback_impl<suback_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_suback_impl<suback_phase::reasons>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case suback_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_suback_impl<suback_phase::reasons>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case suback_phase::reasons:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                remaining_length_, // Reason Codes
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer /*buf*/, any session_life_keeper, this_type_sp /*self*/) mutable {
                    {
                        LockGuard<Mutex> lck_store (store_mtx_);
                        LockGuard<Mutex> lck_sub_unsub (sub_unsub_inflight_mtx_);
                        packet_id_.erase(info.packet_id);
                        sub_unsub_inflight_.erase(info.packet_id);
                    }
                    switch (version_) {
                    case protocol_version::v3_1_1:
                    {
                        // TODO: We can avoid an allocation by casting the raw bytes of the
                        // mqtt::buffer that is being parsed, and instead call the suback
                        // handler with an std::span and the mqtt::buffer (as lifekeeper)
                        std::vector<suback_return_code> results;
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
                        if (on_suback(info.packet_id, force_move(results))) {
                            on_mqtt_message_processed(force_move(session_life_keeper));
                        }
                        break;
                    }
                    case protocol_version::v5:
                    {
                        // TODO: We can avoid an allocation by casting the raw bytes of the
                        // mqtt::buffer that is being parsed, and instead call the suback
                        // handler with an std::span and the mqtt::buffer (as lifekeeper)
                        std::vector<v5::suback_reason_code> reasons;
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
                        if (on_v5_suback(info.packet_id, force_move(reasons), force_move(info.props))) {
                            on_mqtt_message_processed(force_move(session_life_keeper));
                        }
                        break;
                    }
                    default:
                        BOOST_ASSERT(false);
                    }
                },
                force_move(self)
            );
            break;
        }
    }

    // process unsubscribe

    enum class unsubscribe_phase {
        packet_id,
        properties,
        topic,
        finish,
    };

    struct unsubscribe_info {
        packet_id_t packet_id;
        v5::properties props;
        std::vector<buffer> entries;
    };

    void process_unsubscribe(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<unsubscribe_info,
                       &this_type::process_unsubscribe_impl<unsubscribe_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            unsubscribe_info(),
            force_move(self)
        );
    }

    template<unsubscribe_phase Phase>
    void process_unsubscribe_impl(
        any session_life_keeper,
        buffer buf,
        unsubscribe_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case unsubscribe_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    if (version_ == protocol_version::v5) {
                        process_unsubscribe_impl<unsubscribe_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_unsubscribe_impl<unsubscribe_phase::topic>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case unsubscribe_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_unsubscribe_impl<unsubscribe_phase::topic>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case unsubscribe_phase::topic:
            process_string(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (buffer topic_filter, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.entries.emplace_back(force_move(topic_filter));
                    if (remaining_length_ == 0) {
                        process_unsubscribe_impl<unsubscribe_phase::finish>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                    else {
                        process_unsubscribe_impl<unsubscribe_phase::topic>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                    }
                },
                force_move(self)
            );
            break;
        case unsubscribe_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                if (on_unsubscribe(info.packet_id, force_move(info.entries))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            case protocol_version::v5:
                if (on_v5_unsubscribe(info.packet_id, force_move(info.entries), force_move(info.props))) {
                    on_mqtt_message_processed(force_move(session_life_keeper));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
            break;
        }
    }

    // process unsuback

    enum class unsuback_phase {
        packet_id,
        properties,
        reasons,
    };

    struct unsuback_info {
        packet_id_t packet_id;
        v5::properties props;
    };

    void process_unsuback(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        static constexpr std::size_t header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<unsuback_info,
                       &this_type::process_unsuback_impl<unsuback_phase::packet_id>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            unsuback_info(),
            force_move(self)
        );
    }

    template<unsuback_phase Phase>
    void process_unsuback_impl(
        any session_life_keeper,
        buffer buf,
        unsuback_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case unsuback_phase::packet_id:
            process_packet_id(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (packet_id_t packet_id, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    {
                        LockGuard<Mutex> lck_store (store_mtx_);
                        LockGuard<Mutex> lck_sub_unsub (sub_unsub_inflight_mtx_);
                        packet_id_.erase(info.packet_id);
                        sub_unsub_inflight_.erase(info.packet_id);
                    }
                    switch (version_) {
                    case protocol_version::v3_1_1:
                        if (on_unsuback(info.packet_id)) {
                            on_mqtt_message_processed(force_move(session_life_keeper));
                        }
                        break;
                    case protocol_version::v5:
                        process_unsuback_impl<unsuback_phase::properties>(
                            force_move(session_life_keeper),
                            force_move(buf),
                            force_move(info),
                            force_move(self)
                        );
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                },
                force_move(self)
            );
            break;
        case unsuback_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_unsuback_impl<unsuback_phase::reasons>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case unsuback_phase::reasons:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                remaining_length_, // Reason Codes
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer /*buf*/, any session_life_keeper, this_type_sp /*self*/) mutable {
                    BOOST_ASSERT(version_ == protocol_version::v5);
                    {
                        LockGuard<Mutex> lck_store (store_mtx_);
                        LockGuard<Mutex> lck_sub_unsub (sub_unsub_inflight_mtx_);
                        packet_id_.erase(info.packet_id);
                        sub_unsub_inflight_.erase(info.packet_id);
                    }

                    std::vector<v5::unsuback_reason_code> reasons;
                    reasons.resize(body.size());
                    std::transform(
                        body.begin(),
                        body.end(),
                        reasons.begin(),
                        [&](auto const& e) {
                            return static_cast<v5::unsuback_reason_code>(e);
                        }
                    );
                    if (on_v5_unsuback(info.packet_id, force_move(reasons), force_move(info.props))) {
                        on_mqtt_message_processed(force_move(session_life_keeper));
                    }
                },
                force_move(self)
            );
            break;
        }
    }

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

    enum class disconnect_phase {
        reason_code,
        properties,
        finish,
    };

    struct disconnect_info {
        v5::disconnect_reason_code reason_code;
        v5::properties props;
    };

    void process_disconnect(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        if (remaining_length_ == 0) {
            disconnect_info info { v5::disconnect_reason_code::normal_disconnection, v5::properties() };
            process_disconnect_impl<disconnect_phase::finish>(
                force_move(session_life_keeper),
                buffer(),
                force_move(info),
                force_move(self)
            );
            return;
        }

        if (version_ != protocol_version::v5) {
            call_protocol_error_handlers();
            return;
        }

        static constexpr std::size_t header_len =
            1; // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<disconnect_info,
                       &this_type::process_disconnect_impl<disconnect_phase::reason_code>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            disconnect_info(),
            force_move(self)
        );
    }

    template<disconnect_phase Phase>
    void process_disconnect_impl(
        any session_life_keeper,
        buffer buf,
        disconnect_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case disconnect_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::disconnect_reason_code>(body[0]);
                    process_disconnect_impl<disconnect_phase::properties>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case disconnect_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_disconnect_impl<disconnect_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case disconnect_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                on_disconnect();
                break;
            case protocol_version::v5:
                on_v5_disconnect(info.reason_code, force_move(info.props));
                break;
            default:
                BOOST_ASSERT(false);
            }
            shutdown(*socket_);
            on_mqtt_message_processed(force_move(session_life_keeper));
            break;
        }
    }

    // process auth

    enum class auth_phase {
        reason_code,
        properties,
        finish,
    };

    struct auth_info {
        v5::auth_reason_code reason_code;
        v5::properties props;
    };

    void process_auth(
        any session_life_keeper,
        bool all_read,
        this_type_sp self
    ) {
        if (version_ != protocol_version::v5) {
            call_protocol_error_handlers();
            return;
        }

        if (remaining_length_ == 0) {
            process_auth_impl<auth_phase::finish>(
                force_move(session_life_keeper),
                buffer(),
                auth_info{ v5::auth_reason_code::success, v5::properties() },
                force_move(self)
            );
            return;
        }

        static constexpr std::size_t header_len =
            1; // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers();
            return;
        }

        process_header<auth_info,
                       &this_type::process_auth_impl<auth_phase::reason_code>>(
            force_move(session_life_keeper),
            all_read,
            header_len,
            auth_info(),
            force_move(self)
        );
    }

    template<auth_phase Phase>
    void process_auth_impl(
        any session_life_keeper,
        buffer buf,
        auth_info&& info,
        this_type_sp self
    ) {
        switch (Phase) {
        case auth_phase::reason_code:
            process_nbytes(
                force_move(session_life_keeper),
                force_move(buf),
                1, // reason_code
                [
                    this,
                    info = force_move(info)
                ]
                (buffer body, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.reason_code = static_cast<v5::auth_reason_code>(body[0]);
                    process_auth_impl<auth_phase::properties>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case auth_phase::properties:
            process_properties(
                force_move(session_life_keeper),
                force_move(buf),
                [
                    this,
                    info = force_move(info)
                ]
                (v5::properties props, buffer buf, any session_life_keeper, this_type_sp self) mutable {
                    info.props = force_move(props);
                    process_auth_impl<auth_phase::finish>(
                        force_move(session_life_keeper),
                        force_move(buf),
                        force_move(info),
                        force_move(self)
                    );
                },
                force_move(self)
            );
            break;
        case auth_phase::finish:
            BOOST_ASSERT(version_ == protocol_version::v5);
            if (on_v5_auth(info.reason_code, force_move(info.props))) {
                on_mqtt_message_processed(force_move(session_life_keeper));
            }
            break;
        }
    }

    template <typename F, typename AF>
    void auto_pub_response(F const& f, AF const& af) {
        if (auto_pub_response_) {
            if (auto_pub_response_async_) af();
            else f();
        }
    }

    // Blocking senders.
    void send_connect(
        buffer client_id,
        optional<buffer> user_name,
        optional<buffer> password,
        optional<will> w,
        std::uint16_t keep_alive_sec,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(
                v3_1_1::connect_message(
                    keep_alive_sec,
                    force_move(client_id),
                    clean_session_,
                    force_move(w),
                    force_move(user_name),
                    force_move(password)
                )
            );
            break;
        case protocol_version::v5:
            do_sync_write(
                v5::connect_message(
                    keep_alive_sec,
                    force_move(client_id),
                    clean_session_,
                    force_move(w),
                    force_move(user_name),
                    force_move(password),
                    force_move(props)
                )
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_connack(
        bool session_present,
        variant<connect_return_code, v5::connect_reason_code> reason_code,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(
                v3_1_1::connack_message(
                    session_present,
                    variant_get<connect_return_code>(reason_code)
                )
            );
            break;
        case protocol_version::v5:
            do_sync_write(
                v5::connack_message(
                    session_present,
                    variant_get<v5::connect_reason_code>(reason_code),
                    force_move(props)
                )
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_publish(
        packet_id_t      packet_id,
        as::const_buffer topic_name,
        as::const_buffer payload,
        publish_options  pubopts,
        v5::properties   props,
        any              life_keeper) {

        auto do_send_publish =
            [&](auto msg, auto const& serialize_publish) {

                if (pubopts.get_qos() == qos::at_least_once || pubopts.get_qos() == qos::exactly_once) {
                    auto store_msg = msg;
                    store_msg.set_dup(true);
                    LockGuard<Mutex> lck (store_mtx_);
                    store_.emplace(
                        packet_id,
                        pubopts.get_qos() == qos::at_least_once
                         ? control_packet_type::puback
                         : control_packet_type::pubrec,
                        store_msg,
                        force_move(life_keeper)
                    );
                    (this->*serialize_publish)(store_msg);
                }
                do_sync_write(force_move(msg));
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            do_send_publish(
                v3_1_1::basic_publish_message<PacketIdBytes>(
                    packet_id,
                    topic_name,
                    payload,
                    pubopts
                ),
                &endpoint::on_serialize_publish_message
            );
            break;
        case protocol_version::v5:
            do_send_publish(
                v5::basic_publish_message<PacketIdBytes>(
                    packet_id,
                    topic_name,
                    payload,
                    pubopts,
                    force_move(props)
                ),
                &endpoint::on_serialize_v5_publish_message
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_puback(
        packet_id_t packet_id,
        v5::puback_reason_code reason,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_puback_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_puback_message<PacketIdBytes>(packet_id, reason, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        on_pub_res_sent(packet_id);
    }

    void send_pubrec(
        packet_id_t packet_id,
        v5::pubrec_reason_code reason,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_pubrec_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_pubrec_message<PacketIdBytes>(packet_id, reason, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pubrel(
        packet_id_t packet_id,
        v5::pubrel_reason_code reason,
        v5::properties props,
        any life_keeper
    ) {

        auto impl =
            [&](auto msg, auto const& serialize) {
                {
                    LockGuard<Mutex> lck (store_mtx_);

                    // insert if not registerd (start from pubrel sending case)
                    packet_id_.insert(packet_id);

                    auto ret = store_.emplace(
                        packet_id,
                        control_packet_type::pubcomp,
                        msg,
                        force_move(life_keeper)
                    );
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                }

                (this->*serialize)(msg);
                do_sync_write(force_move(msg));
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                &endpoint::on_serialize_pubrel_message
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                &endpoint::on_serialize_v5_pubrel_message
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void store_pubrel(
        packet_id_t packet_id,
        v5::pubrel_reason_code reason,
        v5::properties props,
        any life_keeper
    ) {

        auto impl =
            [&](auto msg, auto const& serialize) {
                {
                    LockGuard<Mutex> lck (store_mtx_);

                    // insert if not registerd (start from pubrel sending case)
                    packet_id_.insert(packet_id);

                    auto ret = store_.emplace(
                        packet_id,
                        control_packet_type::pubcomp,
                        msg,
                        force_move(life_keeper)
                    );
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                }

                (this->*serialize)(msg);
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                &endpoint::on_serialize_pubrel_message
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                &endpoint::on_serialize_v5_pubrel_message
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pubcomp(
        packet_id_t packet_id,
        v5::pubcomp_reason_code reason,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_pubcomp_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_pubcomp_message<PacketIdBytes>(packet_id, reason, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        on_pub_res_sent(packet_id);
    }

    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        packet_id_t packet_id,
        v5::properties props
    ) {
        {
            LockGuard<Mutex> lck (sub_unsub_inflight_mtx_);
            sub_unsub_inflight_.insert(packet_id);
        }
        for(auto const& p : params)
        {
            (void)p;
            BOOST_ASSERT(
                std::get<1>(p).get_qos() == qos::at_most_once ||
                std::get<1>(p).get_qos() == qos::at_least_once ||
                std::get<1>(p).get_qos() == qos::exactly_once
            );
        }
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_subscribe_message<PacketIdBytes>(force_move(params), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_subscribe_message<PacketIdBytes>(force_move(params), packet_id, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_suback(
        variant<std::vector<suback_return_code>, std::vector<v5::suback_reason_code>> params,
        packet_id_t packet_id,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_suback_message<PacketIdBytes>(force_move(variant_get<std::vector<suback_return_code>>(params)), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_suback_message<PacketIdBytes>(force_move(variant_get<std::vector<v5::suback_reason_code>>(params)), packet_id, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_unsubscribe(
        std::vector<as::const_buffer> params,
        packet_id_t packet_id,
        v5::properties props
    ) {
        {
            LockGuard<Mutex> lck (sub_unsub_inflight_mtx_);
            sub_unsub_inflight_.insert(packet_id);
        }
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_unsubscribe_message<PacketIdBytes>(force_move(params), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_unsubscribe_message<PacketIdBytes>(force_move(params), packet_id, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_unsuback(
        packet_id_t packet_id
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_unsuback_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            BOOST_ASSERT(false);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_unsuback(
        std::vector<v5::unsuback_reason_code> params,
        packet_id_t packet_id,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_unsuback_message<PacketIdBytes>(force_move(params), packet_id, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pingreq() {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::pingreq_message());
            set_pingresp_timer();
            break;
        case protocol_version::v5:
            do_sync_write(v5::pingreq_message());
            set_pingresp_timer();
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pingresp() {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::pingresp_message());
            break;
        case protocol_version::v5:
            do_sync_write(v5::pingresp_message());
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_auth(
        v5::auth_reason_code reason,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_sync_write(v5::auth_message(reason, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_disconnect(
        v5::disconnect_reason_code reason,
        v5::properties props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::disconnect_message());
            break;
        case protocol_version::v5:
            do_sync_write(v5::disconnect_message(reason, force_move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_store() {
        LockGuard<Mutex> lck (store_mtx_);
        auto const& idx = store_.template get<tag_seq>();
        for (auto const& e : idx) {
            do_sync_write(e.message());
        }
    }

    // Blocking write
    template <typename MessageVariant>
    void do_sync_write(MessageVariant&& mv) {
        boost::system::error_code ec;
        if (!connected_) return;
        on_pre_send();
        total_bytes_sent_ += socket_->write(const_buffer_sequence<PacketIdBytes>(std::forward<MessageVariant>(mv)), ec);
        // If ec is set as error, the error will be handled by async_read.
        // If `handle_error(ec);` is called here, error_handler would be called twice.
    }

    // Non blocking (async) senders
    void async_send_connect(
        buffer client_id,
        optional<buffer> user_name,
        optional<buffer> password,
        optional<will> const& w,
        std::uint16_t keep_alive_sec,
        v5::properties props,
        async_handler_t func
    ) {

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::connect_message(
                    keep_alive_sec,
                    force_move(client_id),
                    clean_session_,
                    w,
                    force_move(user_name),
                    force_move(password)
                ),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::connect_message(
                    keep_alive_sec,
                    force_move(client_id),
                    clean_session_,
                    w,
                    force_move(user_name),
                    force_move(password),
                    force_move(props)
                ),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_connack(
        bool session_present,
        variant<connect_return_code, v5::connect_reason_code> reason_code,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::connack_message(
                    session_present,
                    variant_get<connect_return_code>(reason_code)
                ),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::connack_message(
                    session_present,
                    variant_get<v5::connect_reason_code>(reason_code),
                    force_move(props)
                ),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer payload,
        publish_options pubopts,
        v5::properties props,
        any life_keeper,
        async_handler_t func
    ) {
        auto do_async_send_publish =
            [&](auto msg, auto const& serialize_publish) {
                if (pubopts.get_qos() == qos::at_least_once || pubopts.get_qos() == qos::exactly_once) {
                    auto store_msg = msg;
                    store_msg.set_dup(true);
                    {
                        LockGuard<Mutex> lck (store_mtx_);
                        auto ret = store_.emplace(
                            packet_id,
                            pubopts.get_qos() == qos::at_least_once ? control_packet_type::puback
                                                                    : control_packet_type::pubrec,
                            store_msg,
                            life_keeper
                        );
                        (void)ret;
                        BOOST_ASSERT(ret.second);
                    }

                    (this->*serialize_publish)(store_msg);
                }
                do_async_write(
                    force_move(msg),
                    [life_keeper = force_move(life_keeper), func = force_move(func)](error_code ec) {
                        if (func) func(ec);
                    }
                );
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_send_publish(
                v3_1_1::basic_publish_message<PacketIdBytes>(
                    packet_id,
                    topic_name,
                    payload,
                    pubopts
                ),
                &endpoint::on_serialize_publish_message
            );
            break;
        case protocol_version::v5:
            do_async_send_publish(
                v5::basic_publish_message<PacketIdBytes>(
                    packet_id,
                    topic_name,
                    payload,
                    pubopts,
                    force_move(props)
                ),
                &endpoint::on_serialize_v5_publish_message
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_puback(
        packet_id_t packet_id,
        v5::puback_reason_code reason,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_puback_message<PacketIdBytes>(packet_id),
                [this, self = this->shared_from_this(), packet_id, func = force_move(func)]
                (error_code ec) {
                    if (func) func(ec);
                    on_pub_res_sent(packet_id);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_puback_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                [this, self = this->shared_from_this(), packet_id, func = force_move(func)]
                (error_code ec) {
                    if (func) func(ec);
                    on_pub_res_sent(packet_id);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubrec(
        packet_id_t packet_id,
        v5::pubrec_reason_code reason,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_pubrec_message<PacketIdBytes>(packet_id),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_pubrec_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubrel(
        packet_id_t packet_id,
        v5::pubrel_reason_code reason,
        v5::properties props,
        any life_keeper,
        async_handler_t func
    ) {

        auto msg = basic_pubrel_message<PacketIdBytes>(packet_id);

        auto impl =
            [&](auto msg, auto const& serialize) {
                {
                    LockGuard<Mutex> lck (store_mtx_);

                    // insert if not registerd (start from pubrel sending case)
                    packet_id_.insert(packet_id);

                    auto ret = store_.emplace(
                        packet_id,
                        control_packet_type::pubcomp,
                        msg,
                        life_keeper
                    );
                    // publish store is erased when pubrec is received.
                    // pubrel store is erased when pubcomp is received.
                    // If invalid client send pubrec twice with the same packet id,
                    // then send corresponding pubrel twice is a possible client/server
                    // implementation.
                    // In this case, overwrite store_.
                    if (!ret.second) {
                        store_.modify(
                            ret.first,
                            [&] (auto& e) {
                                e = store(
                                    packet_id,
                                    control_packet_type::pubcomp,
                                    force_move(msg),
                                    life_keeper
                                );
                            }
                        );
                    }
                }

                (this->*serialize)(msg);

                do_async_write(
                    force_move(msg),
                    [life_keeper = force_move(life_keeper), func = force_move(func)](error_code ec) {
                        if(func) func(ec);
                    }
                );
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                &endpoint::on_serialize_pubrel_message
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                &endpoint::on_serialize_v5_pubrel_message
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubcomp(
        packet_id_t packet_id,
        v5::pubcomp_reason_code reason,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_pubcomp_message<PacketIdBytes>(packet_id),
                [this, self = this->shared_from_this(), packet_id, func = force_move(func)]
                (error_code ec) {
                    if (func) func(ec);
                    on_pub_res_sent(packet_id);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_pubcomp_message<PacketIdBytes>(packet_id, reason, force_move(props)),
                [this, self = this->shared_from_this(), packet_id, func = force_move(func)]
                (error_code ec) {
                    if (func) func(ec);
                    on_pub_res_sent(packet_id);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        packet_id_t packet_id,
        v5::properties props,
        async_handler_t func
    ) {
        {
            LockGuard<Mutex> lck (sub_unsub_inflight_mtx_);
            sub_unsub_inflight_.insert(packet_id);
        }
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_subscribe_message<PacketIdBytes>(
                    force_move(params),
                    packet_id),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_subscribe_message<PacketIdBytes>(
                    force_move(params),
                    packet_id,
                    force_move(props)),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_suback(
        variant<std::vector<suback_return_code>, std::vector<v5::suback_reason_code>> params,
        packet_id_t packet_id,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_suback_message<PacketIdBytes>(
                    force_move(variant_get<std::vector<suback_return_code>>(params)),
                    packet_id),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_suback_message<PacketIdBytes>(
                    force_move(variant_get<std::vector<v5::suback_reason_code>>(params)),
                    packet_id,
                    force_move(props)),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_unsubscribe(
        std::vector<as::const_buffer> params,
        packet_id_t packet_id,
        v5::properties props,
        async_handler_t func
    ) {
        {
            LockGuard<Mutex> lck (sub_unsub_inflight_mtx_);
            sub_unsub_inflight_.insert(packet_id);
        }
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_unsubscribe_message<PacketIdBytes>(
                    force_move(params),
                    packet_id
                ),
                force_move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_unsubscribe_message<PacketIdBytes>(
                    force_move(params),
                    packet_id,
                    force_move(props)
                ),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_unsuback(
        packet_id_t packet_id,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_unsuback_message<PacketIdBytes>(packet_id), force_move(func)
            );
            break;
        case protocol_version::v5:
            BOOST_ASSERT(false);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_unsuback(
        std::vector<v5::unsuback_reason_code> params,
        packet_id_t packet_id,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_unsuback_message<PacketIdBytes>(
                    force_move(params),
                    packet_id,
                    force_move(props)
                ),
                force_move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pingreq(async_handler_t func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::pingreq_message(), force_move(func));
            set_pingresp_timer();
            break;
        case protocol_version::v5:
            do_async_write(v5::pingreq_message(), force_move(func));
            set_pingresp_timer();
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pingresp(async_handler_t func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::pingresp_message(), force_move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::pingresp_message(), force_move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_auth(
        v5::auth_reason_code reason,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_async_write(v5::auth_message(reason, force_move(props)), force_move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_disconnect(
        v5::disconnect_reason_code reason,
        v5::properties props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::disconnect_message(), force_move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::disconnect_message(reason, force_move(props)), force_move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_store(std::function<void()> func) {
        auto g = shared_scope_guard(
            [func = force_move(func)] {
                func();
            }
        );
        LockGuard<Mutex> lck (store_mtx_);
        auto const& idx = store_.template get<tag_seq>();
        for (auto const& e : idx) {
            do_async_write(
                e.message(),
                [g]
                (error_code /*ec*/) {
                }
            );
        }
    }

    // Non blocking (async) write

    class async_packet {
    public:
        async_packet(
            basic_message_variant<PacketIdBytes> mv,
            async_handler_t h = {})
            : mv_(force_move(mv))
            , handler_(force_move(h)) {}
        basic_message_variant<PacketIdBytes> const& message() const {
            return mv_;
        }
        basic_message_variant<PacketIdBytes>& message() {
            return mv_;
        }
        async_handler_t const& handler() const { return handler_; }
        async_handler_t& handler() { return handler_; }
    private:
        basic_message_variant<PacketIdBytes> mv_;
        async_handler_t handler_;
    };

    struct write_completion_handler {
        write_completion_handler(
            std::shared_ptr<this_type> self,
            async_handler_t func,
            std::size_t num_of_messages,
            std::size_t expected)
            :self_(force_move(self)),
             func_(force_move(func)),
             num_of_messages_(num_of_messages),
             bytes_to_transfer_(expected)
        {
            // write_completion_handler is only constructed in one place
            // and a handler is provided in that location.
            // Since we don't check that the handler is valid before calling it
            // it's a bug if the handler is invalid when constructed.
            BOOST_ASSERT(func_);
        }
        void operator()(error_code ec) const {
            func_(ec);
            for (std::size_t i = 0; i != num_of_messages_; ++i) {
                self_->queue_.pop_front();
            }
            if (ec || // Error is handled by async_read.
                !self_->connected_) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    // Handlers for outgoing packets need not be valid.
                    if(auto&& h = self_->queue_.front().handler()) h(ec);
                    self_->queue_.pop_front();
                }
                return;
            }
            if (!self_->queue_.empty()) {
                self_->do_async_write();
            }
        }
        void operator()(
            error_code ec,
            std::size_t bytes_transferred) const {
            func_(ec);
            self_->total_bytes_sent_ += bytes_transferred;
            for (std::size_t i = 0; i != num_of_messages_; ++i) {
                self_->queue_.pop_front();
            }
            if (ec || // Error is handled by async_read.
                !self_->connected_) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    // Handlers for outgoing packets need not be valid.
                    if(auto&& h = self_->queue_.front().handler()) h(ec);
                    self_->queue_.pop_front();
                }
                return;
            }
            if (bytes_to_transfer_ != bytes_transferred) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    // Handlers for outgoing packets need not be valid.
                    if(auto&& h = self_->queue_.front().handler()) h(ec);
                    self_->queue_.pop_front();
                }
                throw write_bytes_transferred_error(bytes_to_transfer_, bytes_transferred);
            }
            if (!self_->queue_.empty()) {
                self_->do_async_write();
            }
        }
        std::shared_ptr<this_type> self_;
        async_handler_t func_;
        std::size_t num_of_messages_;
        std::size_t bytes_to_transfer_;
    };

    void do_async_write() {
        // Only attempt to send up to the user specified maximum items
        using difference_t = typename decltype(queue_)::difference_type;
        std::size_t iterator_count =   (max_queue_send_count_ == 0)
                                ? queue_.size()
                                : std::min(max_queue_send_count_, queue_.size());
        auto const& start = queue_.cbegin();
        auto end = std::next(start, boost::numeric_cast<difference_t>(iterator_count));

        // And further, only up to the specified maximum bytes
        std::size_t total_bytes = 0;
        std::size_t total_const_buffer_sequence = 0;
        for (auto it = start; it != end; ++it) {
            auto const& elem = *it;
            auto const& mv = elem.message();
            std::size_t const size = MQTT_NS::size<PacketIdBytes>(mv);

            // If we hit the byte limit, we don't include this buffer for this send.
            if (max_queue_send_size_ != 0 && max_queue_send_size_ < total_bytes + size) {
                end = it;
                iterator_count = boost::numeric_cast<std::size_t>(std::distance(start, end));
                break;
            }
            total_bytes += size;
            total_const_buffer_sequence += num_of_const_buffer_sequence(mv);
        }

        std::vector<as::const_buffer> buf;
        std::vector<async_handler_t> handlers;

        buf.reserve(total_const_buffer_sequence);
        handlers.reserve(iterator_count);

        for (auto it = start; it != end; ++it) {
            auto const& elem = *it;
            auto const& mv = elem.message();
            auto const& cbs = const_buffer_sequence(mv);
            std::copy(cbs.begin(), cbs.end(), std::back_inserter(buf));
            handlers.emplace_back(elem.handler());
        }

        on_pre_send();

        socket_->async_write(
            force_move(buf),
            write_completion_handler(
                this->shared_from_this(),
                [handlers = force_move(handlers)]
                (error_code ec) {
                    for (auto const& h : handlers) {
                        if (h) h(ec);
                    }
                },
                iterator_count,
                total_bytes
            )
        );
    }

    void do_async_write(basic_message_variant<PacketIdBytes> mv, async_handler_t func) {
        // Move this job to the socket's strand so that it can be queued without mutexes.
        socket_->post(
            [this, self = this->shared_from_this(), mv = force_move(mv), func = force_move(func)]
            () mutable {
                if (!connected_) {
                    // offline async publish is successfully finished, because there's nothing to do.
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::success));
                    return;
                }
                queue_.emplace_back(force_move(mv), force_move(func));
                // Only need to start async writes if there was nothing in the queue before the above item.
                if (queue_.size() > 1) return;
                do_async_write();
            }
        );
    }

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
        for (auto packet_id : sub_unsub_inflight_) {
            packet_id_.erase(packet_id);
        }
    }

    void clean_sub_unsub_inflight_on_error(error_code ec) {
        clean_sub_unsub_inflight();
        on_error(ec);
    }

    void set_pingresp_timer() {
        if (pingresp_timeout_ == std::chrono::steady_clock::duration::zero()) return;
        if (tim_pingresp_set_) return;
        tim_pingresp_set_ = true;
        tim_pingresp_.expires_after(pingresp_timeout_);
        std::weak_ptr<this_type> wp(std::static_pointer_cast<this_type>(this->shared_from_this()));
        tim_pingresp_.async_wait(
            [wp = force_move(wp)](error_code ec) {
                if (auto sp = wp.lock()) {
                    sp->tim_pingresp_set_ = false;
                    if (!ec) {
                        sp->socket().post(
                            [sp] {
                                sp->force_disconnect();
                            }
                        );
                    }
                }
            }
        );
    }

    static optional<topic_alias_t> get_topic_alias_by_prop(v5::property_variant const& prop) {
        optional<topic_alias_t> val;
        MQTT_NS::visit(
            make_lambda_visitor(
                [&val](v5::property::topic_alias const& p) {
                    val = p.val();
                },
                [](auto&&) {
                }
            ), prop
        );
        return val;
    }

    static optional<topic_alias_t> get_topic_alias_by_props(v5::properties const& props) {
        for (auto const& prop : props) {
            if (auto val = get_topic_alias_by_prop(prop)) {
                return val;
            }
        }
        return nullopt;
    }

protected:
    // Ensure that only code that knows the *exact* type of an object
    // inheriting from this abstract base class can destruct it.
    // This avoids issues of the destructor not triggering destruction
    // of derived classes, and any member variables contained in them.
    // Note: Not virtual to avoid need for a vtable when possible.
    ~endpoint() = default;

protected:
    bool clean_session_{false};

private:
    optional<MQTT_NS::socket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> mqtt_connected_{false};

    std::array<char, 10>  buf_;
    std::uint8_t fixed_header_;
    std::size_t remaining_length_multiplier_;
    std::size_t remaining_length_;
    std::vector<char> payload_;

    Mutex store_mtx_;
    mi_store store_;
    std::set<packet_id_t> qos2_publish_handled_;
    std::deque<async_packet> queue_;
    packet_id_t packet_id_master_{0};
    std::set<packet_id_t> packet_id_;
    Mutex sub_unsub_inflight_mtx_;
    std::set<packet_id_t> sub_unsub_inflight_;
    bool auto_pub_response_{true};
    bool auto_pub_response_async_{false};
    bool async_send_store_ { false };
    bool async_read_on_message_processed_ { true };
    bool disconnect_requested_{false};
    bool connect_requested_{false};
    std::size_t max_queue_send_count_{1};
    std::size_t max_queue_send_size_{0};
    protocol_version version_{protocol_version::undetermined};
    std::size_t packet_bulk_read_limit_ = 256;
    std::size_t props_bulk_read_limit_ = packet_bulk_read_limit_;
    std::size_t total_bytes_sent_ = 0;
    std::size_t total_bytes_received_ = 0;
    static constexpr std::uint8_t variable_length_continue_flag = 0b10000000;

    std::chrono::steady_clock::duration pingresp_timeout_ = std::chrono::steady_clock::duration::zero();
    as::steady_timer tim_pingresp_;
    bool tim_pingresp_set_ = false;

    mutable Mutex topic_alias_recv_mtx_;
    topic_alias_recv_map_t topic_alias_recv_;
};

} // namespace MQTT_NS

#endif // MQTT_ENDPOINT_HPP

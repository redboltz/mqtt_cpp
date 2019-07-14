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

#include <mqtt/any.hpp>
#include <mqtt/fixed_header.hpp>
#include <mqtt/remaining_length.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/connect_flags.hpp>
#include <mqtt/will.hpp>
#include <mqtt/session_present.hpp>
#include <mqtt/qos.hpp>
#include <mqtt/publish.hpp>
#include <mqtt/connect_return_code.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/tcp_endpoint.hpp>
#include <mqtt/unique_scope_guard.hpp>
#include <mqtt/shared_scope_guard.hpp>
#include <mqtt/message_variant.hpp>
#include <mqtt/two_byte_util.hpp>
#include <mqtt/four_byte_util.hpp>
#include <mqtt/packet_id_type.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/protocol_version.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/subscribe.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/shared_ptr_array.hpp>
#include <mqtt/type_erased_socket.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

namespace mqtt {

namespace as = boost::asio;
namespace mi = boost::multi_index;

template <typename Mutex = std::mutex, template<typename...> class LockGuard = std::lock_guard, std::size_t PacketIdBytes = 2>
class endpoint : public std::enable_shared_from_this<endpoint<Mutex, LockGuard, PacketIdBytes>> {
    using this_type = endpoint<Mutex, LockGuard, PacketIdBytes>;
    using this_type_sp = std::shared_ptr<this_type>;
public:
    using std::enable_shared_from_this<this_type>::shared_from_this;
    using async_handler_t = std::function<void(boost::system::error_code const& ec)>;
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    /**
     * @brief Constructor for client
     */
    endpoint(protocol_version version = protocol_version::undetermined, bool async_send_store = false)
        :async_send_store_{async_send_store},
         h_mqtt_message_processed_(
             [this]
             (async_handler_t func) {
                 async_read_control_packet_type(std::move(func));
             }
         ),
         version_(version)
    {}

    /**
     * @brief Constructor for server.
     *        socket should have already been connected with another endpoint.
     */
    template <typename Socket>
    explicit endpoint(std::shared_ptr<Socket> socket, protocol_version version = protocol_version::undetermined, bool async_send_store = false)
        :socket_(std::move(socket)),
         connected_(true),
         async_send_store_{async_send_store},
         h_mqtt_message_processed_(
             [this]
             (async_handler_t func) {
                 async_read_control_packet_type(std::move(func));
             }
         ),
         version_(version)
    {}

    // MQTT Common handlers

    /**
     * @brief Pingreq handler
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718086<BR>
     *        3.13 PINGREQ – PING request
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using pingreq_handler = std::function<bool()>;

    /**
     * @brief Pingresp handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200<BR>
     *        3.13 PINGRESP – PING response
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using pingresp_handler = std::function<bool()>;


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
    using connect_handler = std::function<
        bool(mqtt::buffer client_id,
             mqtt::optional<mqtt::buffer> user_name,
             mqtt::optional<mqtt::buffer> password,
             mqtt::optional<will> will,
             bool clean_session,
             std::uint16_t keep_alive)>;

    /**
     * @brief Connack handler
     * @param session_present
     *        Session present flag.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718035<BR>
     *        3.2.2.2 Session Present
     * @param return_code
     *        connect_return_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718035<BR>
     *        3.2.2.3 Connect Return code
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using connack_handler = std::function<bool(bool session_present, std::uint8_t return_code)>;
    /**
     * @brief Publish handler
     * @param fixed_header
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718038<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using mqtt::publish functions.
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is mqtt::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718039<BR>
     *        3.3.2  Variable header
     * @param topic_name
     *        Topic name
     * @param contents
     *        Published contents
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using publish_handler = std::function<bool(std::uint8_t fixed_header,
                                               mqtt::optional<packet_id_t> packet_id,
                                               mqtt::buffer topic_name,
                                               mqtt::buffer contents)>;

    /**
     * @brief Puback handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.4.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using puback_handler = std::function<bool(packet_id_t packet_id)>;

    /**
     * @brief Pubrec handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718050<BR>
     *        3.5.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using pubrec_handler = std::function<bool(packet_id_t packet_id)>;

    /**
     * @brief Pubrel handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349791<BR>
     *        3.6.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using pubrel_handler = std::function<bool(packet_id_t packet_id)>;

    /**
     * @brief Pubcomp handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718060<BR>
     *        3.7.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using pubcomp_handler = std::function<bool(packet_id_t packet_id)>;

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
    using subscribe_handler = std::function<bool(packet_id_t packet_id,
                                                 std::vector<std::tuple<mqtt::buffer, std::uint8_t>> entries)>;

    /**
     * @brief Suback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718070<BR>
     *        3.9.2 Variable header
     * @param qoss
     *        Collection of QoS that is corresponding to subscribed topic order.<BR>
     *        If subscription is failure, the value is mqtt::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718071<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using suback_handler = std::function<bool(packet_id_t packet_id,
                                              std::vector<mqtt::optional<std::uint8_t>> qoss)>;

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
    using unsubscribe_handler = std::function<bool(packet_id_t packet_id,
                                                   std::vector<mqtt::buffer> topics)>;

    /**
     * @brief Unsuback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.11.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using unsuback_handler = std::function<bool(packet_id_t)>;

    /**
     * @brief Disconnect handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800463<BR>
     *        3.14 DISCONNECT – Disconnect notification
     */
    using disconnect_handler = std::function<void()>;

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
    using v5_connect_handler = std::function<
        bool(mqtt::buffer client_id,
             mqtt::optional<mqtt::buffer> user_name,
             mqtt::optional<mqtt::buffer> password,
             mqtt::optional<will> will,
             bool clean_start,
             std::uint16_t keep_alive,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_connack_handler = std::function<
        bool(bool session_present,
             std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

    /**
     * @brief Publish handler
     * @param fixed_header
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901101<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using mqtt::publish functions.
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is mqtt::nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901108<BR>
     *        3.3.2.2 Packet Identifier
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
    using v5_publish_handler = std::function<
        bool(std::uint8_t fixed_header,
             mqtt::optional<packet_id_t> packet_id,
             mqtt::buffer topic_name,
             mqtt::buffer contents,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_puback_handler = std::function<
        bool(packet_id_t packet_id,
             std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_pubrec_handler = std::function<
        bool(packet_id_t packet_id,
             std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_pubrel_handler = std::function<
        bool(packet_id_t packet_id,
             std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_pubcomp_handler = std::function<
        bool(packet_id_t packet_id,
             std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_subscribe_handler = std::function<
        bool(packet_id_t packet_id,
             std::vector<std::tuple<mqtt::buffer, std::uint8_t>> entries,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_suback_handler = std::function<
        bool(packet_id_t packet_id,
             std::vector<std::uint8_t> reasons,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_unsubscribe_handler = std::function<
        bool(packet_id_t packet_id,
             std::vector<mqtt::buffer> topics,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_unsuback_handler = std::function<
        bool(packet_id_t,
             std::vector<std::uint8_t> reasons,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_disconnect_handler = std::function<
        void(std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;

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
    using v5_auth_handler = std::function<
        bool(std::uint8_t reason_code,
             std::vector<v5::property_variant> props)
    >;


    // Original handlers

    /**
     * @brief Close handler
     *
     * This handler is called if the client called `disconnect()` and the server closed the socket cleanly.
     * If the socket is closed by other reasons, error_handler is called.
     */
    using close_handler = std::function<void()>;

    /**
     * @brief Error handler
     *
     * This handler is called if the socket is closed without client's `disconnect()` call.
     *
     * @param ec error code
     */
    using error_handler = std::function<void(boost::system::error_code const& ec)>;

    /**
     * @brief Publish response sent handler
     *        This function is called just after puback sent on QoS1, or pubcomp sent on QoS2.
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901026<BR>
     *        2.2.1 Packet Identifier
     */
    using pub_res_sent_handler = std::function<void(packet_id_t packet_id)>;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg publish message
     */
    using serialize_publish_message_handler = std::function<void(basic_publish_message<PacketIdBytes> msg)>;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg v5::publish message
     */
    using serialize_v5_publish_message_handler = std::function<void(v5::basic_publish_message<PacketIdBytes> msg)>;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param packet_id packet identifier of the serializing message
     * @param data      pointer to the serializing message
     * @param size      size of the serializing message
     */
    using serialize_publish_handler = std::function<void(packet_id_t packet_id, char const* data, std::size_t size)>;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    using serialize_pubrel_message_handler = std::function<void(basic_pubrel_message<PacketIdBytes> msg)>;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    using serialize_v5_pubrel_message_handler = std::function<void(v5::basic_pubrel_message<PacketIdBytes> msg)>;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param packet_id packet identifier of the serializing message
     * @param data      pointer to the serializing message
     * @param size      size of the serializing message
     */
    using serialize_pubrel_handler = std::function<void(packet_id_t packet_id, char const* data, std::size_t size)>;

    /**
     * @brief Remove serialized message
     * @param packet_id packet identifier of the removing message
     */
    using serialize_remove_handler = std::function<void(packet_id_t packet_id)>;

    /**
     * @brief Pre-send handler
     *        This handler is called when any mqtt control packet is decided to send.
     */
    using pre_send_handler = std::function<void()>;

    /**
     * @brief is valid length handler
     *        This handler is called when remaining length is received.
     * @param control_packet_type control_packet_type that has variable length
     * @param remaining length
     * @return true if check is success, otherwise false
     */
    using is_valid_length_handler =
        std::function<bool(std::uint8_t control_packet_type, std::size_t remaining_length)>;

    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     * @param func A callback function that is called when async operation will finish.
     */
    using mqtt_message_processed_handler =
        std::function<void(async_handler_t func)>;

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


    // MQTT Common handlers

    /**
     * @brief Set pingreq handler
     * @param h handler
     */
    void set_pingreq_handler(pingreq_handler h = pingreq_handler()) {
        h_pingreq_ = std::move(h);
    }

    /**
     * @brief Set pingresp handler
     * @param h handler
     */
    void set_pingresp_handler(pingresp_handler h = pingresp_handler()) {
        h_pingresp_ = std::move(h);
    }


    /**
     * @brief Get pingreq handler
     * @return handler
     */
    pingreq_handler const& get_pingreq_handler() const {
        return h_pingreq_;
    }

    /**
     * @brief Get pingresp handler
     * @return handler
     */
    pingresp_handler const& get_pingresp_handler() const {
        return h_pingresp_;
    }


    // MQTT v3_1_1 handlers

    /**
     * @brief Set connect handler
     * @param h handler
     */
    void set_connect_handler(connect_handler h = connect_handler()) {
        h_connect_ = std::move(h);
    }

    /**
     * @brief Set connack handler
     * @param h handler
     */
    void set_connack_handler(connack_handler h = connack_handler()) {
        h_connack_ = std::move(h);
    }

    /**
     * @brief Set publish handler
     * @param h handler
     */
    void set_publish_handler(publish_handler h = publish_handler()) {
        h_publish_ = std::move(h);
    }

    /**
     * @brief Set puback handler
     * @param h handler
     */
    void set_puback_handler(puback_handler h = puback_handler()) {
        h_puback_ = std::move(h);
    }

    /**
     * @brief Set pubrec handler
     * @param h handler
     */
    void set_pubrec_handler(pubrec_handler h = pubrec_handler()) {
        h_pubrec_ = std::move(h);
    }

    /**
     * @brief Set pubrel handler
     * @param h handler
     */
    void set_pubrel_handler(pubrel_handler h = pubrel_handler()) {
        h_pubrel_ = std::move(h);
    }

    /**
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_pubcomp_handler(pubcomp_handler h = pubcomp_handler()) {
        h_pubcomp_ = std::move(h);
    }

    /**
     * @brief Set subscribe handler
     * @param h handler
     */
    void set_subscribe_handler(subscribe_handler h = subscribe_handler()) {
        h_subscribe_ = std::move(h);
    }

    /**
     * @brief Set suback handler
     * @param h handler
     */
    void set_suback_handler(suback_handler h = suback_handler()) {
        h_suback_ = std::move(h);
    }

    /**
     * @brief Set unsubscribe handler
     * @param h handler
     */
    void set_unsubscribe_handler(unsubscribe_handler h = unsubscribe_handler()) {
        h_unsubscribe_ = std::move(h);
    }

    /**
     * @brief Set unsuback handler
     * @param h handler
     */
    void set_unsuback_handler(unsuback_handler h = unsuback_handler()) {
        h_unsuback_ = std::move(h);
    }

    /**
     * @brief Set disconnect handler
     * @param h handler
     */
    void set_disconnect_handler(disconnect_handler h = disconnect_handler()) {
        h_disconnect_ = std::move(h);
    }

    /**
     * @brief Get connect handler
     * @return handler
     */
    connect_handler const& get_connect_handler() const {
        return h_connect_;
    }

    /**
     * @brief Get connack handler
     * @return handler
     */
    connack_handler const& get_connack_handler() const {
        return h_connack_;
    }

    /**
     * @brief Set publish handler
     * @return handler
     */
    publish_handler const& get_publish_handler() const {
        return h_publish_;
    }

    /**
     * @brief Get puback handler
     * @return handler
     */
    puback_handler const& get_puback_handler() const {
        return h_puback_;
    }

    /**
     * @brief Get pubrec handler
     * @return handler
     */
    pubrec_handler const& get_pubrec_handler() const {
        return h_pubrec_;
    }

    /**
     * @brief Get pubrel handler
     * @return handler
     */
    pubrel_handler const& get_pubrel_handler() const {
        return h_pubrel_;
    }

    /**
     * @brief Get pubcomp handler
     * @return handler
     */
    pubcomp_handler const& get_pubcomp_handler() const {
        return h_pubcomp_;
    }

    /**
     * @brief Get subscribe handler
     * @return handler
     */
    subscribe_handler const& get_subscribe_handler() const {
        return h_subscribe_;
    }

    /**
     * @brief Get suback handler
     * @return handler
     */
    suback_handler const& get_suback_handler() const {
        return h_suback_;
    }

    /**
     * @brief Get unsubscribe handler
     * @return handler
     */
    unsubscribe_handler const& get_unsubscribe_handler() const {
        return h_unsubscribe_;
    }

    /**
     * @brief Get unsuback handler
     * @return handler
     */
    unsuback_handler const& get_unsuback_handler() const {
        return h_unsuback_;
    }

    /**
     * @brief Get disconnect handler
     * @return handler
     */
    disconnect_handler const& get_disconnect_handler() const {
        return h_disconnect_;
    }

    // MQTT v5 handlers

    /**
     * @brief Set connect handler
     * @param h handler
     */
    void set_v5_connect_handler(v5_connect_handler h = v5_connect_handler()) {
        h_v5_connect_ = std::move(h);
    }

    /**
     * @brief Set connack handler
     * @param h handler
     */
    void set_v5_connack_handler(v5_connack_handler h = v5_connack_handler()) {
        h_v5_connack_ = std::move(h);
    }

    /**
     * @brief Set publish handler
     * @param h handler
     */
    void set_v5_publish_handler(v5_publish_handler h = v5_publish_handler()) {
        h_v5_publish_ = std::move(h);
    }

    /**
     * @brief Set puback handler
     * @param h handler
     */
    void set_v5_puback_handler(v5_puback_handler h = v5_puback_handler()) {
        h_v5_puback_ = std::move(h);
    }

    /**
     * @brief Set pubrec handler
     * @param h handler
     */
    void set_v5_pubrec_handler(v5_pubrec_handler h = v5_pubrec_handler()) {
        h_v5_pubrec_ = std::move(h);
    }

    /**
     * @brief Set pubrel handler
     * @param h handler
     */
    void set_v5_pubrel_handler(v5_pubrel_handler h = v5_pubrel_handler()) {
        h_v5_pubrel_ = std::move(h);
    }

    /**
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_v5_pubcomp_handler(v5_pubcomp_handler h = v5_pubcomp_handler()) {
        h_v5_pubcomp_ = std::move(h);
    }

    /**
     * @brief Set subscribe handler
     * @param h handler
     */
    void set_v5_subscribe_handler(v5_subscribe_handler h = v5_subscribe_handler()) {
        h_v5_subscribe_ = std::move(h);
    }

    /**
     * @brief Set suback handler
     * @param h handler
     */
    void set_v5_suback_handler(v5_suback_handler h = v5_suback_handler()) {
        h_v5_suback_ = std::move(h);
    }

    /**
     * @brief Set unsubscribe handler
     * @param h handler
     */
    void set_v5_unsubscribe_handler(v5_unsubscribe_handler h = v5_unsubscribe_handler()) {
        h_v5_unsubscribe_ = std::move(h);
    }

    /**
     * @brief Set unsuback handler
     * @param h handler
     */
    void set_v5_unsuback_handler(v5_unsuback_handler h = v5_unsuback_handler()) {
        h_v5_unsuback_ = std::move(h);
    }

    /**
     * @brief Set disconnect handler
     * @param h handler
     */
    void set_v5_disconnect_handler(v5_disconnect_handler h = v5_disconnect_handler()) {
        h_v5_disconnect_ = std::move(h);
    }

    /**
     * @brief Set auth handler
     * @param h handler
     */
    void set_v5_auth_handler(v5_auth_handler h = v5_auth_handler()) {
        h_v5_auth_ = std::move(h);
    }

    /**
     * @brief Get connect handler
     * @return handler
     */
    v5_connect_handler const& get_v5_connect_handler() const {
        return h_v5_connect_;
    }

    /**
     * @brief Get connack handler
     * @return handler
     */
    v5_connack_handler const& get_v5_connack_handler() const {
        return h_v5_connack_;
    }

    /**
     * @brief Set publish handler
     * @return handler
     */
    v5_publish_handler const& get_v5_publish_handler() const {
        return h_v5_publish_;
    }

    /**
     * @brief Get puback handler
     * @return handler
     */
    v5_puback_handler const& get_v5_puback_handler() const {
        return h_v5_puback_;
    }

    /**
     * @brief Get pubrec handler
     * @return handler
     */
    v5_pubrec_handler const& get_v5_pubrec_handler() const {
        return h_v5_pubrec_;
    }

    /**
     * @brief Get pubrel handler
     * @return handler
     */
    v5_pubrel_handler const& get_v5_pubrel_handler() const {
        return h_v5_pubrel_;
    }

    /**
     * @brief Get pubcomp handler
     * @return handler
     */
    v5_pubcomp_handler const& get_v5_pubcomp_handler() const {
        return h_v5_pubcomp_;
    }

    /**
     * @brief Get subscribe handler
     * @return handler
     */
    v5_subscribe_handler const& get_v5_subscribe_handler() const {
        return h_v5_subscribe_;
    }

    /**
     * @brief Get suback handler
     * @return handler
     */
    v5_suback_handler const& get_v5_suback_handler() const {
        return h_v5_suback_;
    }

    /**
     * @brief Get unsubscribe handler
     * @return handler
     */
    v5_unsubscribe_handler const& get_v5_unsubscribe_handler() const {
        return h_v5_unsubscribe_;
    }

    /**
     * @brief Get unsuback handler
     * @return handler
     */
    v5_unsuback_handler const& get_v5_unsuback_handler() const {
        return h_v5_unsuback_;
    }

    /**
     * @brief Get disconnect handler
     * @return handler
     */
    v5_disconnect_handler const& get_v5_disconnect_handler() const {
        return h_v5_disconnect_;
    }

    /**
     * @brief Get auth handler
     * @return handler
     */
    v5_auth_handler const& get_v5_auth_handler() const {
        return h_v5_auth_;
    }


    // Original handlers

    /**
     * @brief Set close handler
     * @param h handler
     */
    void set_close_handler(close_handler h = close_handler()) {
        h_close_ = std::move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = std::move(h);
    }

    /**
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_pub_res_sent_handler(pub_res_sent_handler h = pub_res_sent_handler()) {
        h_pub_res_sent_ = std::move(h);
    }

    /**
     * @brief Set serialize handlers
     * @param h_publish serialize handler for publish message
     * @param h_pubrel serialize handler for pubrel message
     * @param h_remove remove handler for serialized message
     */
    void set_serialize_handlers(
        serialize_publish_message_handler h_publish,
        serialize_pubrel_message_handler h_pubrel,
        serialize_remove_handler h_remove) {
        h_serialize_publish_ = std::move(h_publish);
        h_serialize_pubrel_ = std::move(h_pubrel);
        h_serialize_remove_ = std::move(h_remove);
    }

    /**
     * @brief Set serialize handlers
     * @param h_publish serialize handler for publish message
     * @param h_pubrel serialize handler for pubrel message
     * @param h_remove remove handler for serialized message
     */
    void set_v5_serialize_handlers(
        serialize_v5_publish_message_handler h_publish,
        serialize_v5_pubrel_message_handler h_pubrel,
        serialize_remove_handler h_remove) {
        h_serialize_v5_publish_ = std::move(h_publish);
        h_serialize_v5_pubrel_ = std::move(h_pubrel);
        h_serialize_remove_ = std::move(h_remove);
    }

    /**
     * @brief Set serialize handlers
     * @param h_publish serialize handler for publish message
     * @param h_pubrel serialize handler for pubrel message
     * @param h_remove remove handler for serialized message
     */
    void set_serialize_handlers(
        serialize_publish_handler h_publish,
        serialize_pubrel_handler h_pubrel,
        serialize_remove_handler h_remove) {
        h_serialize_publish_ =
            [h_publish = std::move(h_publish)]
            (basic_publish_message<PacketIdBytes> msg) {
                if (h_publish) {
                    auto buf = continuous_buffer<PacketIdBytes>(msg);
                    h_publish(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_pubrel_ =
            [h_pubrel = std::move(h_pubrel)]
            (basic_pubrel_message<PacketIdBytes> msg) {
                if (h_pubrel) {
                    auto buf = continuous_buffer<PacketIdBytes>(msg);
                    h_pubrel(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_remove_ = std::move(h_remove);
    }

    /**
     * @brief Set serialize handlers
     * @param h_publish serialize handler for publish message
     * @param h_pubrel serialize handler for pubrel message
     * @param h_remove remove handler for serialized message
     */
    void set_v5_serialize_handlers(
        serialize_publish_handler h_publish,
        serialize_pubrel_handler h_pubrel,
        serialize_remove_handler h_remove) {
        h_serialize_v5_publish_ =
            [h_publish = std::move(h_publish)]
            (v5::basic_publish_message<PacketIdBytes> msg) {
                if (h_publish) {
                    auto buf = continuous_buffer<PacketIdBytes>(msg);
                    h_publish(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_v5_pubrel_ =
            [h_pubrel = std::move(h_pubrel)]
            (v5::basic_pubrel_message<PacketIdBytes> msg) {
                if (h_pubrel) {
                    auto buf = continuous_buffer<PacketIdBytes>(msg);
                    h_pubrel(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_remove_ = std::move(h_remove);
    }

    /**
     * @brief Clear serialize handlers
     */
    void set_serialize_handlers() {
        h_serialize_publish_ = serialize_publish_message_handler();
        h_serialize_pubrel_ = serialize_pubrel_message_handler();
        h_serialize_v5_publish_ = serialize_v5_publish_message_handler();
        h_serialize_v5_pubrel_ = serialize_v5_pubrel_message_handler();
        h_serialize_remove_ = serialize_remove_handler();
    }

    /**
     * @brief Set pre-send handler
     * @param h handler
     */
    void set_pre_send_handler(pre_send_handler h = pre_send_handler()) {
        h_pre_send_ = std::move(h);
    }

    /**
     * @brief Set check length handler
     * @param h handler
     */
    void set_is_valid_length_handler(is_valid_length_handler h = is_valid_length_handler()) {
        h_is_valid_length_ = std::move(h);
    }

    void set_packet_bulk_read_limit(std::size_t size) {
        packet_bulk_read_limit_ = size;
    }

    void set_props_bulk_read_limit(std::size_t size) {
        props_bulk_read_limit_ = size;
    }

    /**
     * @brief Get close handler
     * @return handler
     */
    close_handler const& get_close_handler() const {
        return h_close_;
    }

    /**
     * @brief Get error handler
     * @return handler
     */
    error_handler const& get_error_handler() const {
        return h_error_;
    }


    /**
     * @brief Get publish response sent handler
     * @return handler
     */
    pub_res_sent_handler const& get_pub_res_sent_handler() const {
        return h_pub_res_sent_;
    }

    /**
     * @brief Get serialize publish handler
     * @return handler
     */
    serialize_publish_message_handler const& get_serialize_publish_message_handler() const {
        return h_serialize_publish_;
    }

    /**
     * @brief Get serialize pubrel handler
     * @return handler
     */
    serialize_pubrel_message_handler const& get_serialize_pubrel_message_handler() const {
        return h_serialize_pubrel_;
    }

    /**
     * @brief Get serialize publish handler
     * @return handler
     */
    serialize_v5_publish_message_handler const& get_serialize_v5_publish_message_handler() const {
        return h_serialize_v5_publish_;
    }

    /**
     * @brief Get serialize pubrel handler
     * @return handler
     */
    serialize_v5_pubrel_message_handler const& get_serialize_v5_pubrel_message_handler() const {
        return h_serialize_v5_pubrel_;
    }

    /**
     * @brief Get serialize remove handler
     * @return handler
     */
    serialize_remove_handler const& get_serialize_remove_handler() const {
        return h_serialize_remove_;
    }

    /**
     * @brief Get pre-send handler
     * @return handler
     */
    pre_send_handler const& get_pre_send_handler() const {
        return h_pre_send_;
    }

    /**
     * @brief Get check length handler
     * @return handler
     */
    is_valid_length_handler const& get_is_valid_length_handler() const {
        return h_is_valid_length_;
    }


    /**
     * @brief start session with a connected endpoint.
     * @param func finish handler that is called when the session is finished
     *
     */
    void start_session(async_handler_t func = async_handler_t()) {
        async_read_control_packet_type(std::move(func));
    }

    // Blocking APIs

    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void publish_at_most_once(
        mqtt::string_view topic_name,
        mqtt::string_view contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        publish_at_most_once(as::buffer(topic_name.data(), topic_name.size()),
                             as::buffer(contents.data(), contents.size()),
                             retain,
                             std::move(props));
    }

    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     *        3.3.1.3 RETAIN
     */
    void publish_at_most_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        acquired_publish(0, topic_name, contents, mqtt::any(), qos::at_most_once, retain, std::move(props));
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t publish_at_least_once(
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish_at_least_once(topic_buf,
                                     contents_buf,
                                     std::make_pair(std::move(sp_topic), std::move(sp_contents)),
                                     retain,
                                     std::move(props));
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t publish_at_least_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props));
        return packet_id;
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t publish_exactly_once(
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish_exactly_once(topic_buf,
                                    contents_buf,
                                    std::make_pair(std::move(sp_topic), std::move(sp_contents)),
                                    retain,
                                    std::move(props));
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t publish_exactly_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props));
        return packet_id;
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     *
     * @note If you know ahead of time that qos will be at_most_once, then prefer
     *       publish_at_most_once() over publish() as it is slightly more efficent.
     */
    packet_id_t publish(
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if(qos == qos::at_most_once)
        {
            publish_at_most_once(std::move(topic_name), std::move(contents), retain, std::move(props));
            return 0;
        }
        else
        {
            auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
            auto sp_contents = std::make_shared<std::string>(std::move(contents));

            as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
            as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

            packet_id_t packet_id = acquire_unique_packet_id();
            acquired_publish(packet_id, topic_buf, contents_buf, std::make_pair(std::move(sp_topic), std::move(sp_contents)), qos, retain, std::move(props));
            return packet_id;
        }
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     *
     * @note If you know ahead of time that qos will be at_most_once, then prefer
     *       publish_at_most_once() over publish() as it is slightly more efficent.
     */
    packet_id_t publish(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if(qos == qos::at_most_once)
        {
            publish_at_most_once(topic_name, contents, retain, std::move(props));
            return 0;
        }
        else
        {
            packet_id_t packet_id = acquire_unique_packet_id();
            acquired_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props));
            return packet_id;
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
     * @param args
     *        args should be zero or more pairs of topic_name and option.
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename... Args>
    packet_id_t subscribe(
        mqtt::string_view topic_name,
        std::uint8_t option,
        Args&&... args) {
        return subscribe(as::buffer(topic_name.data(), topic_name.size()), option, std::forward<Args>(args)...);
    }

    /**
     * @brief Subscribe
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param args
     *        args should be zero or more pairs of topic_name and option.
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename... Args>
    packet_id_t subscribe(
        as::const_buffer topic_name,
        std::uint8_t option,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_subscribe(packet_id, topic_name, option, std::forward<Args>(args)...);
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param params a vector of the topic_filter and option pair.
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
    packet_id_t subscribe(
        std::vector<std::tuple<mqtt::string_view, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props = {}
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> asio_params;
        asio_params.reserve(params.size());
        for(auto const& p : params)
        {
            asio_params.emplace_back(as::buffer(std::get<0>(p).data(), std::get<0>(p).size()), std::get<1>(p));
        }
        acquired_subscribe(packet_id, std::move(asio_params), std::move(props));
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param params a vector of the topic_filter and option pair.
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
    packet_id_t subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        std::vector<v5::property_variant> props = {}
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_subscribe(packet_id, std::move(params), std::move(props));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param params a collection of topic_filter.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t unsubscribe(
        std::vector<mqtt::string_view> const& params,
        std::vector<v5::property_variant> props = {}
    ) {
        std::vector<as::const_buffer> asio_params;
        asio_params.reserve(params.size());
        for(auto const& p : params)
        {
           asio_params.emplace_back(as::buffer(p.data(), p.size()));
        }
        return unsubscribe(std::move(asio_params), std::move(props));
    }

    /**
     * @brief Unsubscribe
     * @param params a collection of topic_filter.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t unsubscribe(
        std::vector<as::const_buffer> params,
        std::vector<v5::property_variant> props = {}
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_unsubscribe(packet_id, std::move(params), std::move(props));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param args
     *        args should be zero or more topics
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    packet_id_t unsubscribe(
        mqtt::string_view topic_name,
        Args&&... args) {
        return unsubscribe(as::buffer(topic_name.data(), topic_name.size()), std::forward<Args>(args)...);
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param args
     *        args should be zero or more topics
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    packet_id_t unsubscribe(
        as::const_buffer topic_name,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_unsubscribe(packet_id, topic_name, std::forward<Args>(args)...);
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
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            send_disconnect(reason, std::move(props));
        }
    }

    /**
     * @brief Disconnect by endpoint
     * Force disconnect. It is not a clean disconnect sequence.<BR>
     * When the endpoint disconnects using force_disconnect(), a will will send.<BR>
     */
    void force_disconnect() {
        connected_ = false;
        mqtt_connected_ = false;
        shutdown_from_client(*socket_);
    }

    // packet_id manual setting version


    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    bool publish_at_most_once(
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        mqtt::string_view contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        return publish_at_most_once(packet_id,
                                    as::buffer(topic_name.data(), topic_name.size()),
                                    as::buffer(contents.data(), contents.size()),
                                    retain,
                                    std::move(props));
    }

    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     *        3.3.1.3 RETAIN
     */
    bool publish_at_most_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        acquired_publish_at_most_once(packet_id, topic_name, contents, mqtt::any(), retain, std::move(props));
        return true;
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish_at_least_once(packet_id, topic_buf, contents_buf, std::make_pair(std::move(sp_topic), std::move(sp_contents)), retain, std::move(props));
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            acquired_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish_exactly_once(packet_id, topic_buf, contents_buf, std::make_pair(std::move(sp_topic), std::move(sp_contents)), retain, std::move(props));
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            acquired_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish(packet_id, topic_buf, contents_buf, std::make_pair(std::move(sp_topic), std::move(sp_contents)), qos, retain, std::move(props));
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic    = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        as::const_buffer topic_buf    = as::buffer(sp_topic->data(), sp_topic->size());
        as::const_buffer contents_buf = as::buffer(sp_contents->data(), sp_contents->size());

        return publish_dup(packet_id, topic_buf, contents_buf, std::make_pair(std::move(sp_topic), std::move(sp_contents)), qos, retain, std::move(props));
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_publish_dup(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param args
     *        args should be one or more pairs of topic_name and qos.
     *
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    template <typename... Args>
    bool subscribe(
        packet_id_t packet_id,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_subscribe(packet_id, std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param params a vector of the topic_filter and qos pair.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    bool subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<mqtt::string_view, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            std::vector<std::tuple<as::const_buffer, std::uint8_t>> asio_params;
            asio_params.reserve(params.size());
            for(auto const& p : params)
            {
                asio_params.emplace_back(as::buffer(std::get<0>(p).data(), std::get<0>(p).size()), std::get<1>(p));
            }
            acquired_subscribe(packet_id, std::move(asio_params), std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param params a vector of the topic_filter and qos pair.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    bool subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            acquired_subscribe(packet_id, std::move(params), std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param args
     *        args should be one or more topics
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    bool unsubscribe(
        packet_id_t packet_id,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_unsubscribe(packet_id, std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param params a collection of topic_filter
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool unsubscribe(
        packet_id_t packet_id,
        std::vector<mqtt::string_view> const& params,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            std::vector<as::const_buffer> asio_params;
            asio_params.reserve(params.size());
            for(auto const& p : params)
            {
               asio_params.emplace_back(as::buffer(p.data(), p.size()));
            }
            acquired_unsubscribe(packet_id, std::move(asio_params), std::move(props));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param params a collection of topic_filter
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        std::vector<v5::property_variant> props = {}
    ) {
        if (register_packet_id(packet_id)) {
            acquired_unsubscribe(packet_id, std::move(params), std::move(props));
            return true;
        }
        return false;
    }

    // packet_id has already been acquired version

    /**
     * @brief Publish QoS0 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_at_most_once(
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        mqtt::string_view contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        acquired_publish_at_most_once(
            packet_id,
            as::buffer(topic_name.data(), topic_name.size()),
            as::buffer(contents.data(), contents.size()),
            mqtt::any(),
            retain,
            std::move(props)
        );
    }

    /**
     * @brief Publish QoS0 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_at_most_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(packet_id == 0);
        send_publish(
            topic_name,
            qos::at_most_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(life_keeper)
        );
    }
    /**
     * @brief Publish QoS1 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        acquired_publish_at_least_once(
            packet_id,
            topic_buf,
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents)),
            retain,
            std::move(props)
        );
    }

    /**
     * @brief Publish QoS1 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(packet_id != 0);
        send_publish(
            topic_name,
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish QoS2 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        acquired_publish_exactly_once(
            packet_id,
            topic_buf,
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents)),
            retain,
            std::move(props)
        );
    }

    /**
     * @brief Publish QoS2 with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(packet_id != 0);
        send_publish(
            topic_name,
            qos::exactly_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(life_keeper)
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
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        if(qos == qos::at_most_once)
        {
            // In the at_most_once case, we know a priori that send_publish won't track the lifetime.
            acquired_publish(packet_id,
                             as::buffer(topic_name),
                             as::buffer(contents),
                             mqtt::any(),
                             qos,
                             retain,
                             std::move(props));
        }
        else
        {
            auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
            auto sp_contents   = std::make_shared<std::string>(std::move(contents));

            auto topic_buf    = as::buffer(*sp_topic_name);
            auto contents_buf = as::buffer(*sp_contents);

            acquired_publish(packet_id,
                             topic_buf,
                             contents_buf,
                             std::make_pair(std::move(sp_topic_name), std::move(sp_contents)),
                             qos,
                             retain,
                             std::move(props));
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
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        send_publish(
            topic_name,
            qos,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish as dup with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        if(qos == qos::at_most_once)
        {
            // In the at_most_once case, we know a priori that send_publish won't track the lifetime.
            acquired_publish_dup(packet_id,
                                 as::buffer(topic_name),
                                 as::buffer(contents),
                                 mqtt::any(),
                                 qos,
                                 retain,
                                 std::move(props));
        }
        else
        {
            auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
            auto sp_contents   = std::make_shared<std::string>(std::move(contents));

            auto topic_buf    = as::buffer(*sp_topic_name);
            auto contents_buf = as::buffer(*sp_contents);

            acquired_publish_dup(packet_id,
                                 topic_buf,
                                 contents_buf,
                                 std::make_pair(std::move(sp_topic_name), std::move(sp_contents)),
                                 qos,
                                 retain,
                                 std::move(props));
        }
    }

    /**
     * @brief Publish as dup with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     */
    void acquired_publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        std::vector<v5::property_variant> props = {}
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        send_publish(
            topic_name,
            qos,
            retain,
            true,
            packet_id,
            std::move(props),
            contents,
            std::move(life_keeper)
        );
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param args
     *        args should be one or more pairs of topic_name and qos.
     *
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    template <typename... Args>
    void acquired_subscribe(
        packet_id_t packet_id,
        Args&&... args) {
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve(sizeof...(args) / 2);
        send_subscribe(std::move(params), packet_id, std::forward<Args>(args)...);
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
    void acquired_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<mqtt::string_view, std::uint8_t>> params,
        std::vector<v5::property_variant> props = {}
    ) {
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> cb_params;
        cb_params.reserve(params.size());
        for (auto const& e : params) {
            cb_params.emplace_back(as::buffer(std::get<0>(e).data(), std::get<0>(e).size()), std::get<1>(e));
        }
        send_subscribe(std::move(cb_params), packet_id, std::move(props));
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
    void acquired_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        std::vector<v5::property_variant> props = {}
    ) {
        send_subscribe(std::move(params), packet_id, std::move(props));
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param args
     *        args should be one or more topic names to unsubscribe to
     *
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    void acquired_unsubscribe(
        packet_id_t packet_id,
        Args&&... args) {

        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args));

        send_unsubscribe(std::move(params), packet_id, std::forward<Args>(args)...);
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
    void acquired_unsubscribe(
        packet_id_t packet_id,
        std::vector<mqtt::string_view> params,
        std::vector<v5::property_variant> props = {}
    ) {
        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        for (auto const& e : params) {
            cb_params.emplace_back(as::buffer(e.data(), e.size()));
        }
        send_unsubscribe(std::move(cb_params), packet_id, std::move(props));
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
    void acquired_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        std::vector<v5::property_variant> props = {}
    ) {
        send_unsubscribe(std::move(params), packet_id, std::move(props));
    }

    /**
     * @brief Send pingreq packet.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901195
     */
    void pingreq() {
        if (connected_ && mqtt_connected_) send_pingreq();
    }

    /**
     * @brief Send pingresp packet. This function is for broker.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200
     */
    void pingresp() {
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
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        send_auth(reason_code, std::move(props));
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
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
        mqtt::optional<will> const& w,
        std::uint16_t keep_alive_sec,
        std::vector<v5::property_variant> props = {}
    ) {
        connect_requested_ = true;
        send_connect(client_id, user_name, password, w, keep_alive_sec, std::move(props));
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props = {}
    ) {
        send_connack(session_present, reason_code, std::move(props));
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
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        send_puback(packet_id, reason_code, std::move(props));
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
     */
    void pubrec(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        send_pubrec(packet_id, reason_code, std::move(props));
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
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param
     */
    void pubrel(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {},
        mqtt::any life_keeper = mqtt::any()
    ) {
        send_pubrel(packet_id, reason_code, std::move(props), std::move(life_keeper));
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
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        send_pubcomp(packet_id, reason_code, std::move(props));
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param args additional reason_code
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     */
    template <typename... Args>
    void suback(
        packet_id_t packet_id,
        Args&&... args) {
        std::vector<std::uint8_t> params;
        params.reserve(sizeof...(args));
        send_suback(params, packet_id, std::forward<Args>(args)...);
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
        std::vector<std::uint8_t> reasons,
        std::vector<v5::property_variant> props = {}
    ) {
        send_suback(std::move(reasons), packet_id, std::move(props));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     */
    void unsuback(
        packet_id_t packet_id
    ) {
        send_unsuback(packet_id);
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param args additional reason_code
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     */
    template <typename... Args>
    void unsuback(
        packet_id_t packet_id,
        std::uint8_t reason,
        Args&&... args) {
        std::vector<std::uint8_t> params;
        send_unsuback(params, packet_id, reason, std::forward<Args>(args)...);
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
        std::vector<std::uint8_t> reasons,
        std::vector<v5::property_variant> props = {}
    ) {
        send_unsuback(std::move(reasons), packet_id, std::move(props));
    }

    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish_at_most_once(
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, std::move(topic_name), std::move(contents), qos::at_most_once, retain, std::move(func));
    }

    /**
     * @brief Publish QoS0
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish_at_most_once(
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, std::move(topic_name), std::move(contents), qos::at_most_once, retain, std::move(props), std::move(func));
    }

    /**
     * @brief Publish QoS0
     *        topic_name and contents are reference type. So caller need to keep the lifetime of them
     *        until func is called.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish_at_most_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, topic_name, contents, std::move(life_keeper), qos::at_most_once, retain, std::move(func));
    }

    /**
     * @brief Publish QoS0
     *        topic_name and contents are reference type. So caller need to keep the lifetime of them
     *        until func is called.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void async_publish_at_most_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, topic_name, contents, std::move(life_keeper), qos::at_most_once, retain, std::move(props), std::move(func));
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_at_least_once(
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_at_least_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_at_least_once(
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_at_least_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_at_least_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS1
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_at_least_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_exactly_once(
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_exactly_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_exactly_once(
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_exactly_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_exactly_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish QoS2
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id
     * packet_id is automatically generated.
     */
    packet_id_t async_publish_exactly_once(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     */
    packet_id_t async_publish(
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        packet_id_t packet_id = qos == qos::at_most_once ? 0 : acquire_unique_packet_id();
        acquired_async_publish(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     */
    packet_id_t async_publish(
        std::string topic_name,
        std::string contents,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        packet_id_t packet_id = qos == qos::at_most_once ? 0 : acquire_unique_packet_id();
        acquired_async_publish(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     */
    packet_id_t async_publish(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        packet_id_t packet_id = qos == qos::at_most_once ? 0 : acquire_unique_packet_id();
        acquired_async_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(func));
        return packet_id;
    }

    /**
     * @brief Publish
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id. If qos is set to at_most_once, return 0.
     * packet_id is automatically generated.
     */
    packet_id_t async_publish(
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        packet_id_t packet_id = qos == qos::at_most_once ? 0 : acquire_unique_packet_id();
        acquired_async_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename Arg0, typename... Args>
    packet_id_t async_subscribe(
        std::string topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, std::move(topic_name), option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename Arg0, typename... Args>
    packet_id_t async_subscribe(
        as::const_buffer topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, topic_name, option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        return packet_id;
    }

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
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        std::string topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, std::move(topic_name), option, std::move(func));
        return packet_id;
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
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */

    packet_id_t async_subscribe(
        std::string topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, std::move(topic_name), option, std::move(props), std::move(func));
        return packet_id;
    }

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
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        as::const_buffer topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, topic_name, option, std::move(func));
        return packet_id;
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
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        as::const_buffer topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, topic_name, option, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        std::vector<std::tuple<std::string, std::uint8_t>> const& params,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, params, std::move(func));
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param params
     * A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        std::vector<std::tuple<std::string, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, params, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Subscribe
     * @param params
     *        A collection of the pair of topic_name and option to subscribe.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, params, std::move(func));
        return packet_id;
    }

    /**
     * @brief Subscribe
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
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    packet_id_t async_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_subscribe(packet_id, params, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename Arg0, typename... Args>
    packet_id_t async_unsubscribe(
        std::string topic_name,
        Arg0&& arg0,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, std::move(topic_name), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename Arg0, typename... Args>
    packet_id_t async_unsubscribe(
        as::const_buffer topic_name,
        Arg0&& arg0,
        Args&&... args) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, topic_name, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::string topic_name,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, std::move(topic_name), std::move(func));
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
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::string topic_name,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, std::move(topic_name), std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param topic_name
     *        A topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        as::const_buffer topic_name,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, topic_name, std::move(func));
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
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        as::const_buffer topic_name,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, topic_name, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::vector<std::string> const& params,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, params, std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::vector<std::string> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, params, std::move(props), std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::vector<as::const_buffer> const& params,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, params, std::move(func));
        return packet_id;
    }

    /**
     * @brief Unsubscribe
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return packet_id.
     * packet_id is automatically generated.<BR>
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    packet_id_t async_unsubscribe(
        std::vector<as::const_buffer> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_async_unsubscribe(packet_id, params, std::move(props), std::move(func));
        return packet_id;
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
        async_handler_t func = async_handler_t()
    ) {
        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            async_send_disconnect(std::move(func));
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
        std::uint8_t reason,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (connected_ && mqtt_connected_) {
            disconnect_requested_ = true;
            async_send_disconnect(reason, std::move(props), std::move(func));
        }
    }

    // packet_id manual setting version

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_at_least_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_at_least_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_at_least_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_exactly_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_exactly_once(packet_id, std::move(topic_name), std::move(contents), retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents doesn't publish, otherwise return true and contents publish.
     */
    bool async_publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_publish_exactly_once(packet_id, topic_name, contents, std::move(life_keeper), retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param life_keeper A object that stays alive until the async operation is finished.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish_dup(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish_dup(packet_id, std::move(topic_name), std::move(contents), qos, retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param life_keeper A object that stays alive until the async operation is finished.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish_dup(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         contents don't publish, otherwise return true and contents publish.
     */
    bool async_publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_async_publish_dup(packet_id, topic_name, contents, std::move(life_keeper), qos, retain, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    template <typename Arg0, typename... Args>
    bool async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, std::move(topic_name), option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    template <typename Arg0, typename... Args>
    bool async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, topic_name, option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, std::move(topic_name), option, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
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
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, std::move(topic_name), option, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
     * @param option
     *        subscription options<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, topic_name, option, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
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
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, topic_name, option, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param params A collection of the pair of topic_name and option to subscribe.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, std::uint8_t>> const& params,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, params, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param params A collection of the pair of topic_name and option to subscribe.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, params, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param params A collection of the pair of topic_name and option to subscribe.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, params, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Subscribe
     * @param packet_id
     *        packet identifier
     * @param params A collection of the pair of topic_name and option to subscribe.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901169<BR>
     *        3.8.3.1 Subscription Options
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't subscribe, otherwise return true and subscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    bool async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_subscribe(packet_id, params, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename Arg0, typename... Args>
    bool async_unsubscribe(
        packet_id_t packet_id,
        std::string  topic_name,
        Arg0&& arg0,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, std::move(topic_name), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier
     * @param topic_name
     *        A topic name to subscribe
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename Arg0, typename... Args>
    bool async_unsubscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Arg0&& arg0,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, topic_name, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> const& params,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, params, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier
     * @param params
     *        A collection of the topic names to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, params, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier
     * @param params
     *        A collection of the topic names to unsubscribe
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> const& params,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, params, std::move(func));
            return true;
        }
        return false;
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier
     * @param params
     *        A collection of the topic name to unsubscribe
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * @return If packet_id is used in the publishing/subscribing sequence, then returns false and
     *         doesn't unsubscribe, otherwise return true and unsubscribes.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    bool async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> const& params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        if (register_packet_id(packet_id)) {
            acquired_async_unsubscribe(packet_id, params, std::move(props), std::move(func));
            return true;
        }
        return false;
    }

    // packet_id has already been acquired version

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        async_send_publish(
            as::buffer(*sp_topic_name),
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            as::buffer(*sp_contents),
            std::move(func),
            [sp_topic_name, sp_contents] {}
        );
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_at_least_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        async_send_publish(
            as::buffer(*sp_topic_name),
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::move(props),
            as::buffer(*sp_contents),
            std::move(func),
            [sp_topic_name, sp_contents] {}
        );
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {

        async_send_publish(
            topic_name,
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish QoS1 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_at_least_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        async_send_publish(
            topic_name,
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents = std::make_shared<std::string>(std::move(contents));

        async_send_publish(
            as::buffer(*sp_topic_name),
            qos::exactly_once,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            as::buffer(*sp_contents),
            std::move(func),
            [sp_topic_name, sp_contents] {}
        );
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_exactly_once(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf   = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        async_send_publish(
            topic_buf,
            qos::at_least_once,
            retain,
            false,
            std::move(props),
            packet_id,
            contents_buf,
            std::move(func),
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
        );
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {

        async_send_publish(
            topic_name,
            qos::exactly_once,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish QoS2 with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_exactly_once(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        async_send_publish(
            topic_name,
            qos::exactly_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(func),
            std::move(life_keeper)
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
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(topic_name);
        auto sp_contents   = std::make_shared<std::string>(contents);

        auto topic_buf   = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        async_send_publish(
            topic_buf,
            qos,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            contents_buf,
            std::move(func),
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
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
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        async_send_publish(
            topic_buf,
            qos,
            retain,
            false,
            packet_id,
            std::move(props),
            contents_buf,
            std::move(func),
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
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
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        async_send_publish(
            topic_name,
            qos,
            retain,
            false,
            packet_id,
            std::vector<v5::property_variant>{},
            contents,
            std::move(func),
            std::move(life_keeper)
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
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        async_send_publish(
            topic_name,
            qos,
            retain,
            false,
            packet_id,
            std::move(props),
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(topic_name);
        auto sp_contents = std::make_shared<std::string>(contents);

        async_send_publish(
            as::buffer(*sp_topic_name),
            qos,
            retain,
            true,
            packet_id,
            std::vector<v5::property_variant>{},
            as::buffer(*sp_contents),
            std::move(func),
            [sp_topic_name, sp_contents] {}
        );
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_dup(
        packet_id_t packet_id,
        std::string topic_name,
        std::string contents,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        async_send_publish(
            topic_buf,
            qos,
            retain,
            true,
            packet_id,
            std::move(props),
            contents_buf,
            std::move(func),
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
        );
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param life_keeper A object that stays alive until the async operation is finished.
     */
    void acquired_async_publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos = qos::at_most_once,
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        async_send_publish(
            topic_name,
            qos,
            retain,
            true,
            packet_id,
            std::vector<v5::property_variant>{},
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Publish as dup with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     *        If qos == qos::at_most_once, packet_id must be 0. But not checked in release mode due to performance.
     * @param topic_name
     *        A topic name to publish
     * @param contents
     *        The contents to publish
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * @param qos
     *        mqtt::qos
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901104<BR>
     *        3.3.1.3 RETAIN
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901109<BR>
     *        3.3.2.3 PUBLISH Properties
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     */
    void acquired_async_publish_dup(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        as::const_buffer contents,
        mqtt::any life_keeper,
        std::uint8_t qos,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        async_send_publish(
            topic_name,
            qos,
            retain,
            true,
            packet_id,
            std::move(props),
            contents,
            std::move(func),
            std::move(life_keeper)
        );
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * You can subscribe multiple topics all at once.<BR>
     */
    template <typename Arg0, typename... Args>
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        acquired_async_subscribe_imp(packet_id, std::move(topic_name), option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

    /**
     * @brief Subscribe with a manual set packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
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
     *        You can set a callback function that is called when async operation will finish.
     * You can subscribe multiple topics all at once.<BR>
     */
    template <typename Arg0, typename... Args>
    void acquired_async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        Arg0&& arg0,
        Args&&... args) {
        acquired_async_subscribe_imp(packet_id, topic_name, option, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto topic_buf = as::buffer(*sp_topic_name);

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, std::uint8_t>>({std::make_tuple(topic_buf, option)}),
            std::move(sp_topic_name),
            packet_id,
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto topic_buf = as::buffer(*sp_topic_name);

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, std::uint8_t>>({std::make_tuple(topic_buf, option)}),
            std::move(sp_topic_name),
            packet_id,
            std::move(props),
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        async_handler_t func = async_handler_t()
    ) {
        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, std::uint8_t>>({std::make_tuple(topic_name, option)}),
            mqtt::any(),
            packet_id,
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t option,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        async_send_subscribe(
            std::vector<std::tuple<as::const_buffer, std::uint8_t>>({std::make_tuple(topic_name, option)}),
            mqtt::any(),
            packet_id,
            std::move(props),
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, std::uint8_t>> params,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto const& e : params) {
            auto sp_topic_name = std::make_shared<std::string>(std::move(std::get<0>(e)));
            cb_params.emplace_back(as::buffer(*sp_topic_name), std::get<1>(e));
            life_keepers.emplace_back(std::move(sp_topic_name));
        }
        async_send_subscribe(
            std::move(cb_params),
            std::move(life_keepers),
            packet_id,
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<std::string, std::uint8_t>> params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto const& e : params) {
            auto sp_topic_name = std::make_shared<std::string>(std::move(std::get<0>(e)));
            cb_params.emplace_back(as::buffer(*sp_topic_name), std::get<1>(e));
            life_keepers.emplace_back(std::move(sp_topic_name));
        }
        async_send_subscribe(
            std::move(cb_params),
            life_keepers,
            packet_id,
            std::move(props),
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        async_handler_t func = async_handler_t()
    ) {
        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(func)
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
    void acquired_async_subscribe(
        packet_id_t packet_id,
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(props),
            std::move(func)
        );
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename Arg0, typename... Args>
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        std::string topic_name,
        Arg0&& arg0,
        Args&&... args) {
        acquired_async_unsubscribe_imp(packet_id, std::move(topic_name), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

    /**
     * @brief Unsubscribe
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name topic_name
     * @param args
     *        The format of args is `[topic_name, topicname, ... , ][props,][func]`<BR>
     *        args should be some topic_names to unsubscribe, <BR>
     *        You can set props optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     *        You can set a callback function that is called when async operation will finish.
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161
     */
    template <typename Arg0, typename... Args>
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Arg0&& arg0,
        Args&&... args) {
        acquired_async_unsubscribe_imp(packet_id, topic_name, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
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
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> params,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto const& e : params) {
            life_keepers.emplace_back(std::make_shared<std::string>(std::move(e)));
            cb_params.emplace_back(as::buffer(*life_keepers.back()));
        }

        async_send_unsubscribe(
            std::move(cb_params),
            std::move(life_keepers),
            packet_id,
            std::move(func)
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
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        std::vector<std::string> params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<as::const_buffer> cb_params;
        cb_params.reserve(params.size());

        std::vector<std::shared_ptr<std::string>> life_keepers;
        life_keepers.reserve(params.size());

        for (auto const& e : params) {
            life_keepers.emplace_back(std::make_shared<std::string>(std::move(e)));
            cb_params.emplace_back(as::buffer(*life_keepers.back()));
        }

        async_send_unsubscribe(
            std::move(cb_params),
            life_keepers,
            packet_id,
            std::move(props),
            std::move(func)
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
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<std::shared_ptr<std::string>> life_keepers;
        async_send_unsubscribe(
            std::move(params),
            life_keepers,
            packet_id,
            std::move(func)
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
    void acquired_async_unsubscribe(
        packet_id_t packet_id,
        std::vector<as::const_buffer> params,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {

        std::vector<std::shared_ptr<std::string>> life_keepers;
        async_send_unsubscribe(
            std::move(params),
            life_keepers,
            packet_id,
            std::move(props),
            std::move(func)
        );
    }

    /**
     * @brief Send pingreq packet.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901195
     */
    void async_pingreq(async_handler_t func = async_handler_t()) {
        if (connected_ && mqtt_connected_) async_send_pingreq(std::move(func));
    }

    /**
     * @brief Send pingresp packet. This function is for broker.
     * @param func
     *        functor object who's operator() will be called when the async operation completes.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200
     */
    void async_pingresp(async_handler_t func = async_handler_t()) {
        async_send_pingresp(std::move(func));
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
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {},
        async_handler_t func = async_handler_t()) {
        async_send_auth(reason_code, std::move(props), std::move(func));
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
        std::string const& client_id,
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
        mqtt::optional<will> const& w,
        std::uint16_t keep_alive_sec,
        async_handler_t func = async_handler_t()
    ) {
        async_connect(client_id,
                      user_name,
                      password,
                      w,
                      keep_alive_sec,
                      std::vector<v5::property_variant>{},
                      std::move(func));
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
        std::string const& client_id,
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
        mqtt::optional<will> const& w,
        std::uint16_t keep_alive_sec,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        connect_requested_ = true;
        async_send_connect(client_id,
                           user_name,
                           password,
                           w,
                           keep_alive_sec,
                           std::move(props),
                           std::move(func));
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
        std::uint8_t reason_code,
        async_handler_t func = async_handler_t()
    ) {
        async_send_connack(session_present, reason_code, std::vector<v5::property_variant>{}, std::move(func));
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_connack(session_present, reason_code, std::move(props), std::move(func));
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
        async_handler_t func = async_handler_t()
    ) {
        async_send_puback(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, std::move(func));
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_puback(packet_id, reason_code, std::move(props), std::move(func));
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
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubrec(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, std::move(func));
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubrec(packet_id, reason_code, std::move(props), std::move(func));
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
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubrel(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, std::move(func));
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
     * @param life_keeper
     *        An object that stays alive (but is moved with std::move()) until the async operation is finished.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubrel(
        packet_id_t packet_id,
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t(),
        mqtt::any life_keeper = mqtt::any()
    ) {
        async_send_pubrel(packet_id, reason_code, std::move(props), std::move(func), std::move(life_keeper));
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
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubcomp(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, std::move(func));
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubcomp(packet_id, reason_code, std::move(props), std::move(func));
    }

    /**
     * @brief Send suback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901178<BR>
     *        3.9.3 SUBACK Payload
     * @param args additional reason_code
     *        The format of args is `[option, option, ...,][props,][func]`<BR>
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901174<BR>
     *        3.9.2.1 SUBACK Properties
     *        You can set a callback function that is called when async operation will finish.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    template <typename Arg0, typename... Args>
    void async_suback(
        packet_id_t packet_id,
        std::uint8_t reason,
        Arg0&& arg0,
        Args&&... args) {
        async_suback_imp(packet_id, reason, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
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
        std::uint8_t reason,
        async_handler_t func = async_handler_t()
    ) {
        async_send_suback(std::vector<std::uint8_t>{}, packet_id, reason, std::vector<v5::property_variant>{}, std::move(func));
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
        std::uint8_t reason,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_suback(std::vector<std::uint8_t>{}, packet_id, reason, std::move(props), std::move(func));
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
        std::vector<std::uint8_t> reasons,
        async_handler_t func = async_handler_t()
    ) {
        async_send_suback(std::move(reasons), packet_id, std::vector<v5::property_variant>{}, std::move(func));
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
        std::vector<std::uint8_t> reasons,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_suback(std::move(reasons), packet_id, std::move(props), std::move(func));
    }

    /**
     * @brief Send unsuback packet. This function is for broker.
     * @param packet_id packet id corresponding to subscribe
     * @param reason
     *        reason_code<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901194<BR>
     *        3.11.3 UNSUBACK Payload
     * @param args
     *        The format of args is `[option, option, ...,][props,][func]`<BR>
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901190<BR>
     *        3.11.2.1 UNSUBACK Properties
     *        You can set a callback function that is called when async operation will finish.
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718068
     */
    template <typename Arg0, typename... Args>
    void async_unsuback(
        packet_id_t packet_id,
        std::uint8_t reason,
        Arg0&& arg0,
        Args&&... args) {
        async_unsuback_imp(packet_id, reason, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
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
        std::uint8_t reason,
        async_handler_t func = async_handler_t()
    ) {
        async_send_unsuback(std::vector<std::uint8_t>{}, packet_id, reason, std::move(func));
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
        std::uint8_t reason,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_unsuback(std::vector<std::uint8_t>{}, packet_id, reason, std::move(props), std::move(func));
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
        std::vector<std::uint8_t> reasons,
        async_handler_t func = async_handler_t()
    ) {
        async_send_unsuback(std::move(reasons), packet_id, std::vector<v5::property_variant>{}, std::move(func));
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
        std::vector<std::uint8_t> reasons,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_unsuback(std::move(reasons), packet_id, std::move(props), std::move(func));
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
        async_handler_t func = async_handler_t()) {
        async_send_unsuback(packet_id, std::move(func));
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
     * @param f applying function. f should be void(message_variant const&)
     */
    void for_each_store(std::function<void(message_variant const&)> const& f) {
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
     * @param packet_id packet id of the message
     * @param b         iterator begin of the message
     * @param e         iterator end of the message
     */
    template <typename Iterator>
    typename std::enable_if<std::is_convertible<typename Iterator::value_type, char>::value>::type
    restore_serialized_message(packet_id_t /*packet_id*/, Iterator b, Iterator e) {
        static_assert(
            std::is_same<
                typename std::iterator_traits<Iterator>::iterator_category,
                std::random_access_iterator_tag
            >::value,
            "Iterator doesn't support random access"
        );

        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        switch (get_control_packet_type(fixed_header)) {
        case control_packet_type::publish: {
            auto size = static_cast<std::size_t>(std::distance(b, e));
            auto spa = make_shared_ptr_array(size);
            std::copy(b, e, spa.get());
            restore_serialized_message(
                basic_publish_message<PacketIdBytes>(
                    buffer(string_view(spa.get(), size))
                ),
                spa
            );
        } break;
        case control_packet_type::pubrel: {
            restore_serialized_message(
                basic_pubrel_message<PacketIdBytes>(
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
     *        An object that stays alive (but is moved with std::move()) until the stored message is sent.
     */
    void restore_serialized_message(basic_publish_message<PacketIdBytes> msg, mqtt::any life_keeper) {
        auto packet_id = msg.packet_id();
        auto qos = msg.qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                qos == qos::at_least_once ? control_packet_type::puback
                                          : control_packet_type::pubrec,
                std::move(msg),
                std::move(life_keeper)
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
                            std::move(msg),
                            std::move(life_keeper)
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
    void restore_serialized_message(basic_pubrel_message<PacketIdBytes> msg) {
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                control_packet_type::pubcomp,
                std::move(msg)
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
                            std::move(msg)
                        );
                    }
                );
            }
        }
    }

    /**
     * @brief Restore serialized publish and pubrel messages.
     *        This function shouold be called before connect.
     * @param packet_id packet id of the message
     * @param b         iterator begin of the message
     * @param e         iterator end of the message
     */
    template <typename Iterator>
    typename std::enable_if<std::is_convertible<typename Iterator::value_type, char>::value>::type
    restore_v5_serialized_message(packet_id_t /*packet_id*/, Iterator b, Iterator e) {
        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        switch (get_control_packet_type(fixed_header)) {
        case control_packet_type::publish: {
            auto size = static_cast<std::size_t>(std::distance(b, e));
            auto spa = make_shared_ptr_array(size);
            std::copy(b, e, spa.get());
            restore_v5_serialized_message(
                v5::basic_publish_message<PacketIdBytes>(
                    buffer(string_view(spa.get(), size))
                ),
                spa
            );
        } break;
        case control_packet_type::pubrel: {
            auto size = static_cast<std::size_t>(std::distance(b, e));
            auto spa = make_shared_ptr_array(size);
            std::copy(b, e, spa.get());
            restore_v5_serialized_message(
                v5::basic_pubrel_message<PacketIdBytes>(
                    buffer(string_view(spa.get(), size))
                ),
                spa
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
     *        An object that stays alive (but is moved with std::move()) until the stored message is sent.
     */
    void restore_v5_serialized_message(v5::basic_publish_message<PacketIdBytes> msg, mqtt::any life_keeper) {
        auto packet_id = msg.packet_id();
        auto qos = msg.qos();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                qos == qos::at_least_once ? control_packet_type::puback
                                          : control_packet_type::pubrec,
                std::move(msg),
                std::move(life_keeper)
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
                            std::move(msg),
                            std::move(life_keeper)
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
     *        An object that stays alive (but is moved with std::move()) until the stored message is sent.
     */
    void restore_v5_serialized_message(v5::basic_pubrel_message<PacketIdBytes> msg, mqtt::any life_keeper) {
        auto packet_id = msg.packet_id();
        LockGuard<Mutex> lck (store_mtx_);
        if (packet_id_.insert(packet_id).second) {
            auto ret = store_.emplace(
                packet_id,
                control_packet_type::pubcomp,
                std::move(msg),
                std::move(life_keeper)
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
                            std::move(msg)
                        );
                    }
                );
            }
        }
    }

    /**
     * @brief Check connection status
     * @return current connection status
     */
    bool connected() const {
        return connected_ && mqtt_connected_;
    }

    /**
     * @brief Set custom mqtt_message_processed_handler.
     *        The default setting is calling async_read_control_packet_type().
     *        (See endpoint() constructor).
     *        The typical usecase of this function is delaying the next
     *        message reading timing.
     *        In order to do that
     *        1) store func parameter of the mqtt_message_processed_handler.
     *        2) call async_read_next_message with the stored func if
     *           you are ready to read the next mqtt message.
     * @param h mqtt_message_processed_handler.
     */
    void set_mqtt_message_processed_handler(
        mqtt_message_processed_handler h = mqtt_message_processed_handler()) {
        if (h) {
            h_mqtt_message_processed_ = std::move(h);
        }
        else {
            h_mqtt_message_processed_ =
                [this]
                (async_handler_t func) {
                    async_read_control_packet_type(std::move(func));
            };
        }
    }

    /**
     * @brief Get  mqtt_message_processed_handler.
     * @return mqtt_message_processed_handler.
     */
    mqtt_message_processed_handler get_mqtt_message_processed_handler() const {
        return h_mqtt_message_processed_;
    }

    /**
     * @brief Trigger next mqtt message manually.
     *        If you call this function, you need to set manual receive mode
     *        using set_auto_next_read(false);
     */
    void async_read_next_message(async_handler_t func) {
        async_read_control_packet_type(std::move(func));
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

    mqtt::socket const& socket() const {
        return socket_.value();
    }

    mqtt::socket& socket() {
        return socket_.value();
    }

protected:

    /**
     * @brief Get shared_any of socket
     * @return reference of shared_any socket
     */
    mqtt::optional<mqtt::socket>& socket_optional() {
        return socket_;
    }

    void async_read_control_packet_type(async_handler_t func) {
        auto self = shared_from_this();
        socket_->async_read(
            as::buffer(&buf_[0], 1),
            [this, self = std::move(self), func = std::move(func)](
                boost::system::error_code const& ec,
                std::size_t bytes_transferred) {
                if (!check_error_and_transferred_length(ec, func, bytes_transferred, 1)) return;
                handle_control_packet_type(std::move(func), std::move(self));
            }
        );
    }

    bool handle_close_or_error(boost::system::error_code const& ec) {
        if (!ec) return false;
        if (connected_) {
            connected_ = false;
            mqtt_connected_ = false;
            {
                boost::system::error_code ec;
                socket_->close(ec);
            }
        }
        if (ec == as::error::eof ||
            ec == as::error::connection_reset
#if defined(MQTT_USE_WS)
            ||
            ec == boost::beast::websocket::error::closed
#endif // defined(MQTT_USE_WS)
#if !defined(MQTT_NO_TLS)
            ||
#if defined(SSL_R_SHORT_READ)
            ERR_GET_REASON(ec.value()) == SSL_R_SHORT_READ
#else  // defined(SSL_R_SHORT_READ)
            ERR_GET_REASON(ec.value()) == boost::asio::ssl::error::stream_truncated
#endif // defined(SSL_R_SHORT_READ)
#endif // defined(MQTT_NO_TLS)
        ) {
            if (disconnect_requested_) {
                disconnect_requested_ = false;
                connect_requested_ = false;
                handle_close();
                return true;
            }
        }
        disconnect_requested_ = false;
        connect_requested_ = false;
        handle_error(ec);
        return true;
    }

    void set_connect() {
        connected_ = true;
    }

    void set_protocol_version(std::size_t version) {
        version_ = version;
    }

    void handle_close() {
        if (h_close_) h_close_();
    }

    void handle_error(boost::system::error_code const& ec) {
        if (h_error_) h_error_(ec);
    }

private:

    bool check_error(
        boost::system::error_code const& ec,
        async_handler_t const& func) {
        if (handle_close_or_error(ec)) {
            if (func) func(ec);
            return false;
        }
        return true;
    }

    bool check_transferred_length(
        std::size_t bytes_transferred,
        std::size_t bytes_expected,
        async_handler_t const& func) {
        if (bytes_transferred != bytes_expected) {
            call_message_size_error_handlers(func);
            return false;
        }
        return true;
    }

    bool check_error_and_transferred_length(
        boost::system::error_code const& ec,
        async_handler_t const& func,
        std::size_t bytes_transferred,
        std::size_t bytes_expected) {
        if (!check_error(ec, func)) return false;
        if (!check_transferred_length(bytes_transferred, bytes_expected, func)) return false;
        return true;
    }

    void call_error_handlers(boost::system::error_code const& ec, async_handler_t const& func) {
        handle_error(ec);
        if (func) func(ec);
    }

    void call_message_size_error_handlers(async_handler_t const& func) {
        call_error_handlers(
            boost::system::errc::make_error_code(boost::system::errc::message_size),
            func
        );
    }

    void call_protocol_error_handlers(async_handler_t const& func) {
        call_error_handlers(
            boost::system::errc::make_error_code(boost::system::errc::protocol_error),
            func
        );
    }

    template <typename T>
    void shutdown_from_client(T& socket) {
        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_subscribe_imp(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t qos,
        Args&&... args) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);

        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(topic_name),
            qos,
            std::forward<Args>(args)...
        );
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_subscribe_imp(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);

        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            topic_name,
            qos,
            std::forward<Args>(args)...
        );
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_subscribe_imp(
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t qos,
        Args&&... args) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);

        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(topic_name),
            qos,
            std::forward<Args>(args)...,
            async_handler_t()
        );
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_subscribe_imp(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {

        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);

        async_send_subscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            topic_name,
            qos,
            std::forward<Args>(args)...,
            async_handler_t()
        );
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_unsubscribe_imp(
        packet_id_t packet_id,
        std::string topic_name,
        Args&&... args) {

        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args));

        async_send_unsubscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(topic_name),
            std::forward<Args>(args)...
        );
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_unsubscribe_imp(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {

        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args));

        async_send_unsubscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            topic_name,
            std::forward<Args>(args)...
        );
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_unsubscribe_imp(
        packet_id_t packet_id,
        std::string topic_name,
        Args&&... args) {

        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args) + 1);

        async_send_unsubscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            std::move(topic_name),
            std::forward<Args>(args)...,
            async_handler_t()
        );
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    acquired_async_unsubscribe_imp(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {

        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args) + 1);

        async_send_unsubscribe(
            std::move(params),
            mqtt::any(),
            packet_id,
            topic_name,
            std::forward<Args>(args)...,
            async_handler_t()
        );
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    async_suback_imp(
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        async_send_suback(std::vector<std::uint8_t>{}, packet_id, qos, std::forward<Args>(args)...);
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    async_suback_imp(
        packet_id_t packet_id,
        Args&&... qos) {
        async_send_suback(std::vector<std::uint8_t>({qos...}), packet_id, async_handler_t());
    }

    template <typename... Args>
    typename std::enable_if<
        std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    async_unsuback_imp(
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        async_send_unsuback(std::vector<std::uint8_t>{}, packet_id, qos, std::forward<Args>(args)...);
    }

    template <typename... Args>
    typename std::enable_if<
        !std::is_convertible<
            typename std::tuple_element<sizeof...(Args) - 1, std::tuple<Args...>>::type,
            async_handler_t
        >::value
    >::type
    async_unsuback_imp(
        packet_id_t packet_id,
        Args&&... qos) {
        async_send_unsuback(std::vector<std::uint8_t>({qos...}), packet_id, async_handler_t());
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
        static constexpr std::size_t const payload_position_ = 5;
        std::shared_ptr<std::string> buf_;
    };

    struct store {
        store(
            packet_id_t id,
            std::uint8_t type,
            basic_store_message_variant<PacketIdBytes> smv,
            mqtt::any life_keeper = mqtt::any())
            : packet_id_(id)
            , expected_control_packet_type_(type)
            , smv_(std::move(smv))
            , life_keeper_(std::move(life_keeper)) {}
        packet_id_t packet_id() const { return packet_id_; }
        std::uint8_t expected_control_packet_type() const { return expected_control_packet_type_; }
        basic_message_variant<PacketIdBytes> message() const {
            return get_basic_message_variant<PacketIdBytes>(smv_);
        }
    private:
        packet_id_t packet_id_;
        std::uint8_t expected_control_packet_type_;
        basic_store_message_variant<PacketIdBytes> smv_;
        mqtt::any life_keeper_;
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
                        store, std::uint8_t,
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

    void handle_control_packet_type(async_handler_t func, this_type_sp self) {
        fixed_header_ = static_cast<std::uint8_t>(buf_[0]);
        remaining_length_ = 0;
        remaining_length_multiplier_ = 1;
        socket_->async_read(
            as::buffer(&buf_[0], 1),
            [this, self = std::move(self), func = std::move(func)](
                boost::system::error_code const& ec,
                std::size_t bytes_transferred){
                if (!check_error_and_transferred_length(ec, func, bytes_transferred, 1)) return;
                handle_remaining_length(std::move(func), std::move(self));
            }
        );
    }

    void handle_remaining_length(async_handler_t func, this_type_sp self) {
        remaining_length_ += (buf_[0] & 0b01111111) * remaining_length_multiplier_;
        remaining_length_multiplier_ *= 128;
        if (remaining_length_multiplier_ > 128 * 128 * 128 * 128) {
            handle_error(boost::system::errc::make_error_code(boost::system::errc::message_size));
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return;
        }
        if (buf_[0] & 0b10000000) {
            socket_->async_read(
                as::buffer(&buf_[0], 1),
                [self = std::move(self), func = std::move(func)](
                    boost::system::error_code const& ec,
                    std::size_t bytes_transferred){
                    if (self->handle_close_or_error(ec)) {
                        if (func) func(ec);
                        return;
                    }
                    if (bytes_transferred != 1) {
                        self->handle_error(boost::system::errc::make_error_code(boost::system::errc::message_size));
                        if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                        return;
                    }
                    self->handle_remaining_length(std::move(func), std::move(self));
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
                            if (h_is_valid_length_) {
                                return h_is_valid_length_(cpt, remaining_length_);
                            }
                            else {
                                return true;
                            }
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
                        default:
                            return false;
                        }
                        break;
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
                            if (h_is_valid_length_) {
                                return h_is_valid_length_(cpt, remaining_length_);
                            }
                            else {
                                return true;
                            }
                        case control_packet_type::pingreq:
                        case control_packet_type::pingresp:
                            return remaining_length_ == 0;
                        default:
                            return false;
                        }
                        break;
                    }
                };
            if (!check()) {
                handle_error(boost::system::errc::make_error_code(boost::system::errc::message_size));
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return;
            }

            process_payload(std::move(func), std::move(self));
        }
    }

    void process_payload(async_handler_t func, this_type_sp self) {
        auto control_packet_type = get_control_packet_type(fixed_header_);
        switch (control_packet_type) {
        case control_packet_type::connect:
            process_connect(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            return;
        case control_packet_type::connack:
            process_connack(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            return;
        case control_packet_type::publish:
            if (mqtt_connected_) {
                process_publish(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::puback:
            if (mqtt_connected_) {
                process_puback(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::pubrec:
            if (mqtt_connected_) {
                process_pubrec(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::pubrel:
            if (mqtt_connected_) {
                process_pubrel(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::pubcomp:
            if (mqtt_connected_) {
                process_pubcomp(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::subscribe:
            if (mqtt_connected_) {
                process_subscribe(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::suback:
            if (mqtt_connected_) {
                process_suback(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::unsubscribe:
            if (mqtt_connected_) {
                process_unsubscribe(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::unsuback:
            if (mqtt_connected_) {
                process_unsuback(std::move(func), remaining_length_ < packet_bulk_read_limit_, std::move(self));
            }
            break;
        case control_packet_type::pingreq:
            if (mqtt_connected_) {
                process_pingreq(std::move(func));
            }
            break;
        case control_packet_type::pingresp:
            if (mqtt_connected_) {
                process_pingresp(std::move(func));
            }
            break;
        case control_packet_type::disconnect:
            process_disconnect(func, remaining_length_ < packet_bulk_read_limit_, std::move(self));
            break;
        case control_packet_type::auth:
            process_auth(func, remaining_length_ < packet_bulk_read_limit_, std::move(self));
            break;
        default:
            break;
        }
    }

    // primitive read functions
    void process_nbytes(
        async_handler_t func,
        buffer buf,
        std::size_t size,
        std::function<void(buffer, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        if (remaining_length_ < size) {
            call_message_size_error_handlers(func);
            return;
        }
        remaining_length_ -= size;

        if (buf.empty()) {
            auto spa = make_shared_ptr_array(size);
            socket_->async_read(
                as::buffer(spa.get(), size),
                [
                    this,
                    self = std::move(self),
                    func = std::move(func),
                    size,
                    handler = std::move(handler),
                    spa
                ]
                (boost::system::error_code const& ec,
                 std::size_t bytes_transferred) mutable {
                    if (!check_error_and_transferred_length(ec, func, bytes_transferred, size)) return;
                    auto ptr = spa.get();
                    handler(
                        buffer(string_view(ptr, size), std::move(spa)),
                        buffer(),
                        std::move(func),
                        std::move(self)
                    );
                }
            );
        }
        else {
            if (buf.size() < size) {
                call_message_size_error_handlers(func);
                return;
            }
            socket_->post(
                [
                    self = std::move(self),
                    func = std::move(func),
                    buf = std::move(buf),
                    size,
                    handler = std::move(handler)
                ]
                () mutable {
                    handler(
                        buf.substr(0, size),
                        buf.substr(size),
                        std::move(func),
                        std::move(self)
                    );
                }
            );
        }
    }

    template <std::size_t Bytes>
    void process_fixed_length(
        async_handler_t func,
        buffer buf,
        std::function<void(std::size_t, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        if (remaining_length_ < Bytes) {
            call_message_size_error_handlers(func);
            return;
        }

        remaining_length_ -= Bytes;

        if (buf.empty()) {
            socket_->async_read(
                as::buffer(&buf_[0], Bytes),
                [
                    this,
                    self = std::move(self),
                    func = std::move(func),
                    handler = std::move(handler)
                ]
                (boost::system::error_code const& ec,
                 std::size_t bytes_transferred) mutable {
                    if (!check_error_and_transferred_length(ec, func, bytes_transferred, Bytes)) return;
                    handler(
                        make_packet_id<Bytes>::apply(&buf_[0], &buf_[0] + static_cast<buffer::difference_type>(Bytes)),
                        buffer(),
                        std::move(func),
                        std::move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
               [
                    self = std::move(self),
                    func = std::move(func),
                    buf = std::move(buf),
                    handler = std::move(handler)
               ]
               () mutable {
                    auto packet_id = make_packet_id<Bytes>::apply(&buf[0], &buf[0] + static_cast<buffer::difference_type>(Bytes));
                    handler(
                        packet_id,
                        std::move(buf).substr(Bytes),
                        std::move(func),
                        std::move(self)
                    );
               }
            );
        }
    }

    // This function isn't used for remaining lengh.
    void process_variable_length(
        async_handler_t func,
        buffer buf,
        std::function<void(std::size_t, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_variable_length_impl(
            std::move(func),
            std::move(buf),
            std::move(handler),
            0,
            1,
            std::move(self)
        );
    }

    void process_variable_length_impl(
        async_handler_t func,
        buffer buf,
        std::function<void(std::size_t, buffer, async_handler_t, this_type_sp)> handler,
        std::size_t size,
        std::size_t multiplier,
        this_type_sp self
    ) {
        if (remaining_length_ == 0) {
            call_message_size_error_handlers(func);
            return;
        }
        --remaining_length_;

        auto proc =
            [this]
            (
                async_handler_t&& func,
                buffer&& buf,
                std::function<void(std::size_t, buffer, async_handler_t, this_type_sp)>&& handler,
                std::size_t size,
                std::size_t multiplier,
                this_type_sp&& self
            ) mutable {
                size += (buf[0] & 0b01111111) * multiplier;
                multiplier *= 128;
                if (multiplier > 128 * 128 * 128 * 128) {
                    call_message_size_error_handlers(func);
                    return;
                }
                if (buf[0] & 0b10000000) {
                    BOOST_ASSERT(!buf.empty());
                    process_variable_length_impl(
                        std::move(func),
                        std::move(buf).substr(1),
                        std::move(handler),
                        size,
                        multiplier,
                        std::move(self)
                    );
                }
                else {
                    handler(
                        size,
                        std::move(buf).substr(1),
                        std::move(func),
                        std::move(self)
                    );
                }
            };

        if (buf.empty()) {
            socket_->async_read(
                as::buffer(&buf_[0], 1),
                [
                    this,
                    self = std::move(self),
                    func = std::move(func),
                    handler = std::move(handler),
                    size,
                    multiplier,
                    proc = std::move(proc)
                ]
                (boost::system::error_code const& ec,
                 std::size_t bytes_transferred) mutable {
                    if (!check_error_and_transferred_length(ec, func, bytes_transferred, 1)) return;
                    proc(
                        std::move(func),
                        buffer(string_view(buf_.data(), 1)),
                        std::move(handler),
                        size,
                        multiplier,
                        std::move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
                [
                    func = std::move(func),
                    handler = std::move(handler),
                    buf = std::move(buf),
                    size,
                    multiplier,
                    proc = std::move(proc),
                    self = std::move(self)
                ]
                () mutable {
                    proc(
                        std::move(func),
                        std::move(buf),
                        std::move(handler),
                        size,
                        multiplier,
                        std::move(self)
                    );
                }
            );
        }
    }

    void process_packet_id(
        async_handler_t func,
        buffer buf,
        std::function<void(packet_id_t, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_fixed_length<sizeof(packet_id_t)>(
            std::move(func),
            std::move(buf),
            [
                handler = std::move(handler)
            ]
            (std::size_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                handler(static_cast<packet_id_t>(packet_id), std::move(buf), std::move(func), std::move(self));
            },
            std::move(self)
        );
    }

    void process_binary(
        async_handler_t func,
        buffer buf,
        std::function<void(buffer, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        if (remaining_length_ < 2) {
            call_message_size_error_handlers(func);
            return;
        }
        process_fixed_length<2>(
            std::move(func),
            std::move(buf),
            [
                this,
                handler = std::move(handler)
            ]
            (std::size_t size,
             buffer buf,
             async_handler_t func,
             this_type_sp self) mutable {
                if (remaining_length_ < size) {
                    call_message_size_error_handlers(func);
                    return;
                }
                process_nbytes(
                    std::move(func),
                    std::move(buf),
                    size,
                    std::move(handler),
                    std::move(self)
                );
            },
            std::move(self)
        );
    }

    void process_string(
        async_handler_t func,
        buffer buf,
        std::function<void(buffer, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_binary(
            std::move(func),
            std::move(buf),
            [this, handler = std::move(handler)]
            (buffer str, buffer buf, async_handler_t func, this_type_sp self) mutable {
                auto r = utf8string::validate_contents(str);
                if (r != utf8string::validation::well_formed) {
                    call_protocol_error_handlers(func);
                    return;
                }
                handler(std::move(str), std::move(buf), std::move(func), std::move(self));
            },
            std::move(self)
        );
    }


    void process_properties(
        async_handler_t func,
        buffer buf,
        std::function<void(std::vector<v5::property_variant>, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {
        process_variable_length(
            std::move(func),
            std::move(buf),
            [
                this,
                handler = std::move(handler)
            ]
            (std::size_t property_length, buffer buf, async_handler_t func, this_type_sp self) mutable {
                if (property_length > remaining_length_) {
                    call_message_size_error_handlers(func);
                    return;
                }
                if (property_length == 0) {
                    handler(std::vector<v5::property_variant>(), std::move(buf), std::move(func), std::move(self));
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
                                        std::move(spa),
                                        ptr,
                                        property_length
                                    };
                            }
                            return
                                {
                                    nullptr,
                                    &buf_[0],
                                    1
                                };
                        } ();
                    socket_->async_read(
                        as::buffer(result.address, result.len),
                        [
                            this,
                            self = std::move(self),
                            func = std::move(func),
                            handler = std::move(handler),
                            property_length,
                            result
                        ]
                        (boost::system::error_code const& ec,
                         std::size_t bytes_transferred) mutable {
                            if (!check_error_and_transferred_length(ec, func, bytes_transferred, result.len)) return;
                            process_property_id(
                                std::move(func),
                                buffer(string_view(result.address, result.len), result.spa),
                                property_length,
                                std::vector<v5::property_variant>(),
                                std::move(handler),
                                std::move(self)
                            );
                        }
                    );
                }
                else {
                    socket_->post(
                        [
                            this,
                            self = std::move(self),
                            func = std::move(func),
                            buf = std::move(buf),
                            handler = std::move(handler),
                            property_length
                        ]
                        () mutable {
                            process_property_id(
                                std::move(func),
                                std::move(buf),
                                property_length,
                                std::vector<v5::property_variant>(),
                                std::move(handler),
                                std::move(self)
                            );
                        }
                    );
                }
            },
            std::move(self)
        );
    }

    void process_property_id(
        async_handler_t func,
        buffer buf,
        std::size_t property_length_rest,
        std::vector<v5::property_variant> props,
        std::function<void(std::vector<v5::property_variant>, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {

        if (property_length_rest == 0) {
            handler(std::move(props), std::move(buf), std::move(func), std::move(self));
            return;
        }

        --remaining_length_;
        if (buf.empty()) {
            socket_->async_read(
                as::buffer(&buf_[0], 1),
                [
                    this,
                    self = std::move(self),
                    func = std::move(func),
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (boost::system::error_code const& ec,
                 std::size_t bytes_transferred) mutable {
                    if (!check_error_and_transferred_length(ec, func, bytes_transferred, 1)) return;
                    process_property_body(
                        std::move(func),
                        buffer(),
                        static_cast<v5::property::id>(buf_[0]),
                        property_length_rest - 1,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                }
            );
        }
        else {
            socket_->post(
                [
                    this,
                    self = std::move(self),
                    func = std::move(func),
                    buf = std::move(buf),
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                () mutable {
                    auto id = static_cast<v5::property::id>(buf[0]);
                    process_property_body(
                        std::move(func),
                        std::move(buf).substr(1),
                        id,
                        property_length_rest - 1,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                }
            );
        }
    }

    void process_property_body(
        async_handler_t func,
        buffer buf,
        v5::property::id id,
        std::size_t property_length_rest,
        std::vector<v5::property_variant> props,
        std::function<void(std::vector<v5::property_variant>, buffer, async_handler_t, this_type_sp)> handler,
        this_type_sp self
    ) {

        static constexpr std::size_t const length_bytes = 2;

        if (property_length_rest == 0) {
            call_message_size_error_handlers(func);
            return;
        }

        switch (id) {
        case v5::property::id::payload_format_indicator: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::payload_format_indicator(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::message_expiry_interval: {
            std::size_t const len = 4;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::message_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::content_type: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::content_type(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::response_topic: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_topic(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::correlation_data: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::correlation_data(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::subscription_identifier: {
            process_variable_length(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest,
                    size_before = buf.size()
                ]
                (std::size_t size, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - (size_before - buf.size());
                    props.emplace_back(
                        v5::property::subscription_identifier(size)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::session_expiry_interval: {
            std::size_t const len = 4;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::session_expiry_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::assigned_client_identifier: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::assigned_client_identifier(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );

        } break;
        case v5::property::id::server_keep_alive: {
            std::size_t const len = 2;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::server_keep_alive(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::authentication_method: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_method(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::authentication_data: {
            process_binary(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::authentication_data(std::move(body))
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::request_problem_information: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::request_problem_information(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::will_delay_interval: {
            std::size_t const len = 4;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::will_delay_interval(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::request_response_information: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::request_response_information(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::response_information: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::response_information(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::server_reference: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::server_reference(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::reason_string: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - body.size();
                    props.emplace_back(
                        v5::property::reason_string(std::move(body), true)
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::receive_maximum: {
            std::size_t const len = 2;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::receive_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::topic_alias_maximum: {
            std::size_t const len = 2;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::topic_alias_maximum(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::topic_alias: {
            std::size_t const len = 2;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::topic_alias(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::maximum_qos: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::maximum_qos(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::retain_available: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::retain_available(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::user_property: {
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    property_length_rest
                ]
                (buffer key, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    auto rest = property_length_rest - length_bytes - key.size();
                    process_string(
                        std::move(func),
                        std::move(buf),
                        [
                            this,
                            props = std::move(props),
                            handler = std::move(handler),
                            key = std::move(key),
                            property_length_rest = rest
                        ]
                        (buffer val, buffer buf, async_handler_t func, this_type_sp self) mutable {
                            auto rest = property_length_rest - length_bytes - val.size();
                            props.emplace_back(
                                v5::property::user_property(
                                    std::move(key),
                                    std::move(val),
                                    true,
                                    true
                                )
                            );
                            process_property_id(
                                std::move(func),
                                std::move(buf),
                                rest,
                                std::move(props),
                                std::move(handler),
                                std::move(self)
                            );
                        },
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::maximum_packet_size: {
            std::size_t const len = 4;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::maximum_packet_size(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::wildcard_subscription_available: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::wildcard_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::subscription_identifier_available: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::subscription_identifier_available(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        case v5::property::id::shared_subscription_available: {
            std::size_t const len = 1;
            if (property_length_rest < len) {
                call_message_size_error_handlers(func);
                return;
            }
            process_nbytes(
                std::move(func),
                std::move(buf),
                len,
                [
                    this,
                    props = std::move(props),
                    handler = std::move(handler),
                    rest = property_length_rest - len
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    props.emplace_back(
                        v5::property::shared_subscription_available(body.begin(), body.end())
                    );
                    process_property_id(
                        std::move(func),
                        std::move(buf),
                        rest,
                        std::move(props),
                        std::move(handler),
                        std::move(self)
                    );
                },
                std::move(self)
            );
        } break;
        }
    }

    // process common

    template <typename NextFunc, typename NextPhase, typename Info>
    void process_header(
        async_handler_t func,
        bool all_read,
        std::size_t header_len,
        NextFunc&& next_func,
        NextPhase next_phase,
        Info&& info,
        this_type_sp self
    ) {

        if (all_read) {
            auto spa = make_shared_ptr_array(remaining_length_);
            auto ptr = spa.get();
            socket_->async_read(
                as::buffer(ptr, remaining_length_),
                [
                    this,
                    func = std::move(func),
                    spa = std::move(spa),
                    next_func = std::forward<NextFunc>(next_func),
                    next_phase,
                    info = std::forward<Info>(info),
                    self = std::move(self)
                ]
                (boost::system::error_code const& ec, std::size_t bytes_transferred) mutable {
                    if (!check_error_and_transferred_length(ec, func, bytes_transferred, remaining_length_)) return;
                    auto ptr = spa.get();
                    (this->*next_func)(
                        std::move(func),
                        buffer(string_view(ptr, remaining_length_), std::move(spa)),
                        next_phase,
                        std::move(info),
                        std::move(self)
                    );
                }
            );
            return;
        }

        if (header_len == 0) {
            (this->*next_func)(
                std::move(func),
                buffer(),
                next_phase,
                std::move(info),
                std::move(self)
            );
            return;
        }

        socket_->async_read(
            as::buffer(&buf_[0], header_len),
            [
                this,
                func = std::move(func),
                header_len,
                next_func = std::forward<NextFunc>(next_func),
                next_phase,
                info = std::forward<Info>(info),
                self = std::move(self)
            ]
            (boost::system::error_code const& ec,
             std::size_t bytes_transferred) mutable {
                if (!check_error_and_transferred_length(ec, func, bytes_transferred, header_len)) return;
                (this->*next_func)(
                    std::move(func),
                    buffer(string_view(buf_.data(), header_len)),
                    next_phase,
                    std::move(info),
                    std::move(self)
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
        std::vector<v5::property_variant> props;
        buffer client_id;
        std::vector<v5::property_variant> will_props;
        buffer will_topic;
        buffer will_payload;
        mqtt::optional<buffer> user_name;
        mqtt::optional<buffer> password;
    };

    void process_connect(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            2 +  // string length
            4 +  // "MQTT" string
            1 +  // ProtocolVersion
            1 +  // ConnectFlag
            2;   // KeepAlive

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        connect_info info;
        info.header_len = header_len;

        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_connect_impl,
            connect_phase::header,
            std::move(info),
            std::move(self)
        );
    }

    void process_connect_impl(
        async_handler_t func,
        buffer buf,
        connect_phase phase,
        connect_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case connect_phase::header: {
            std::size_t i = 0;
            if (buf[i++] != 0x00 ||
                buf[i++] != 0x04 ||
                buf[i++] != 'M' ||
                buf[i++] != 'Q' ||
                buf[i++] != 'T' ||
                buf[i++] != 'T') {
                call_protocol_error_handlers(func);
                return;
            }
            auto version = static_cast<protocol_version>(buf[i++]);
            if (version != protocol_version::v3_1_1 && version != protocol_version::v5) {
                if (func) {
                    func(
                        boost::system::errc::make_error_code(
                            boost::system::errc::protocol_not_supported
                        )
                    );
                }
                return;
            }

            if (version_ == protocol_version::undetermined) {
                version_ = version;
            }
            else if (version_ != version) {
                if (func) {
                    func(
                        boost::system::errc::make_error_code(
                            boost::system::errc::protocol_not_supported
                        )
                    );
                }
                return;
            }

            info.connect_flag = buf[i++];

            info.keep_alive = make_uint16_t(buf[i], buf[i + 1]);
            clean_session_ = connect_flags::has_clean_session(info.connect_flag);

            process_connect_impl(
                std::move(func),
                std::move(buf).substr(info.header_len), // consume buffer
                [&] {
                    if (version_ == protocol_version::v5) {
                        return connect_phase::properties;
                    }
                    return connect_phase::client_id;
                } (),
                std::move(info),
                std::move(self)
            );
        } break;
        case connect_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (
                    std::vector<v5::property_variant> props,
                    buffer buf,
                    async_handler_t func,
                    this_type_sp self
                ) mutable {
                    info.props = std::move(props);
                    process_connect_impl(
                        std::move(func),
                        std::move(buf),
                        connect_phase::client_id,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case connect_phase::client_id:
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer client_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.client_id = std::move(client_id);
                    auto connect_flag = info.connect_flag;
                    process_connect_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (connect_flags::has_will_flag(connect_flag)) {
                                return connect_phase::will;
                            }
                            if (connect_flags::has_user_name_flag(connect_flag)) {
                                return connect_phase::user_name;
                            }
                            if (connect_flags::has_password_flag(connect_flag)) {
                                return connect_phase::password;
                            }
                            return connect_phase::finish;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case connect_phase::will: {
            auto topic_message_proc =
                [this]
                (
                    async_handler_t&& func,
                    buffer&& buf,
                    connect_info&& info,
                    this_type_sp&& self
                ) mutable {
                    process_string(
                        std::move(func),
                        std::move(buf),
                        [
                            this,
                            info = std::move(info)
                        ]
                        (buffer will_topic, buffer buf, async_handler_t func, this_type_sp self) mutable {
                            info.will_topic = std::move(will_topic);
                            process_binary(
                                std::move(func),
                                std::move(buf),
                                [
                                    this,
                                    info = std::move(info)
                                ]
                                (buffer will_payload, buffer buf, async_handler_t func, this_type_sp self) mutable {
                                    info.will_payload = std::move(will_payload);
                                    auto connect_flag = info.connect_flag;
                                    process_connect_impl(
                                        std::move(func),
                                        std::move(buf),
                                        [&] {
                                            if (connect_flags::has_user_name_flag(connect_flag)) {
                                                return connect_phase::user_name;
                                            }
                                            if (connect_flags::has_password_flag(connect_flag)) {
                                                return connect_phase::password;
                                            }
                                            return connect_phase::finish;
                                        } (),
                                        std::move(info),
                                        std::move(self)
                                    );
                                },
                                std::move(self)
                            );
                        },
                        std::move(self)
                    );
                };

            if (version_ == protocol_version::v5) {
                process_properties(
                    std::move(func),
                    std::move(buf),
                    [
                        info = std::move(info),
                        topic_message_proc
                    ]
                    (std::vector<v5::property_variant> will_props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                        info.will_props = std::move(will_props);
                        topic_message_proc(
                            std::move(func),
                            std::move(buf),
                            std::move(info),
                            std::move(self)
                        );
                    },
                    std::move(self)
                );
                return;
            }
            topic_message_proc(
                std::move(func),
                std::move(buf),
                std::move(info),
                std::move(self)
            );
        } break;
        case connect_phase::user_name:
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer user_name, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.user_name = std::move(user_name);
                    auto connect_flag = info.connect_flag;
                    process_connect_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (connect_flags::has_password_flag(connect_flag)) {
                                return connect_phase::password;
                            }
                            return connect_phase::finish;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case connect_phase::password:
            process_binary(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer password, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.password = std::move(password);
                    process_connect_impl(
                        std::move(func),
                        std::move(buf),
                        connect_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case connect_phase::finish:
            mqtt_connected_ = true;
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_connect_) {
                    if (!h_connect_(
                            std::move(info.client_id),
                            std::move(info.user_name),
                            std::move(info.password),
                            [&] () -> optional<will> {
                                if (connect_flags::has_will_flag(info.connect_flag)) {
                                    return
                                        will(
                                            std::move(info.will_topic),
                                            std::move(info.will_payload),
                                            connect_flags::has_will_retain(info.connect_flag),
                                            connect_flags::will_qos(info.connect_flag)
                                        );
                                }
                                return mqtt::nullopt;
                            } (),
                            clean_session_,
                            info.keep_alive
                        )
                    ) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_connect_) {
                    if (!h_v5_connect_(
                            std::move(info.client_id),
                            std::move(info.user_name),
                            std::move(info.password),
                            [&] () -> optional<will> {
                                if (connect_flags::has_will_flag(info.connect_flag)) {
                                    return
                                        will(
                                            std::move(info.will_topic),
                                            std::move(info.will_payload),
                                            connect_flags::has_will_retain(info.connect_flag),
                                            connect_flags::will_qos(info.connect_flag),
                                            std::move(info.will_props)
                                        );
                                }
                                return mqtt::nullopt;
                            } (),
                            clean_session_,
                            info.keep_alive,
                            std::move(info.props)
                        )
                    ) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_connack(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            1 +  // Connect Acknowledge Flags
            1;   // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        connack_info info;
        info.header_len = header_len;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_connack_impl,
            connack_phase::header,
            std::move(info),
            std::move(self)
        );
    }

    void process_connack_impl(
        async_handler_t func,
        buffer buf,
        connack_phase phase,
        connack_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case connack_phase::header:
            info.session_present = is_session_present(buf[0]);
            info.reason_code = static_cast<std::uint8_t>(buf[1]);

            process_connack_impl(
                std::move(func),
                std::move(buf).substr(info.header_len),
                [&] {
                    if (version_ == protocol_version::v5) {
                        return connack_phase::properties;
                    }
                    return connack_phase::finish;
                } (),
                std::move(info),
                std::move(self)
            );
            break;
        case connack_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_connack_impl(
                        std::move(func),
                        std::move(buf),
                        connack_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case connack_phase::finish: {
            mqtt_connected_ = true;
            auto connack_proc =
                [this]
                (
                    async_handler_t&& func,
                    connack_info&& info
                ) mutable {

                    switch (version_) {
                    case protocol_version::v3_1_1:
                        if (h_connack_) {
                            if (!h_connack_(info.session_present, info.reason_code)) {
                                return;
                            }
                        }
                        h_mqtt_message_processed_(std::move(func));
                        break;
                    case protocol_version::v5:
                        if (h_v5_connack_) {
                            if (!h_v5_connack_(info.session_present, info.reason_code, std::move(info.props))) {
                                return;
                            }
                        }
                        h_mqtt_message_processed_(std::move(func));
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                };

            if (info.reason_code == v5::reason_code::success) {
                if (clean_session_) {
                    LockGuard<Mutex> lck (store_mtx_);
                    store_.clear();
                    packet_id_.clear();
                }
                else {
                    if (async_send_store_) {
                        // Store and replace mqtt_message_processed handler
                        // Until all stored messages are written to internal send buffer,
                        // stop the next async read.
                        auto org = get_mqtt_message_processed_handler();
                        set_mqtt_message_processed_handler(
                            []
                            (async_handler_t) {
                            }
                        );

                        auto async_connack_proc =
                            [
                                this,
                                self = std::move(self),
                                func = std::move(func),
                                connack_proc = std::move(connack_proc),
                                org = std::move(org),
                                info = std::move(info)
                            ]
                            () mutable {
                                // All stored messages are sent, then restore the original handler
                                // and do the connack process. If it returns true, read the next mqtt message.
                                set_mqtt_message_processed_handler(std::move(org));
                                connack_proc(std::move(func), std::move(info));
                            };
                        async_send_store(std::move(async_connack_proc));
                        return;
                    }
                    send_store();
                }
            }
            connack_proc(std::move(func), std::move(info));
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
        mqtt::optional<packet_id_t> packet_id;
        std::vector<v5::property_variant> props;
    };

    void process_publish(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const min_len =
            2; // topic name length

        if (remaining_length_ < min_len) {
            call_protocol_error_handlers(func);
            return;
        }

        process_header(
            std::move(func),
            all_read,
            0,
            &this_type::process_publish_impl,
            publish_phase::topic_name,
            publish_info(),
            std::move(self)
        );
    }

    void process_publish_impl(
        async_handler_t func,
        buffer buf,
        publish_phase phase,
        publish_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case publish_phase::topic_name:
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer topic_name, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.topic_name = std::move(topic_name);
                    auto qos = publish::get_qos(fixed_header_);
                    if (qos != qos::at_most_once &&
                        qos != qos::at_least_once &&
                        qos != qos::exactly_once) {
                        call_protocol_error_handlers(func);
                        return;
                    }
                    process_publish_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (qos == qos::at_most_once) {
                                if (version_ == protocol_version::v5) {
                                    return publish_phase::properties;
                                }
                                return publish_phase::payload;
                            }
                            return publish_phase::packet_id;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case publish_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_publish_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (version_ == protocol_version::v5) {
                                return publish_phase::properties;
                            }
                            return publish_phase::payload;
                        } (),
                        std::move(info),
                        std::move(self)
                    );

                },
                std::move(self)
            );
            break;
        case publish_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_publish_impl(
                        std::move(func),
                        std::move(buf),
                        publish_phase::payload,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case publish_phase::payload:
            process_nbytes(
                std::move(func),
                std::move(buf),
                remaining_length_,
                [
                    this,
                    info = std::move(info)
                ]
                (buffer payload, buffer /*buf*/, async_handler_t func, this_type_sp /*self*/) mutable {
                    auto handler_call =
                        [&] {
                            switch (version_) {
                            case protocol_version::v3_1_1:
                                if (h_publish_) {
                                    if (!h_publish_(fixed_header_, info.packet_id, std::move(info.topic_name), std::move(payload))) {
                                        return false;
                                    }
                                }
                                h_mqtt_message_processed_(std::move(func));
                                break;
                            case protocol_version::v5:
                                if (h_v5_publish_) {
                                    if (!h_v5_publish_(
                                            fixed_header_,
                                            info.packet_id,
                                            std::move(info.topic_name),
                                            std::move(payload),
                                            std::move(info.props)
                                        )
                                    ) {
                                        return false;
                                    }
                                }
                                h_mqtt_message_processed_(std::move(func));
                                break;
                            default:
                                BOOST_ASSERT(false);
                            }
                            return true;
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
                                        send_puback(*info.packet_id);
                                    }
                                },
                                [this, &info, &func] {
                                    if (connected_) {
                                        async_send_puback(
                                            *info.packet_id,
                                            mqtt::nullopt,
                                            std::vector<v5::property_variant>{},
                                            func
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
                                        send_pubrec(*info.packet_id);
                                    }
                                },
                                [this, &info, &func] {
                                    if (connected_) {
                                        async_send_pubrec(
                                            *info.packet_id,
                                            mqtt::nullopt,
                                            std::vector<v5::property_variant>{},
                                            func
                                        );
                                    }
                                }
                            );
                        }
                        break;
                    }
                },
                std::move(self)
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_puback(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        puback_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_puback_impl,
            puback_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_puback_impl(
        async_handler_t func,
        buffer buf,
        puback_phase phase,
        puback_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case puback_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_puback_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (remaining_length_ == 0) {
                                info.reason_code = v5::reason_code::success;
                                return puback_phase::finish;
                            }
                            return puback_phase::reason_code;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case puback_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_puback_impl(
                        std::move(func),
                        std::move(buf),
                        puback_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case puback_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_puback_impl(
                        std::move(func),
                        std::move(buf),
                        puback_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
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
            if (h_serialize_remove_) h_serialize_remove_(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_puback_) {
                    if (!h_puback_(info.packet_id)) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_puback_) {
                    if (!h_v5_puback_(info.packet_id, info.reason_code, std::move(info.props))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_pubrec(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        pubrec_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_pubrec_impl,
            pubrec_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_pubrec_impl(
        async_handler_t func,
        buffer buf,
        pubrec_phase phase,
        pubrec_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case pubrec_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_pubrec_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (remaining_length_ == 0) {
                                info.reason_code = v5::reason_code::success;
                                return pubrec_phase::finish;
                            }
                            return pubrec_phase::reason_code;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubrec_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_pubrec_impl(
                        std::move(func),
                        std::move(buf),
                        pubrec_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubrec_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_pubrec_impl(
                        std::move(func),
                        std::move(buf),
                        pubrec_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
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
                                send_pubrel(info.packet_id);
                            }
                            else {
                                store_pubrel(info.packet_id);
                            }
                        },
                        [&] {
                            if (connected_) {
                                async_send_pubrel(
                                    info.packet_id,
                                    mqtt::nullopt,
                                    std::vector<v5::property_variant>{},
                                    func
                                );
                            }
                            else {
                                store_pubrel(info.packet_id);
                            }
                        }
                    );
                };
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_pubrec_) {
                    if (!h_pubrec_(info.packet_id)) {
                        return;
                    }
                }
                res();
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_pubrec_) {
                    if (!h_v5_pubrec_(info.packet_id, info.reason_code, std::move(info.props))) {
                        return;
                    }
                }
                res();
                h_mqtt_message_processed_(std::move(func));
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_pubrel(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        pubrel_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_pubrel_impl,
            pubrel_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_pubrel_impl(
        async_handler_t func,
        buffer buf,
        pubrel_phase phase,
        pubrel_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case pubrel_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_pubrel_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (remaining_length_ == 0) {
                                info.reason_code = v5::reason_code::success;
                                return pubrel_phase::finish;
                            }
                            return pubrel_phase::reason_code;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubrel_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_pubrel_impl(
                        std::move(func),
                        std::move(buf),
                        pubrel_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubrel_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_pubrel_impl(
                        std::move(func),
                        std::move(buf),
                        pubrel_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubrel_phase::finish: {
            auto res =
                [&] {
                    auto_pub_response(
                        [&] {
                            if (connected_) {
                                send_pubcomp(info.packet_id);
                            }
                        },
                        [&] {
                            if (connected_) {
                                async_send_pubcomp(
                                    info.packet_id,
                                    mqtt::nullopt,
                                    std::vector<v5::property_variant>{},
                                    func
                                );
                            }
                        }
                    );
                };
            qos2_publish_handled_.erase(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_pubrel_) {
                    if (!h_pubrel_(info.packet_id)) {
                        return;
                    }
                }
                res();
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_pubrel_) {
                    if (!h_v5_pubrel_(info.packet_id, info.reason_code, std::move(info.props))) {
                        return;
                    }
                }
                res();
                h_mqtt_message_processed_(std::move(func));
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_pubcomp(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        pubcomp_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_pubcomp_impl,
            pubcomp_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_pubcomp_impl(
        async_handler_t func,
        buffer buf,
        pubcomp_phase phase,
        pubcomp_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case pubcomp_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_pubcomp_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (remaining_length_ == 0) {
                                info.reason_code = v5::reason_code::success;
                                return pubcomp_phase::finish;
                            }
                            return pubcomp_phase::reason_code;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubcomp_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_pubcomp_impl(
                        std::move(func),
                        std::move(buf),
                        pubcomp_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubcomp_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_pubcomp_impl(
                        std::move(func),
                        std::move(buf),
                        pubcomp_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case pubcomp_phase::finish:
            {
                LockGuard<Mutex> lck (store_mtx_);
                auto& idx = store_.template get<tag_packet_id_type>();
                auto r = idx.equal_range(std::make_tuple(info.packet_id, control_packet_type::pubcomp));
                idx.erase(std::get<0>(r), std::get<1>(r));
                // packet_id shouldn't be erased here.
                // It is reused for pubrel/pubcomp.
            }
            if (h_serialize_remove_) h_serialize_remove_(info.packet_id);
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_pubcomp_) {
                    if (!h_pubcomp_(info.packet_id)) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_pubcomp_) {
                    if (!h_v5_pubcomp_(info.packet_id, info.reason_code, std::move(info.props))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
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
        std::vector<v5::property_variant> props;
        std::vector<std::tuple<buffer, std::uint8_t>> entries;
    };

    void process_subscribe(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        subscribe_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_subscribe_impl,
            subscribe_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_subscribe_impl(
        async_handler_t func,
        buffer buf,
        subscribe_phase phase,
        subscribe_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case subscribe_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_subscribe_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (version_ == protocol_version::v5) {
                                return subscribe_phase::properties;
                            }
                            return subscribe_phase::topic;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case subscribe_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_subscribe_impl(
                        std::move(func),
                        std::move(buf),
                        subscribe_phase::topic,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case subscribe_phase::topic:
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer topic_filter, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    process_nbytes(
                        std::move(func),
                        std::move(buf),
                        1, // requested_qos
                        [
                            this,
                            info = std::move(info),
                            topic_filter = std::move(topic_filter)
                        ]
                        (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                            auto requested_qos = static_cast<std::uint8_t>(body[0]);
                            if (requested_qos != qos::at_most_once &&
                                requested_qos != qos::at_least_once &&
                                requested_qos != qos::exactly_once) {
                                call_protocol_error_handlers(func);
                                return;
                            }
                            info.entries.emplace_back(std::move(topic_filter), requested_qos);
                            process_subscribe_impl(
                                std::move(func),
                                std::move(buf),
                                [&] {
                                    if (remaining_length_ == 0) {
                                        return subscribe_phase::finish;
                                    }
                                    return subscribe_phase::topic;
                                } (),
                                std::move(info),
                                std::move(self)
                            );
                        },
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case subscribe_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_subscribe_) {
                    if (!h_subscribe_(info.packet_id, std::move(info.entries))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_subscribe_) {
                    if (!h_v5_subscribe_(info.packet_id, std::move(info.entries), std::move(info.props))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
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
        std::vector<v5::property_variant> props;
    };

    void process_suback(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        suback_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_suback_impl,
            suback_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_suback_impl(
        async_handler_t func,
        buffer buf,
        suback_phase phase,
        suback_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case suback_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_suback_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (version_ == protocol_version::v5) {
                                return suback_phase::properties;
                            }
                            return suback_phase::reasons;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case suback_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_suback_impl(
                        std::move(func),
                        std::move(buf),
                        suback_phase::reasons,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case suback_phase::reasons:
            process_nbytes(
                std::move(func),
                std::move(buf),
                remaining_length_, // Reason Codes
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer /*buf*/, async_handler_t func, this_type_sp /*self*/) mutable {
                    {
                        LockGuard<Mutex> lck (store_mtx_);
                        packet_id_.erase(info.packet_id);
                    }
                    switch (version_) {
                    case protocol_version::v3_1_1:
                        if (h_suback_) {
                            std::vector<mqtt::optional<std::uint8_t>> results;
                            results.resize(body.size());
                            std::transform(
                                body.begin(),
                                body.end(),
                                results.begin(),
                                [&](auto const& e) -> mqtt::optional<std::uint8_t> {
                                    if (e & 0b10000000) {
                                        return mqtt::nullopt;
                                    }
                                    else {
                                        return static_cast<std::uint8_t>(e);
                                    }
                                }
                            );
                            if (!h_suback_(info.packet_id, std::move(results))) {
                                return;
                            }
                        }
                        h_mqtt_message_processed_(std::move(func));
                        break;
                    case protocol_version::v5:
                        if (h_v5_suback_) {
                            std::vector<std::uint8_t> reasons;
                            reasons.resize(body.size());
                            std::transform(
                                body.begin(),
                                body.end(),
                                reasons.begin(),
                                [&](auto const& e) {
                                    return static_cast<uint8_t>(e);
                                }
                            );
                            if (!h_v5_suback_(info.packet_id, std::move(reasons), std::move(info.props))) {
                                return;
                            }
                        }
                        h_mqtt_message_processed_(std::move(func));
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                },
                std::move(self)
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
        std::vector<v5::property_variant> props;
        std::vector<buffer> entries;
    };

    void process_unsubscribe(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        unsubscribe_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_unsubscribe_impl,
            unsubscribe_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_unsubscribe_impl(
        async_handler_t func,
        buffer buf,
        unsubscribe_phase phase,
        unsubscribe_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case unsubscribe_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    process_unsubscribe_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (version_ == protocol_version::v5) {
                                return unsubscribe_phase::properties;
                            }
                            return unsubscribe_phase::topic;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case unsubscribe_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_unsubscribe_impl(
                        std::move(func),
                        std::move(buf),
                        unsubscribe_phase::topic,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case unsubscribe_phase::topic:
            process_string(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (buffer topic_filter, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.entries.emplace_back(std::move(topic_filter));
                    process_unsubscribe_impl(
                        std::move(func),
                        std::move(buf),
                        [&] {
                            if (remaining_length_ == 0) {
                                return unsubscribe_phase::finish;
                            }
                            return unsubscribe_phase::topic;
                        } (),
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case unsubscribe_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_unsubscribe_) {
                    if (!h_unsubscribe_(info.packet_id, std::move(info.entries))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
                break;
            case protocol_version::v5:
                if (h_v5_unsubscribe_) {
                    if (!h_v5_unsubscribe_(info.packet_id, std::move(info.entries), std::move(info.props))) {
                        return;
                    }
                }
                h_mqtt_message_processed_(std::move(func));
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
        std::vector<v5::property_variant> props;
    };

    void process_unsuback(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        std::size_t const header_len =
            sizeof(packet_id_t);    // Packet Id

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        unsuback_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_unsuback_impl,
            unsuback_phase::packet_id,
            std::move(info),
            std::move(self)
        );
    }

    void process_unsuback_impl(
        async_handler_t func,
        buffer buf,
        unsuback_phase phase,
        unsuback_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case unsuback_phase::packet_id:
            process_packet_id(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (packet_id_t packet_id, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.packet_id = packet_id;
                    {
                        LockGuard<Mutex> lck (store_mtx_);
                        packet_id_.erase(info.packet_id);
                    }
                    switch (version_) {
                    case protocol_version::v3_1_1:
                        if (h_unsuback_) {
                            if (!h_unsuback_(info.packet_id)) {
                                return;
                            }
                        }
                        h_mqtt_message_processed_(std::move(func));
                        break;
                    case protocol_version::v5:
                        process_unsuback_impl(
                            std::move(func),
                            std::move(buf),
                            unsuback_phase::properties,
                            std::move(info),
                            std::move(self)
                        );
                        break;
                    default:
                        BOOST_ASSERT(false);
                    }
                },
                std::move(self)
            );
            break;
        case unsuback_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_unsuback_impl(
                        std::move(func),
                        std::move(buf),
                        unsuback_phase::reasons,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case unsuback_phase::reasons:
            process_nbytes(
                std::move(func),
                std::move(buf),
                remaining_length_, // Reason Codes
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer /*buf*/, async_handler_t func, this_type_sp /*self*/) mutable {
                    BOOST_ASSERT(version_ == protocol_version::v5);
                    {
                        LockGuard<Mutex> lck (store_mtx_);
                        packet_id_.erase(info.packet_id);
                    }
                    if (h_v5_unsuback_) {
                        std::vector<std::uint8_t> reasons;
                        reasons.resize(body.size());
                        std::transform(
                            body.begin(),
                            body.end(),
                            reasons.begin(),
                            [&](auto const& e) {
                                return static_cast<uint8_t>(e);
                            }
                        );
                        if (!h_v5_unsuback_(info.packet_id, std::move(reasons), std::move(info.props))) {
                            return;
                        }
                        h_mqtt_message_processed_(std::move(func));
                    }
                },
                std::move(self)
            );
            break;
        }
    }

    // process pingreq

    void process_pingreq(
        async_handler_t func
    ) {
        std::size_t const header_len = 0;

        if (remaining_length_ != header_len) {
            call_protocol_error_handlers(func);
            return;
        }
        if (h_pingreq_) {
            if (!h_pingreq_()) return;
        }
        h_mqtt_message_processed_(std::move(func));
    }

    // process pingresp

    void process_pingresp(
        async_handler_t func
    ) {
        std::size_t const header_len = 0;

        if (remaining_length_ != header_len) {
            call_protocol_error_handlers(func);
            return;
        }
        if (h_pingresp_) {
            if (!h_pingresp_()) return;
        }
        h_mqtt_message_processed_(std::move(func));
    }

    // process disconnect

    enum class disconnect_phase {
        reason_code,
        properties,
        finish,
    };

    struct disconnect_info {
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_disconnect(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        if (remaining_length_ == 0) {
            disconnect_info info { v5::reason_code::normal_disconnection, std::vector<v5::property_variant>() };
            process_disconnect_impl(
                std::move(func),
                buffer(),
                disconnect_phase::finish,
                std::move(info),
                std::move(self)
            );
            return;
        }

        if (version_ != protocol_version::v5) {
            call_protocol_error_handlers(func);
            return;
        }

        std::size_t const header_len =
            1; // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        disconnect_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_disconnect_impl,
            disconnect_phase::reason_code,
            std::move(info),
            std::move(self)
        );
    }

    void process_disconnect_impl(
        async_handler_t func,
        buffer buf,
        disconnect_phase phase,
        disconnect_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case disconnect_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_disconnect_impl(
                        std::move(func),
                        std::move(buf),
                        disconnect_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case disconnect_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_disconnect_impl(
                        std::move(func),
                        std::move(buf),
                        disconnect_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case disconnect_phase::finish:
            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_disconnect_) {
                    h_disconnect_();
                }
                break;
            case protocol_version::v5:
                if (h_v5_disconnect_) {
                    h_v5_disconnect_(info.reason_code, std::move(info.props));
                }
                break;
            default:
                BOOST_ASSERT(false);
            }
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
        std::uint8_t reason_code;
        std::vector<v5::property_variant> props;
    };

    void process_auth(
        async_handler_t func,
        bool all_read,
        this_type_sp self
    ) {
        if (version_ != protocol_version::v5) {
            call_protocol_error_handlers(func);
            return;
        }

        if (remaining_length_ == 0) {
            auth_info info { v5::reason_code::success, std::vector<v5::property_variant>() };
            process_auth_impl(
                std::move(func),
                buffer(),
                auth_phase::finish,
                std::move(info),
                std::move(self)
            );
            return;
        }

        std::size_t const header_len =
            1; // Reason Code

        if (remaining_length_ < header_len) {
            call_protocol_error_handlers(func);
            return;
        }

        auth_info info;
        process_header(
            std::move(func),
            all_read,
            header_len,
            &this_type::process_auth_impl,
            auth_phase::reason_code,
            std::move(info),
            std::move(self)
        );
    }

    void process_auth_impl(
        async_handler_t func,
        buffer buf,
        auth_phase phase,
        auth_info&& info,
        this_type_sp self
    ) {
        switch (phase) {
        case auth_phase::reason_code:
            process_nbytes(
                std::move(func),
                std::move(buf),
                1, // reason_code
                [
                    this,
                    info = std::move(info)
                ]
                (buffer body, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.reason_code = static_cast<std::uint8_t>(body[0]);
                    process_auth_impl(
                        std::move(func),
                        std::move(buf),
                        auth_phase::properties,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case auth_phase::properties:
            process_properties(
                std::move(func),
                std::move(buf),
                [
                    this,
                    info = std::move(info)
                ]
                (std::vector<v5::property_variant> props, buffer buf, async_handler_t func, this_type_sp self) mutable {
                    info.props = std::move(props);
                    process_auth_impl(
                        std::move(func),
                        std::move(buf),
                        auth_phase::finish,
                        std::move(info),
                        std::move(self)
                    );
                },
                std::move(self)
            );
            break;
        case auth_phase::finish:
            BOOST_ASSERT(version_ == protocol_version::v5);
            if (h_v5_auth_) {
                if (!h_v5_auth_(info.reason_code, std::move(info.props))) {
                    return;
                }
            }
            h_mqtt_message_processed_(std::move(func));
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
        std::string const& client_id,
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
        mqtt::optional<will> const& w,
        std::uint16_t keep_alive_sec,
        std::vector<v5::property_variant> props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(
                v3_1_1::connect_message(
                    keep_alive_sec,
                    client_id,
                    clean_session_,
                    w,
                    user_name,
                    password
                )
            );
            break;
        case protocol_version::v5:
            do_sync_write(
                v5::connect_message(
                    keep_alive_sec,
                    client_id,
                    clean_session_,
                    w,
                    user_name,
                    password,
                    std::move(props)
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
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(
                v3_1_1::connack_message(
                    session_present,
                    reason_code
                )
            );
            break;
        case protocol_version::v5:
            do_sync_write(
                v5::connack_message(
                    session_present,
                    reason_code,
                    std::move(props)
                )
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_publish(
        as::const_buffer topic_name,
        std::uint8_t qos,
        bool retain,
        bool dup,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        as::const_buffer payload,
        mqtt::any life_keeper) {

        auto msg =
            basic_publish_message<PacketIdBytes>(
                topic_name,
                qos,
                retain,
                dup,
                packet_id,
                payload
            );

        auto do_send_publish =
            [&](auto msg, auto const& serialize_publish) {

                if (qos == qos::at_least_once || qos == qos::exactly_once) {
                    auto store_msg = msg;
                    store_msg.set_dup(true);
                    LockGuard<Mutex> lck (store_mtx_);
                    store_.emplace(
                        packet_id,
                        qos == qos::at_least_once
                         ? control_packet_type::puback
                         : control_packet_type::pubrec,
                        store_msg,
                        std::move(life_keeper)
                    );
                    if (serialize_publish) {
                        serialize_publish(store_msg);
                    }
                }
                do_sync_write(msg);
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            do_send_publish(
                v3_1_1::basic_publish_message<PacketIdBytes>(
                    topic_name,
                    qos,
                    retain,
                    dup,
                    packet_id,
                    payload
                ),
                h_serialize_publish_
            );
            break;
        case protocol_version::v5:
            do_send_publish(
                v5::basic_publish_message<PacketIdBytes>(
                    topic_name,
                    qos,
                    retain,
                    dup,
                    packet_id,
                    std::move(props),
                    payload
                ),
                h_serialize_v5_publish_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_puback(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_puback_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_puback_message<PacketIdBytes>(packet_id, reason, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        if (h_pub_res_sent_) h_pub_res_sent_(packet_id);
    }

    void send_pubrec(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_pubrec_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_pubrec_message<PacketIdBytes>(packet_id, reason, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pubrel(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {},
        mqtt::any life_keeper = mqtt::any()
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
                        std::move(life_keeper)
                    );
                    BOOST_ASSERT(ret.second);
                }

                if (serialize) {
                    serialize(msg);
                }
                do_sync_write(std::move(msg));
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                h_serialize_pubrel_
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, std::move(props)),
                h_serialize_v5_pubrel_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void store_pubrel(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
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
                        msg
                    );
                    BOOST_ASSERT(ret.second);
                }

                if (serialize) {
                    serialize(std::move(msg));
                }
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                h_serialize_pubrel_
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, std::move(props)),
                h_serialize_v5_pubrel_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_pubcomp(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_pubcomp_message<PacketIdBytes>(packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_pubcomp_message<PacketIdBytes>(packet_id, reason, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        if (h_pub_res_sent_) h_pub_res_sent_(packet_id);
    }

    template <typename... Args>
    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(topic_name, qos);
        send_subscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(as::buffer(topic_name.data(), topic_name.size()), qos);
        send_subscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props = {}
    ) {
        for(auto const& p : params)
        {
            BOOST_ASSERT(
                subscribe::get_qos(std::get<1>(p)) == qos::at_most_once ||
                subscribe::get_qos(std::get<1>(p)) == qos::at_least_once ||
                subscribe::get_qos(std::get<1>(p)) == qos::exactly_once
            );
        }
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    template <typename... Args>
    void send_suback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::uint8_t reason,
        Args&&... args) {
        params.push_back(reason);
        send_suback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_suback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_suback_message<PacketIdBytes>(std::move(params), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_suback_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    template <typename... Args>
    void send_unsubscribe(
        std::vector<as::const_buffer> params,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {
        params.emplace_back(topic_name);
        send_unsubscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void send_unsubscribe(
        std::vector<as::const_buffer> params,
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        Args&&... args) {
        params.emplace_back(as::buffer(topic_name.data(), topic_name.size()));
        send_unsubscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_unsubscribe(
        std::vector<as::const_buffer> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::basic_unsubscribe_message<PacketIdBytes>(std::move(params), packet_id));
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_unsubscribe_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)));
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

    template <typename... Args>
    void send_unsuback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::uint8_t reason,
        Args&&... args) {
        params.push_back(reason);
        send_suback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_unsuback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_sync_write(v5::basic_unsuback_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)));
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
            break;
        case protocol_version::v5:
            do_sync_write(v5::pingreq_message());
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
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_sync_write(v5::auth_message(reason, std::move(props)));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void send_disconnect(
        mqtt::optional<std::uint8_t> reason = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_sync_write(v3_1_1::disconnect_message());
            break;
        case protocol_version::v5:
            do_sync_write(v5::disconnect_message(reason, std::move(props)));
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
        if (h_pre_send_) h_pre_send_();
        socket_->write(const_buffer_sequence<PacketIdBytes>(std::forward<MessageVariant>(mv)), ec);
        if (ec) handle_error(ec);
    }

    // Non blocking (async) senders
    void async_send_connect(
        std::string const& client_id,
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
        mqtt::optional<will> const& w,
        bool clean_session,
        std::uint16_t keep_alive_sec,
        std::vector<v5::property_variant> props,
        async_handler_t func) {

        clean_session_ = clean_session;

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::connect_message(
                    keep_alive_sec,
                    client_id,
                    clean_session_,
                    w,
                    user_name,
                    password
                ),
                std::move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::connect_message(
                    keep_alive_sec,
                    client_id,
                    clean_session_,
                    w,
                    user_name,
                    password,
                    std::move(props)
                ),
                std::move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_connack(
        bool session_present,
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::connack_message(
                    session_present,
                    reason_code
                ),
                std::move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::connack_message(
                    session_present,
                    reason_code,
                    std::move(props)
                ),
                std::move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_publish(
        as::const_buffer topic_name,
        std::uint8_t qos,
        bool retain,
        bool dup,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        as::const_buffer payload,
        async_handler_t func,
        mqtt::any life_keeper) {

        auto do_async_send_publish =
            [&](auto msg, auto const& serialize_publish) {
                if (qos == qos::at_least_once || qos == qos::exactly_once) {
                    auto store_msg = msg;
                    store_msg.set_dup(true);
                    {
                        LockGuard<Mutex> lck (store_mtx_);
                        auto ret = store_.emplace(
                            packet_id,
                            qos == qos::at_least_once ? control_packet_type::puback
                                                      : control_packet_type::pubrec,
                            store_msg,
                            life_keeper
                        );
                        BOOST_ASSERT(ret.second);
                    }

                    if (serialize_publish) {
                        serialize_publish(store_msg);
                    }
                }
                do_async_write(
                    std::move(msg),
                    [life_keeper = std::move(life_keeper), func = std::move(func)](boost::system::error_code const& ec) {
                        if (func) func(ec);
                    }
                );
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_send_publish(
                v3_1_1::basic_publish_message<PacketIdBytes>(
                    topic_name,
                    qos,
                    retain,
                    dup,
                    packet_id,
                    payload
                ),
                h_serialize_publish_
            );
            break;
        case protocol_version::v5:
            do_async_send_publish(
                v5::basic_publish_message<PacketIdBytes>(
                    topic_name,
                    qos,
                    retain,
                    dup,
                    packet_id,
                    std::move(props),
                    payload
                ),
                h_serialize_v5_publish_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_puback(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {

        auto impl =
            [&] (auto msg) {
                auto self = shared_from_this();
                do_async_write(
                    std::move(msg),
                    [this, self, packet_id, func = std::move(func)]
                    (boost::system::error_code const& ec){
                        if (func) func(ec);
                        if (h_pub_res_sent_) h_pub_res_sent_(packet_id);
                    }
                );
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_puback_message<PacketIdBytes>(packet_id)
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_puback_message<PacketIdBytes>(packet_id, reason, std::move(props))
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubrec(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_pubrec_message<PacketIdBytes>(packet_id),
                std::move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_pubrec_message<PacketIdBytes>(packet_id, reason, std::move(props)),
                std::move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubrel(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason,
        std::vector<v5::property_variant> props,
        async_handler_t func,
        mqtt::any life_keeper = mqtt::any()
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
                        msg
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
                                    std::move(msg),
                                    life_keeper
                                );
                            }
                        );
                    }
                }

                if (serialize) {
                    serialize(msg);
                }
                do_async_write(
                    std::move(msg),
                    [life_keeper = std::move(life_keeper), func = std::move(func)](boost::system::error_code const& ec) {
                        if (func) func(ec);
                    }
                );
            };

        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubrel_message<PacketIdBytes>(packet_id),
                h_serialize_pubrel_
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubrel_message<PacketIdBytes>(packet_id, reason, std::move(props)),
                h_serialize_v5_pubrel_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pubcomp(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        auto impl =
            [&] (auto msg) {
                auto self = shared_from_this();
                do_async_write(
                    std::move(msg),
                    [this, self = std::move(self), packet_id, func = std::move(func)]
                    (boost::system::error_code const& ec){
                        if (func) func(ec);
                        if (h_pub_res_sent_) h_pub_res_sent_(packet_id);
                    }
                );
            };
        switch (version_) {
        case protocol_version::v3_1_1:
            impl(
                v3_1_1::basic_pubcomp_message<PacketIdBytes>(packet_id)
            );
            break;
        case protocol_version::v5:
            impl(
                v5::basic_pubcomp_message<PacketIdBytes>(packet_id, reason, std::move(props))
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    template <typename... Args>
    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(topic_name, qos);
        async_send_subscribe(std::move(params), std::move(life_keepers), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        std::string topic_name,
        std::uint8_t qos,
        Args&&... args) {

        auto sp_topic = std::make_shared<std::string>(std::move(topic_name));
        params.emplace_back(as::buffer(*sp_topic), qos);
        async_send_subscribe(std::move(params), std::make_pair(std::move(life_keepers), std::move(sp_topic)), packet_id, std::forward<Args>(args)...);
    }

    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        async_handler_t func) {

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id, {}),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        async_handler_t func) {

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_subscribe_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    template <typename... Args>
    void async_send_suback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        params.push_back(qos);
        async_send_suback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void async_send_suback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_suback_message<PacketIdBytes>(std::move(params), packet_id), std::move(func)
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_suback_message<PacketIdBytes>(std::move(params), packet_id, std::move(props)), std::move(func)
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    template <typename... Args>
    void async_send_unsubscribe(
        std::vector<as::const_buffer> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {
        params.emplace_back(topic_name);
        async_send_unsubscribe(std::move(params), std::move(life_keepers), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_send_unsubscribe(
        std::vector<as::const_buffer> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        std::string topic_name,
        Args&&... args) {

        auto sp_topic = std::make_shared<std::string>(std::move(topic_name));
        params.emplace_back(as::buffer(*sp_topic));
        async_send_unsubscribe(std::move(params), std::make_pair(std::move(life_keepers), std::move(sp_topic)), packet_id, std::forward<Args>(args)...);
    }

    void async_send_unsubscribe(
        std::vector<as::const_buffer> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        async_handler_t func) {

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_unsubscribe_message<PacketIdBytes>(
                    std::move(params),
                    packet_id
                ),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_unsubscribe_message<PacketIdBytes>(
                    std::move(params),
                    packet_id,
                    {}
                ),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_unsubscribe(
        std::vector<as::const_buffer> params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        async_handler_t func) {

        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_unsubscribe_message<PacketIdBytes>(
                    std::move(params),
                    packet_id
                ),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_unsubscribe_message<PacketIdBytes>(
                    std::move(params),
                    packet_id,
                    std::move(props)
                ),
                [life_keepers = std::move(life_keepers), func = std::move(func)]
                (boost::system::error_code const& ec) {
                    if (func) func(ec);
                }
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_unsuback(
        packet_id_t packet_id, async_handler_t func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(
                v3_1_1::basic_unsuback_message<PacketIdBytes>(packet_id), std::move(func)
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

    template <typename... Args>
    void async_send_unsuback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        params.push_back(qos);
        async_send_unsuback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void async_send_unsuback(
        std::vector<std::uint8_t> params,
        packet_id_t packet_id,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_async_write(
                v5::basic_unsuback_message<PacketIdBytes>(
                    std::move(params),
                    packet_id,
                    std::move(props)
                ),
                std::move(func)
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
            do_async_write(v3_1_1::pingreq_message(), std::move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::pingreq_message(), std::move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_pingresp(async_handler_t func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::pingresp_message(), std::move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::pingresp_message(), std::move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_auth(
        std::uint8_t reason,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            BOOST_ASSERT(false);
            break;
        case protocol_version::v5:
            do_async_write(v5::auth_message(reason, std::move(props)), std::move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_disconnect(
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::disconnect_message(), std::move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::disconnect_message(mqtt::nullopt, {}), std::move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_disconnect(
        std::uint8_t reason,
        std::vector<v5::property_variant> props,
        async_handler_t func
    ) {
        switch (version_) {
        case protocol_version::v3_1_1:
            do_async_write(v3_1_1::disconnect_message(), std::move(func));
            break;
        case protocol_version::v5:
            do_async_write(v5::disconnect_message(reason, std::move(props)), std::move(func));
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    void async_send_store(std::function<void()> func) {
        auto g = shared_scope_guard(
            [func = std::move(func)] {
                func();
            }
        );
        LockGuard<Mutex> lck (store_mtx_);
        auto const& idx = store_.template get<tag_seq>();
        for (auto const& e : idx) {
            do_async_write(
                e.message(),
                [g]
                (boost::system::error_code const& /*ec*/) {
                }
            );
        }
    }

    // Non blocking (async) write

    class async_packet {
    public:
        async_packet(
            basic_message_variant<PacketIdBytes> mv,
            async_handler_t h = async_handler_t())
            : mv_(std::move(mv))
            , handler_(std::move(h)) {}
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
            :self_(std::move(self)),
             func_(std::move(func)),
             num_of_messages_(num_of_messages),
             bytes_to_transfer_(expected)
        {}
        void operator()(boost::system::error_code const& ec) const {
            if (func_) func_(ec);
            for (std::size_t i = 0; i != num_of_messages_; ++i) {
                self_->queue_.pop_front();
            }
            if (ec || // Error is handled by async_read.
                !self_->connected_) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    self_->queue_.front().handler()(ec);
                    self_->queue_.pop_front();
                }
                return;
            }
            if (!self_->queue_.empty()) {
                self_->do_async_write();
            }
        }
        void operator()(
            boost::system::error_code const& ec,
            std::size_t bytes_transferred) const {
            if (func_) func_(ec);
            for (std::size_t i = 0; i != num_of_messages_; ++i) {
                self_->queue_.pop_front();
            }
            if (ec || // Error is handled by async_read.
                !self_->connected_) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    self_->queue_.front().handler()(ec);
                    self_->queue_.pop_front();
                }
                return;
            }
            if (bytes_to_transfer_ != bytes_transferred) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    self_->queue_.front().handler()(ec);
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
            std::size_t const size = mqtt::size<PacketIdBytes>(mv);

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

        if (h_pre_send_) h_pre_send_();

        socket_->async_write(
            std::move(buf),
            write_completion_handler(
                shared_from_this(),
                [handlers = std::move(handlers)]
                (boost::system::error_code const& ec) {
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
        auto self = shared_from_this();
        // Move this job to the socket's strand so that it can be queued without mutexes.
        socket_->post(
            [self = std::move(self), mv = std::move(mv), func = std::move(func)]
            () {
                if (!self->connected_) {
                    // offline async publish is successfully finished, because there's nothing to do.
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::success));
                    return;
                }
                self->queue_.emplace_back(std::move(mv), std::move(func));
                // Only need to start async writes if there was nothing in the queue before the above item.
                if (self->queue_.size() > 1) return;
                self->do_async_write();
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

protected:
    bool clean_session_{false};

private:
    mqtt::optional<mqtt::socket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> mqtt_connected_{false};

    std::array<char, 10>  buf_;
    std::uint8_t fixed_header_;
    std::size_t remaining_length_multiplier_;
    std::size_t remaining_length_;
    std::vector<char> payload_;

    // MQTT common handlers
    pingreq_handler h_pingreq_;
    pingresp_handler h_pingresp_;

    // MQTT v3_1_1 handlers
    connect_handler h_connect_;
    connack_handler h_connack_;
    publish_handler h_publish_;
    puback_handler h_puback_;
    pubrec_handler h_pubrec_;
    pubrel_handler h_pubrel_;
    pubcomp_handler h_pubcomp_;
    subscribe_handler h_subscribe_;
    suback_handler h_suback_;
    unsubscribe_handler h_unsubscribe_;
    unsuback_handler h_unsuback_;
    disconnect_handler h_disconnect_;

    // MQTT v5 handlers
    v5_connect_handler h_v5_connect_;
    v5_connack_handler h_v5_connack_;
    v5_publish_handler h_v5_publish_;
    v5_puback_handler h_v5_puback_;
    v5_pubrec_handler h_v5_pubrec_;
    v5_pubrel_handler h_v5_pubrel_;
    v5_pubcomp_handler h_v5_pubcomp_;
    v5_subscribe_handler h_v5_subscribe_;
    v5_suback_handler h_v5_suback_;
    v5_unsubscribe_handler h_v5_unsubscribe_;
    v5_unsuback_handler h_v5_unsuback_;
    v5_disconnect_handler h_v5_disconnect_;
    v5_auth_handler h_v5_auth_;

    // original handlers
    close_handler h_close_;
    error_handler h_error_;
    pub_res_sent_handler h_pub_res_sent_;
    serialize_publish_message_handler h_serialize_publish_;
    serialize_v5_publish_message_handler h_serialize_v5_publish_;
    serialize_pubrel_message_handler h_serialize_pubrel_;
    serialize_v5_pubrel_message_handler h_serialize_v5_pubrel_;
    serialize_remove_handler h_serialize_remove_;
    pre_send_handler h_pre_send_;
    is_valid_length_handler h_is_valid_length_;
    Mutex store_mtx_;
    mi_store store_;
    std::set<packet_id_t> qos2_publish_handled_;
    std::deque<async_packet> queue_;
    packet_id_t packet_id_master_{0};
    std::set<packet_id_t> packet_id_;
    bool auto_pub_response_{true};
    bool auto_pub_response_async_{false};
    bool async_send_store_ { false };
    bool disconnect_requested_{false};
    bool connect_requested_{false};
    std::size_t max_queue_send_count_{1};
    std::size_t max_queue_send_size_{0};
    mqtt_message_processed_handler h_mqtt_message_processed_;
    protocol_version version_{protocol_version::undetermined};
    std::size_t packet_bulk_read_limit_ = 256;
    std::size_t props_bulk_read_limit_ = packet_bulk_read_limit_;
};

} // namespace mqtt

#endif // MQTT_ENDPOINT_HPP

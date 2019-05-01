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
#include <mqtt/encoded_length.hpp>
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
#include <mqtt/property_parse.hpp>
#include <mqtt/protocol_version.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/subscribe.hpp>

#if defined(MQTT_USE_WS)
#include <mqtt/ws_endpoint.hpp>
#endif // defined(MQTT_USE_WS)

namespace mqtt {

namespace as = boost::asio;
namespace mi = boost::multi_index;

template <typename Socket, typename Mutex = std::mutex, template<typename...> class LockGuard = std::lock_guard, std::size_t PacketIdBytes = 2>
class endpoint : public std::enable_shared_from_this<endpoint<Socket, Mutex, LockGuard, PacketIdBytes>> {
    using this_type = endpoint<Socket, Mutex, LockGuard, PacketIdBytes>;
public:
    using async_handler_t = std::function<void(boost::system::error_code const& ec)>;
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    /**
     * @brief Constructor for client
     */
    endpoint(protocol_version version = protocol_version::undetermined)
        :h_mqtt_message_processed_(
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
    explicit endpoint(std::unique_ptr<Socket> socket, protocol_version version = protocol_version::undetermined)
        :socket_(std::move(socket))
        ,connected_(true)
        ,h_mqtt_message_processed_(
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
     * @param username
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
        bool(std::string const& client_id,
             mqtt::optional<std::string> const& username,
             mqtt::optional<std::string> const& password,
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
                                               std::string topic_name,
                                               std::string contents)>;

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
                                                 std::vector<std::tuple<std::string, std::uint8_t>> entries)>;

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
                                                   std::vector<std::string> topics)>;

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
     * @param username
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
        bool(std::string const& client_id,
             mqtt::optional<std::string> const& username,
             mqtt::optional<std::string> const& password,
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
             std::string topic_name,
             std::string contents,
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
             std::vector<std::tuple<std::string, std::uint8_t>> entries,
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
             std::vector<std::string> topics,
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
        BOOST_ASSERT(
            subscribe::get_qos(option) == qos::at_most_once ||
            subscribe::get_qos(option) == qos::at_least_once ||
            subscribe::get_qos(option) == qos::exactly_once
        );
        packet_id_t packet_id = acquire_unique_packet_id();
        acquired_subscribe(packet_id, as::buffer(topic_name.data(), topic_name.size()), option, std::forward<Args>(args)...);
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
        BOOST_ASSERT(
            subscribe::get_qos(option) == qos::at_most_once ||
            subscribe::get_qos(option) == qos::at_least_once ||
            subscribe::get_qos(option) == qos::exactly_once
        );
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
     * @param topic_name
     *        A topic name to subscribe
     * @param qos
     *        mqtt::qos
     * @param args
     *        args should be zero or more pairs of topic_name and qos.
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
        mqtt::string_view topic_name,
        std::uint8_t qos,
        Args&&... args) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_subscribe(packet_id, as::buffer(topic_name.data(), topic_name.size()), qos, std::forward<Args>(args)...);
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
     * @param qos
     *        mqtt::qos
     * @param args
     *        args should be zero or more pairs of topic_name and qos.
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
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        if (register_packet_id(packet_id)) {
            acquired_subscribe(packet_id, topic_name, qos, std::forward<Args>(args)...);
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
     * @param topic_name
     *        A topic name to subscribe
     * @param args
     *        args should be zero or more topics
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
        mqtt::string_view topic_name,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_unsubscribe(packet_id, topic_name, std::forward<Args>(args)...);
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
     *        args should be zero or more topics
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
        as::const_buffer topic_name,
        Args&&... args) {
        if (register_packet_id(packet_id)) {
            acquired_unsubscribe(packet_id, topic_name, std::forward<Args>(args)...);
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

        send_publish(
            topic_buf,
            qos::at_least_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
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

        send_publish(
            topic_buf,
            qos::exactly_once,
            retain,
            false,
            packet_id,
            std::move(props),
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
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
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        send_publish(
            topic_buf,
            qos,
            retain,
            false,
            packet_id,
            std::move(props),
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
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
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        BOOST_ASSERT((qos == qos::at_most_once && packet_id == 0) || (qos != qos::at_most_once && packet_id != 0));

        auto sp_topic_name = std::make_shared<std::string>(std::move(topic_name));
        auto sp_contents   = std::make_shared<std::string>(std::move(contents));

        auto topic_buf    = as::buffer(*sp_topic_name);
        auto contents_buf = as::buffer(*sp_contents);

        send_publish(
            topic_buf,
            qos,
            retain,
            true,
            packet_id,
            std::move(props),
            contents_buf,
            std::make_pair(std::move(sp_topic_name), std::move(sp_contents))
        );
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
     * @param topic_name
     *        A topic name to subscribe
     * @param qos
     *        mqtt::qos
     * @param args
     *        args should be zero or more pairs of topic_name and qos.
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
        mqtt::string_view topic_name,
        std::uint8_t qos, Args&&... args) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);
        send_subscribe(std::move(params), packet_id, as::buffer(topic_name.data(), topic_name.size()), qos, std::forward<Args>(args)...);
    }

    /**
     * @brief Subscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param qos
     *        mqtt::qos
     * @param args
     *        args should be zero or more pairs of topic_name and qos.
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901161<BR>
     */
    template <typename... Args>
    void acquired_subscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos, Args&&... args) {
        BOOST_ASSERT(qos == qos::at_most_once || qos == qos::at_least_once || qos == qos::exactly_once);
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params;
        params.reserve((sizeof...(args) + 2) / 2);
        send_subscribe(std::move(params), packet_id, topic_name, qos, std::forward<Args>(args)...);
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
     * @param topic_name
     *        A topic name to subscribe
     * @param args
     *        args should be zero or more topics
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    void acquired_unsubscribe(
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        Args&&... args) {
        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args) + 1);
        send_unsubscribe(std::move(params), packet_id, as::buffer(topic_name.data(), topic_name.size()), std::forward<Args>(args)...);
    }

    /**
     * @brief Unsubscribe with already acquired packet identifier
     * @param packet_id
     *        packet identifier. It should be acquired by acquire_unique_packet_id, or register_packet_id.
     *        The ownership of  the packet_id moves to the library.
     * @param topic_name
     *        A topic name to subscribe
     * @param args
     *        args should be zero or more topics
     *        You can set props as the last argument optionally.
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * You can subscribe multiple topics all at once.<BR>
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901179
     */
    template <typename... Args>
    void acquired_unsubscribe(
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {
        std::vector<as::const_buffer> params;
        params.reserve(sizeof...(args) + 1);
        send_unsubscribe(std::move(params), packet_id, topic_name, std::forward<Args>(args)...);
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

        for (auto&& e : params) {
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
     */
    void pubrel(
        packet_id_t packet_id,
        mqtt::optional<std::uint8_t> reason_code = mqtt::nullopt,
        std::vector<v5::property_variant> props = {}
    ) {
        send_pubrel(packet_id, reason_code, std::move(props));
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
        bool retain = false,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, topic_name, contents, mqtt::any(), qos::at_most_once, retain, std::move(func));
    }

    /**
     * @brief Publish QoS0
     *        topic_name and contents are reference type. So caller need to keep the lifetime of them
     *        until func is called.
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
        as::const_buffer topic_name,
        as::const_buffer contents,
        bool retain,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        acquired_async_publish(0, topic_name, contents, mqtt::any(), qos::at_most_once, retain, std::move(props), std::move(func));
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
            life_keeper
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
            life_keeper
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

        for (auto&& e : params) {
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

        for (auto&& e : params) {
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

        for (auto&& e : params) {
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

        for (auto&& e : params) {
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
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
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
     * See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718043
     */
    void async_pubrel(
        packet_id_t packet_id,
        std::uint8_t reason_code,
        std::vector<v5::property_variant> props,
        async_handler_t func = async_handler_t()
    ) {
        async_send_pubrel(packet_id, reason_code, std::move(props), std::move(func));
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
     * @brief Get Socket unique_ptr reference.
     * @return refereence of Socket unique_ptr
     */
    std::unique_ptr<Socket>& socket() {
        return socket_;
    }

    /**
     * @brief Get Socket unique_ptr const reference.
     * @return const refereence of Socket unique_ptr
     */
    std::unique_ptr<Socket> const& socket() const {
        return socket_;
    }


    /**
     * @brief Apply f to stored messages.
     * @param f applying function. f should be void(char const*, std::size_t)
     */
    void for_each_store(std::function<void(char const*, std::size_t)> const& f) {
        LockGuard<Mutex> lck (store_mtx_);
        auto& idx = store_.template get<tag_seq>();
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
        auto& idx = store_.template get<tag_seq>();
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
        if (b == e) return;

        auto fixed_header = static_cast<std::uint8_t>(*b);
        switch (get_control_packet_type(fixed_header)) {
        case control_packet_type::publish: {
            auto sp = std::make_shared<std::string>(b, e);
            restore_serialized_message(basic_publish_message<PacketIdBytes>(sp->begin(), sp->end()), sp);
        } break;
        case control_packet_type::pubrel: {
            restore_serialized_message(basic_pubrel_message<PacketIdBytes>(b, e));
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
            auto sp = std::make_shared<std::string>(b, e);
            restore_v5_serialized_message(v5::basic_publish_message<PacketIdBytes>(sp->begin(), sp->end()), sp);
        } break;
        case control_packet_type::pubrel: {
            auto sp = std::make_shared<std::string>(b, e);
            restore_v5_serialized_message(v5::basic_pubrel_message<PacketIdBytes>(sp->begin(), sp->end()), sp);
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
        mqtt_message_processed_handler const& h = mqtt_message_processed_handler()) {
        if (h) {
            h_mqtt_message_processed_ = h;
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

protected:
    void async_read_control_packet_type(async_handler_t func) {
        async_read(
            *socket_,
            as::buffer(&buf_, 1),
            [self = this->shared_from_this(), func = std::move(func)](
                boost::system::error_code const& ec,
                std::size_t bytes_transferred){
                if (self->handle_close_or_error(ec)) {
                    if (func) func(ec);
                    return;
                }
                if (bytes_transferred != 1) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return;
                }
                self->handle_control_packet_type(std::move(func));
            }
        );
    }

    bool handle_close_or_error(boost::system::error_code const& ec) {
        if (!ec) return false;
        if (connected_) {
            connected_ = false;
            mqtt_connected_ = false;
            shutdown_from_server(*socket_);
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
    template <typename T>
    void shutdown_from_client(T& socket) {
        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }

    template <typename T>
    void shutdown_from_server(T& socket) {
        boost::system::error_code ec;
        socket.close(ec);
    }

#if defined(MQTT_USE_WS)
    template <typename T, typename S>
    void shutdown_from_server(ws_endpoint<T, S>& /*socket*/) {
    }
#endif // defined(MQTT_USE_WS)

#if !defined(MQTT_NO_TLS)
    template <typename T>
    void shutdown_from_client(as::ssl::stream<T>& socket) {
        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }
    template <typename T>
    void shutdown_from_server(as::ssl::stream<T>& socket) {
        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }
#if defined(MQTT_USE_WS)
    template <typename T, typename S>
    void shutdown_from_client(ws_endpoint<as::ssl::stream<T>, S>& socket) {
        boost::system::error_code ec;
        socket.lowest_layer().close(ec);
    }
    template <typename T, typename S>
    void shutdown_from_server(ws_endpoint<as::ssl::stream<T>, S>& /*socket*/) {
    }
#endif // defined(MQTT_USE_WS)
#endif // defined(MQTT_NO_TLS)


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
        async_send_suback(std::vector<std::uint8_t>(), packet_id, qos, std::forward<Args>(args)...);
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
        async_send_unsuback(std::vector<std::uint8_t>(), packet_id, qos, std::forward<Args>(args)...);
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

    void handle_control_packet_type(async_handler_t func) {
        fixed_header_ = static_cast<std::uint8_t>(buf_);
        remaining_length_ = 0;
        remaining_length_multiplier_ = 1;
        async_read(
            *socket_,
            as::buffer(&buf_, 1),
            [self = this->shared_from_this(), func = std::move(func)](
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
                self->handle_remaining_length(std::move(func));
            }
        );
    }

    void handle_remaining_length(async_handler_t func) {
        remaining_length_ += (buf_ & 0b01111111) * remaining_length_multiplier_;
        remaining_length_multiplier_ *= 128;
        if (remaining_length_multiplier_ > 128 * 128 * 128 * 128) {
            handle_error(boost::system::errc::make_error_code(boost::system::errc::message_size));
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return;
        }
        if (buf_ & 0b10000000) {
            async_read(
                *socket_,
                as::buffer(&buf_, 1),
                [self = this->shared_from_this(), func = std::move(func)](
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
                    self->handle_remaining_length(std::move(func));
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
            /*
             * Note that this call to resize will never shrink the vector's allocated memory
             * The standard guarentees that iterators to items that are still valid with the
             * new size will remain valid. There are only two ways to provide that guarantee
             * - Ensure that unused memory is only freed with calls to the memory allocator
             *   that will reclaim the unused memory, and not move items to a new buffer.
             * - Ensure that calls to resize only call the memory allocator when growing.
             * Testing by redboltz shows that the second possibility is the case for at least
             * one implementation of the standard library, and it's unlikely that there would
             * be a difference in this behavior between implementations.
             */
            payload_.resize(remaining_length_);
            if (remaining_length_ == 0) {
                handle_payload(func);
                return;
            }
            async_read(
                *socket_,
                as::buffer(payload_),
                [self = this->shared_from_this(), func = std::move(func)](
                    boost::system::error_code const& ec,
                    std::size_t bytes_transferred){
                    auto g = unique_scope_guard(
                        [&self]
                        {
                            self->payload_.clear();
                        }
                    );
                    if (self->handle_close_or_error(ec)) {
                        if (func) func(ec);
                        return;
                    }
                    if (bytes_transferred != self->remaining_length_) {
                        self->handle_error(boost::system::errc::make_error_code(boost::system::errc::message_size));
                        if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                        return;
                    }
                    self->handle_payload(std::move(func));
                }
            );
        }
    }

    void handle_payload(async_handler_t func) {
        auto control_packet_type = get_control_packet_type(fixed_header_);
        bool ret = false;
        switch (control_packet_type) {
        case control_packet_type::connect:
            ret = handle_connect(func);
            break;
        case control_packet_type::connack:
            ret = handle_connack(func);
            break;
        case control_packet_type::publish:
            if (mqtt_connected_) {
                ret = handle_publish(func);
            }
            break;
        case control_packet_type::puback:
            if (mqtt_connected_) {
                ret = handle_puback(func);
            }
            break;
        case control_packet_type::pubrec:
            if (mqtt_connected_) {
                ret = handle_pubrec(func);
            }
            break;
        case control_packet_type::pubrel:
            if (mqtt_connected_) {
                ret = handle_pubrel(func);
            }
            break;
        case control_packet_type::pubcomp:
            if (mqtt_connected_) {
                ret = handle_pubcomp(func);
            }
            break;
        case control_packet_type::subscribe:
            if (mqtt_connected_) {
                ret = handle_subscribe(func);
            }
            break;
        case control_packet_type::suback:
            if (mqtt_connected_) {
                ret = handle_suback(func);
            }
            break;
        case control_packet_type::unsubscribe:
            if (mqtt_connected_) {
                ret = handle_unsubscribe(func);
            }
            break;
        case control_packet_type::unsuback:
            if (mqtt_connected_) {
                ret = handle_unsuback(func);
            }
            break;
        case control_packet_type::pingreq:
            if (mqtt_connected_) {
                ret = handle_pingreq(func);
            }
            break;
        case control_packet_type::pingresp:
            if (mqtt_connected_) {
                ret = handle_pingresp(func);
            }
            break;
        case control_packet_type::disconnect:
            handle_disconnect(func);
            ret = false;
            break;
        case control_packet_type::auth:
            ret = handle_auth(func);
            break;
        default:
            break;
        }
        if (ret) {
            h_mqtt_message_processed_(std::move(func));
        }
        else if (func) {
            func(boost::system::errc::make_error_code(boost::system::errc::success));
        }
    }

    bool handle_connect(async_handler_t const& func) {
        std::size_t i = 0;
        if (remaining_length_ < 10 || // *1
            payload_[i++] != 0x00 ||
            payload_[i++] != 0x04 ||
            payload_[i++] != 'M' ||
            payload_[i++] != 'Q' ||
            payload_[i++] != 'T' ||
            payload_[i++] != 'T') {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
            return false;
        }

        auto version = static_cast<protocol_version>(payload_[i++]);
        if (version != protocol_version::v3_1_1 && version != protocol_version::v5) {
            if (func) {
                func(boost::system::errc::make_error_code(boost::system::errc::protocol_not_supported));
            }
            return false;
        }

        if (version_ == protocol_version::undetermined) {
            version_ = version;
        }
        else if (version_ != version) {
            if (func) {
                func(boost::system::errc::make_error_code(boost::system::errc::protocol_not_supported));
            }
            return false;
        }

        const char byte8 = payload_[i++];

        const std::uint16_t keep_alive = make_uint16_t(payload_[i], payload_[i + 1]); // index is checked at *1
        i += 2;

        std::vector<v5::property_variant> props;
        if (version_ == protocol_version::v5) {
            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            i += static_cast<std::size_t>(std::distance(b, it));
        }

        if (remaining_length_ < i + 2) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        const std::uint16_t client_id_length = make_uint16_t(payload_[i], payload_[i + 1]);
        i += 2;

        if (remaining_length_ < i + client_id_length) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        std::string client_id(payload_.data() + i, client_id_length);
        if (utf8string::validate_contents(client_id) != utf8string::validation::well_formed) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
            return false;
        }
        i += client_id_length;

        clean_session_ = connect_flags::has_clean_session(byte8);
        mqtt::optional<will> w;
        if (connect_flags::has_will_flag(byte8)) {

            std::vector<v5::property_variant> will_props;
            if (version_ == protocol_version::v5) {
                char const* b = payload_.data() + i;
                char const* it = b;
                char const* e = b + payload_.size() - i;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    will_props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                i += static_cast<std::size_t>(std::distance(b, it));
            }

            if (remaining_length_ < i + 2) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            const std::uint16_t topic_name_length = make_uint16_t(payload_[i], payload_[i + 1]);
            i += 2;

            if (remaining_length_ < i + topic_name_length) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            std::string topic_name(payload_.data() + i, topic_name_length);
            if (utf8string::validate_contents(topic_name) != utf8string::validation::well_formed) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
                return false;
            }

            i += topic_name_length;

            if (remaining_length_ < i + 2) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            std::uint16_t will_message_length = make_uint16_t(payload_[i], payload_[i + 1]);
            i += 2;

            if (remaining_length_ < i + will_message_length) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            std::string will_message(payload_.data() + i, will_message_length);
            i += will_message_length;
            w = will(topic_name,
                     will_message,
                     connect_flags::has_will_retain(byte8),
                     connect_flags::will_qos(byte8),
                     std::move(will_props));
        }

        mqtt::optional<std::string> user_name;
        if (connect_flags::has_user_name_flag(byte8)) {

            if (remaining_length_ < i + 2) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            const std::uint16_t user_name_length = make_uint16_t(payload_[i], payload_[i + 1]);
            i += 2;

            if (remaining_length_ < i + user_name_length) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            user_name = std::string(payload_.data() + i, user_name_length);
            if (utf8string::validate_contents(user_name.value()) != utf8string::validation::well_formed) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
                return false;
            }
            i += user_name_length;
        }

        mqtt::optional<std::string> password;
        if (connect_flags::has_password_flag(byte8)) {

            if (remaining_length_ < i + 2) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            const std::uint16_t password_length = make_uint16_t(payload_[i], payload_[i + 1]);
            i += 2;

            if (remaining_length_ < i + password_length) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            password = std::string(payload_.data() + i, password_length);
            i += password_length;
        }
        mqtt_connected_ = true;

        if (clean_session_) {
            LockGuard<Mutex> lck (store_mtx_);
            store_.clear();
            packet_id_.clear();
        }

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_connect_) {
                if (h_connect_(
                        client_id,
                        user_name,
                        password,
                        std::move(w),
                        clean_session_,
                        keep_alive)
                ) {
                    return true;
                }
                return false;
            }
            break;
        case protocol_version::v5:
            if (h_v5_connect_) {
                if (h_v5_connect_(
                        client_id,
                        user_name,
                        password,
                        std::move(w),
                        clean_session_,
                        keep_alive,
                        props)
                ) {
                    return true;
                }
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_connack(async_handler_t const& func) {
        if (!connect_requested_) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
            return false;
        }
        connect_requested_ = false;

        if (remaining_length_ < 2) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        if (static_cast<std::uint8_t>(payload_[1]) == connect_return_code::accepted) {
            if (clean_session_) {
                LockGuard<Mutex> lck (store_mtx_);
                store_.clear();
                packet_id_.clear();
            }
            else {
                LockGuard<Mutex> lck (store_mtx_);
                auto& idx = store_.template get<tag_seq>();
                for (auto const& e : idx) {
                    do_sync_write(e.message());
                }
            }
        }
        bool session_present = is_session_present(payload_[0]);
        mqtt_connected_ = true;

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_connack_) {
                return
                    h_connack_(
                        session_present,
                        static_cast<std::uint8_t>(payload_[1])
                    );
            }
            break;
        case protocol_version::v5:
            if (h_v5_connack_) {
                std::vector<v5::property_variant> props;
                char const* b = payload_.data() + 2;
                char const* it = b;
                char const* e = b + payload_.size() - 2;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                return
                    h_v5_connack_(
                        session_present,
                        static_cast<std::uint8_t>(payload_[1]),
                        std::move(props)
                    );
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }

        return true;
    }

    template <typename F, typename AF>
    void auto_pub_response(F const& f, AF const& af) {
        if (auto_pub_response_) {
            if (auto_pub_response_async_) af();
            else f();
        }
    }

    bool handle_publish(async_handler_t const& func) {
        if (remaining_length_ < 2) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        std::size_t i = 0;
        std::uint16_t topic_name_length = make_uint16_t(payload_[i], payload_[i + 1]);
        i += 2;

        if (remaining_length_ < i + topic_name_length) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        std::string topic_name(payload_.data() + i, topic_name_length);
        if (utf8string::validate_contents(topic_name) != utf8string::validation::well_formed) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
            return false;
        }
        i += topic_name_length;

        mqtt::optional<packet_id_t> packet_id;
        auto qos = publish::get_qos(fixed_header_);

        auto handler_call =
            [&] {
                switch (version_) {
                case protocol_version::v3_1_1:
                    if (h_publish_) {
                        std::string contents(payload_.data() + i, payload_.size() - i);
                        return h_publish_(fixed_header_, packet_id, std::move(topic_name), std::move(contents));
                    }
                    break;
                case protocol_version::v5:
                    if (h_v5_publish_) {
                        std::vector<v5::property_variant> props;
                        char const* b = payload_.data() + i;
                        char const* it = b;
                        char const* e = b + payload_.size() - i;
                        if (auto props_opt = v5::property::parse_with_length(it, e)) {
                            props = std::move(*props_opt);
                        }
                        else {
                            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                            return false;
                        }
                        i += static_cast<std::size_t>(std::distance(b, it));
                        std::string contents(payload_.data() + i, payload_.size() - i);
                        return h_v5_publish_(fixed_header_, packet_id, std::move(topic_name), std::move(contents), std::move(props));
                    }
                    break;
                default:
                    BOOST_ASSERT(false);
                    return false;
                }
                return true;
            };

        switch (qos) {
        case qos::at_most_once:
            return handler_call();
        case qos::at_least_once: {
            if (remaining_length_ < i + sizeof(packet_id_t)) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            packet_id = make_packet_id<PacketIdBytes>::apply(
                payload_.data() + i,
                payload_.data() + i + sizeof(packet_id_t)
            );
            i += sizeof(packet_id_t);
            auto res = [this, &packet_id, &func] {
                auto_pub_response(
                    [this, &packet_id] {
                        if (connected_) send_puback(*packet_id);
                    },
                    [this, &packet_id, &func] {
                        if (connected_) async_send_puback(*packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, func);
                    }
                );
            };
            if (handler_call()) {
                res();
                return true;
            }
            return false;
        } break;
        case qos::exactly_once: {
            if (remaining_length_ < i + sizeof(packet_id_t)) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            packet_id = make_packet_id<PacketIdBytes>::apply(
                payload_.data() + i,
                payload_.data() + i + sizeof(packet_id_t)
            );
            i += sizeof(packet_id_t);
            auto res = [this, &packet_id, &func] {
                auto_pub_response(
                    [this, &packet_id] {
                        if (connected_) send_pubrec(*packet_id);
                    },
                    [this, &packet_id, &func] {
                        if (connected_) async_send_pubrec(*packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, func);
                    }
                );
            };

            switch (version_) {
            case protocol_version::v3_1_1:
                if (h_publish_) {
                    auto it = qos2_publish_handled_.find(*packet_id);
                    if (it == qos2_publish_handled_.end()) {
                        std::string contents(payload_.data() + i, payload_.size() - i);
                        if (h_publish_(fixed_header_, packet_id, std::move(topic_name), std::move(contents))) {
                            qos2_publish_handled_.emplace(*packet_id);
                            res();
                            return true;
                        }
                        return false;
                    }
                }
                break;
            case protocol_version::v5:
                if (h_v5_publish_) {
                    auto it = qos2_publish_handled_.find(*packet_id);
                    if (it == qos2_publish_handled_.end()) {
                        std::vector<v5::property_variant> props;
                        char const* b = payload_.data() + i;
                        char const* it = b;
                        char const* e = b + payload_.size() - i;
                        if (auto props_opt = v5::property::parse_with_length(it, e)) {
                            props = std::move(*props_opt);
                        }
                        else {
                            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                            return false;
                        }
                        i += static_cast<std::size_t>(std::distance(b, it));
                        std::string contents(payload_.data() + i, payload_.size() - i);
                        if (h_v5_publish_(fixed_header_, packet_id, std::move(topic_name), std::move(contents), std::move(props))) {
                            qos2_publish_handled_.emplace(*packet_id);
                            res();
                            return true;
                        }
                        return false;
                    }
                }
                break;
            default:
                BOOST_ASSERT(false);
                return false;
            }
            res();
        } break;
        default:
            break;
        }
        return true;
    }

    bool handle_puback(async_handler_t const& func) {
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }

        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        {
            LockGuard<Mutex> lck (store_mtx_);
            auto& idx = store_.template get<tag_packet_id_type>();
            auto r = idx.equal_range(std::make_tuple(packet_id, control_packet_type::puback));
            idx.erase(std::get<0>(r), std::get<1>(r));
            packet_id_.erase(packet_id);
        }
        if (h_serialize_remove_) h_serialize_remove_(packet_id);

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_puback_) {
                return h_puback_(packet_id);
            }
            break;
        case protocol_version::v5:
            if (h_v5_puback_) {
                std::vector<v5::property_variant> props;

                if (remaining_length_ == sizeof(packet_id_t)) {
                    return h_v5_puback_(packet_id, v5::reason_code::success, std::move(props));
                }

                char const* b = payload_.data() + 3;
                char const* it = b;
                char const* e = b + payload_.size() - 3;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                return h_v5_puback_(packet_id, static_cast<std::uint8_t>(payload_[sizeof(packet_id_t)]), std::move(props));
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_pubrec(async_handler_t const& func) {
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }

        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        {
            LockGuard<Mutex> lck (store_mtx_);
            auto& idx = store_.template get<tag_packet_id_type>();
            auto r = idx.equal_range(std::make_tuple(packet_id, control_packet_type::pubrec));
            idx.erase(std::get<0>(r), std::get<1>(r));
            // packet_id shouldn't be erased here.
            // It is reused for pubrel/pubcomp.
        }
        auto res = [this, &packet_id, &func] {
            auto_pub_response(
                [this, &packet_id] {
                    if (connected_) send_pubrel(packet_id);
                    else store_pubrel(packet_id);
                },
                [this, &packet_id, &func] {
                    if (connected_) async_send_pubrel(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, func);
                    else store_pubrel(packet_id);
                }
            );
        };

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_pubrec_) {
                if (h_pubrec_(packet_id)) {
                    res();
                    return true;
                }
                return false;
            }
            break;
        case protocol_version::v5:
            if (h_v5_pubrec_) {
                std::vector<v5::property_variant> props;

                if (remaining_length_ == sizeof(packet_id_t)) {
                    if (h_v5_pubrec_(packet_id, v5::reason_code::success, std::move(props))) {
                        res();
                        return true;
                    }
                    return false;
                }

                char const* b = payload_.data() + 3;
                char const* it = b;
                char const* e = b + payload_.size() - 3;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                if (h_v5_pubrec_(packet_id, static_cast<std::uint8_t>(payload_[sizeof(packet_id_t)]), std::move(props))) {
                    res();
                    return true;
                }
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        res();
        return true;
    }

    bool handle_pubrel(async_handler_t const& func) {
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }

        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        auto res = [this, &packet_id, &func] {
            auto_pub_response(
                [this, &packet_id] {
                    if (connected_) send_pubcomp(packet_id);
                },
                [this, &packet_id, &func] {
                    if (connected_) async_send_pubcomp(packet_id, mqtt::nullopt, std::vector<v5::property_variant>{}, func);
                }
            );
        };
        qos2_publish_handled_.erase(packet_id);
        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_pubrel_) {
                if (h_pubrel_(packet_id)) {
                    res();
                    return true;
                }
                return false;
            }
            break;
        case protocol_version::v5:
            if (h_v5_pubrel_) {
                std::vector<v5::property_variant> props;

                if (remaining_length_ == sizeof(packet_id_t)) {
                    if (h_v5_pubrel_(packet_id, v5::reason_code::success, std::move(props))) {
                        res();
                        return true;
                    }
                    return false;
                }

                char const* b = payload_.data() + 3;
                char const* it = b;
                char const* e = b + payload_.size() - 3;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                if (h_v5_pubrel_(packet_id, static_cast<std::uint8_t>(payload_[sizeof(packet_id_t)]), std::move(props))) {
                    res();
                    return true;
                }
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        res();
        return true;
    }

    bool handle_pubcomp(async_handler_t const& func) {
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }

        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        {
            LockGuard<Mutex> lck (store_mtx_);
            auto& idx = store_.template get<tag_packet_id_type>();
            auto r = idx.equal_range(std::make_tuple(packet_id, control_packet_type::pubcomp));
            idx.erase(std::get<0>(r), std::get<1>(r));
            packet_id_.erase(packet_id);
        }
        if (h_serialize_remove_) h_serialize_remove_(packet_id);
        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_pubcomp_) {
                return h_pubcomp_(packet_id);
            }
            break;
        case protocol_version::v5:
            if (h_v5_pubcomp_) {
                std::vector<v5::property_variant> props;

                if (remaining_length_ == sizeof(packet_id_t)) {
                    return h_v5_pubcomp_(packet_id, v5::reason_code::success, std::move(props));
                }

                char const* b = payload_.data() + 3;
                char const* it = b;
                char const* e = b + payload_.size() - 3;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                return h_v5_pubcomp_(packet_id, static_cast<std::uint8_t>(payload_[sizeof(packet_id_t)]), std::move(props));
            }
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_subscribe(async_handler_t const& func) {
        std::size_t i = 0;
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        i += sizeof(packet_id_t);

        switch (version_) {
        case protocol_version::v3_1_1: {
            std::vector<std::tuple<std::string, std::uint8_t>> entries;
            while (i < remaining_length_) {
                if (remaining_length_ < i + 2) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                std::uint16_t topic_length = make_uint16_t(payload_[i], payload_[i + 1]);
                i += 2;

                if (remaining_length_ < i + topic_length) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                std::string topic_filter(payload_.data() + i, topic_length);
                if (utf8string::validate_contents(topic_filter) != utf8string::validation::well_formed) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
                    return false;
                }
                i += topic_length;

                if (remaining_length_ < i + 1) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }

                std::uint8_t options = static_cast<std::uint8_t>(payload_[i]);
                if ((options & 0b11111100) != 0) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
                    return false;
                }
                entries.emplace_back(std::move(topic_filter), options);
                ++i;
            }
            if (h_subscribe_) return h_subscribe_(packet_id, std::move(entries));
        } break;
        case protocol_version::v5: {
            std::vector<v5::property_variant> props;
            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            i += static_cast<std::size_t>(std::distance(b, it));

            std::vector<std::tuple<std::string, std::uint8_t>> entries;
            while (i < remaining_length_) {
                if (remaining_length_ < i + 2) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                std::uint16_t topic_length = make_uint16_t(payload_[i], payload_[i + 1]);
                i += 2;

                if (remaining_length_ < i + topic_length) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                std::string topic_filter(payload_.data() + i, topic_length);
                if (utf8string::validate_contents(topic_filter) != utf8string::validation::well_formed) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
                    return false;
                }
                i += topic_length;

                if (remaining_length_ < i + 1) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }

                std::uint8_t options = static_cast<std::uint8_t>(payload_[i]);
                if ((options & 0b11000000) != 0) {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
                    return false;
                }
                entries.emplace_back(std::move(topic_filter), options);
                ++i;
            }
            if (h_v5_subscribe_) return h_v5_subscribe_(packet_id, std::move(entries), std::move(props));
        } break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_suback(async_handler_t const& func) {
        std::size_t i = 0;
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        i += sizeof(packet_id_t);
        {
            LockGuard<Mutex> lck (store_mtx_);
            packet_id_.erase(packet_id);
        }

        std::vector<v5::property_variant> props;
        if (version_ == protocol_version::v5) {
            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            i += static_cast<std::size_t>(std::distance(b, it));
        }

        switch (version_) {
        case protocol_version::v3_1_1: {
            std::vector<mqtt::optional<std::uint8_t>> results;
            results.reserve(payload_.size() - i);
            auto it = payload_.cbegin() + static_cast<std::vector<char>::difference_type>(i);
            auto end = payload_.cend();
            for (; it != end; ++it) {
                if (*it & 0b10000000) {
                    results.push_back(mqtt::nullopt);
                }
                else {
                    results.push_back(static_cast<std::uint8_t>(*it));
                }
            }
            if (h_suback_) return h_suback_(packet_id, std::move(results));
        } break;
        case protocol_version::v5: {
            std::vector<std::uint8_t> reasons;
            reasons.reserve(payload_.size() - i);
            auto it = payload_.cbegin() + static_cast<std::vector<char>::difference_type>(i);
            auto end = payload_.cend();
            std::copy(it, end, std::back_inserter(reasons));
            if (h_v5_suback_) return h_v5_suback_(packet_id, std::move(reasons), std::move(props));
        } break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_unsubscribe(async_handler_t const& func) {
        std::size_t i = 0;
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        i += sizeof(packet_id_t);

        std::vector<v5::property_variant> props;
        if (version_ == protocol_version::v5) {
            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            i += static_cast<std::size_t>(std::distance(b, it));
        }

        std::vector<std::string> topic_filters;
        while (i < remaining_length_) {
            if (remaining_length_ < i + 2) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            std::uint16_t topic_length = make_uint16_t(payload_[i], payload_[i + 1]);
            i += 2;
            if (remaining_length_ < i + topic_length) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }
            std::string topic_filter(payload_.data() + i, topic_length);
            if (utf8string::validate_contents(topic_filter) != utf8string::validation::well_formed) {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::bad_message));
                return false;
            }
            i += topic_length;

            topic_filters.emplace_back(std::move(topic_filter));
        }

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_unsubscribe_) return h_unsubscribe_(packet_id, std::move(topic_filters));
            break;
        case protocol_version::v5:
            if (h_v5_unsubscribe_) return h_v5_unsubscribe_(packet_id, std::move(topic_filters), std::move(props));
            break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_unsuback(async_handler_t const& func) {
        std::size_t i = 0;
        if (remaining_length_ < sizeof(packet_id_t)) {
            if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
            return false;
        }
        packet_id_t packet_id = make_packet_id<PacketIdBytes>::apply(
            payload_.data(),
            payload_.data() + sizeof(packet_id_t)
        );
        i += sizeof(packet_id_t);

        {
            LockGuard<Mutex> lck (store_mtx_);
            packet_id_.erase(packet_id);
        }

        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_unsuback_) return h_unsuback_(packet_id);
            break;
        case protocol_version::v5: {
            std::vector<v5::property_variant> props;
            {
                char const* b = payload_.data() + i;
                char const* it = b;
                char const* e = b + payload_.size() - i;
                if (auto props_opt = v5::property::parse_with_length(it, e)) {
                    props = std::move(*props_opt);
                }
                else {
                    if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                    return false;
                }
                i += static_cast<std::size_t>(std::distance(b, it));
            }
            std::vector<std::uint8_t> reasons;
            {
                reasons.reserve(payload_.size() - i);
                auto it = payload_.cbegin() + static_cast<std::vector<char>::difference_type>(i);
                auto end = payload_.cend();
                std::copy(it, end, std::back_inserter(reasons));
            }
            if (h_v5_unsuback_) return h_v5_unsuback_(packet_id, std::move(reasons), std::move(props));
        } break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
    }

    bool handle_pingreq(async_handler_t const& /*func*/) {
        if (h_pingreq_) return h_pingreq_();
        return true;
    }

    bool handle_pingresp(async_handler_t const& /*func*/) {
        if (h_pingresp_) return h_pingresp_();
        return true;
    }

    void handle_disconnect(async_handler_t const& func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            if (h_disconnect_) h_disconnect_();
            break;
        case protocol_version::v5: {
            std::size_t i = 0;
            std::vector<v5::property_variant> props;
            if (remaining_length_ < 1) {
                if (h_v5_disconnect_) h_v5_disconnect_(v5::reason_code::normal_disconnection, std::move(props));
                return;
            }

            auto reason = static_cast<std::uint8_t>(payload_[i]);
            ++i;

            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return;
            }

            if (h_v5_disconnect_) h_v5_disconnect_(reason, std::move(props));
        } break;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }

    bool handle_auth(async_handler_t const& func) {
        switch (version_) {
        case protocol_version::v3_1_1:
            return false;
        case protocol_version::v5: {
            std::size_t i = 0;
            std::vector<v5::property_variant> props;
            if (remaining_length_ < 1) {
                if (h_v5_auth_) h_v5_auth_(v5::reason_code::success, std::move(props));
                return true;
            }

            auto reason = static_cast<std::uint8_t>(payload_[i]);
            ++i;

            char const* b = payload_.data() + i;
            char const* it = b;
            char const* e = b + payload_.size() - i;
            if (auto props_opt = v5::property::parse_with_length(it, e)) {
                props = std::move(*props_opt);
            }
            else {
                if (func) func(boost::system::errc::make_error_code(boost::system::errc::message_size));
                return false;
            }

            if (h_v5_auth_) return h_v5_auth_(reason, std::move(props));
        } break;
        default:
            BOOST_ASSERT(false);
            return false;
        }
        return true;
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
        std::vector<v5::property_variant> props = {}
    ) {

        auto impl =
            [&](auto&& msg, auto const& serialize) {
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
                    serialize(msg);
                }
                do_sync_write(msg);
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
            [&](auto&& msg, auto const& serialize) {
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
                    serialize(msg);
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
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(topic_name, qos);
        send_subscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(as::buffer(topic_name.data(), topic_name.size()), qos);
        send_subscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
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
        std::vector<std::uint8_t>& params,
        packet_id_t packet_id,
        std::uint8_t reason,
        Args&&... args) {
        params.push_back(reason);
        send_suback(params, packet_id, std::forward<Args>(args)...);
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
        std::vector<as::const_buffer>&& params,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {
        params.emplace_back(std::move(topic_name));
        send_unsubscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void send_unsubscribe(
        std::vector<as::const_buffer>&& params,
        packet_id_t packet_id,
        mqtt::string_view topic_name,
        Args&&... args) {
        params.emplace_back(as::buffer(topic_name.data(), topic_name.size()));
        send_unsubscribe(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_unsubscribe(
        std::vector<as::const_buffer>&& params,
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
        std::vector<std::uint8_t>&& params,
        packet_id_t packet_id,
        std::uint8_t reason,
        Args&&... args) {
        params.push_back(reason);
        send_suback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void send_unsuback(
        std::vector<std::uint8_t>&& params,
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

    // Blocking write
    template <typename MessageVariant>
    void do_sync_write(MessageVariant&& mv) {
        boost::system::error_code ec;
        if (!connected_) return;
        if (h_pre_send_) h_pre_send_();
        write(*socket_, const_buffer_sequence<PacketIdBytes>(std::forward<MessageVariant>(mv)), ec);
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
            [&] (auto&& msg) {
                auto self = this->shared_from_this();
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
        async_handler_t func
    ) {

        auto msg = basic_pubrel_message<PacketIdBytes>(packet_id);

        auto impl =
            [&](auto&& msg, auto const& serialize) {
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
                                    msg
                                );
                            }
                        );
                    }
                }

                if (serialize) {
                    serialize(msg);
                }
                do_async_write(std::move(msg), std::move(func));
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
            [&] (auto&& msg) {
                auto self = this->shared_from_this();
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
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        std::uint8_t qos,
        Args&&... args) {
        params.emplace_back(std::move(topic_name), qos);
        async_send_subscribe(std::move(params), std::move(life_keepers), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_send_subscribe(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
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
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
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
        std::vector<std::tuple<as::const_buffer, std::uint8_t>>&& params,
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
        std::vector<std::uint8_t>&& params,
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        params.push_back(qos);
        async_send_suback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void async_send_suback(
        std::vector<std::uint8_t>&& params,
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
        std::vector<as::const_buffer>&& params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        as::const_buffer topic_name,
        Args&&... args) {
        params.emplace_back(std::move(topic_name));
        async_send_unsubscribe(std::move(params), std::move(life_keepers), packet_id, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void async_send_unsubscribe(
        std::vector<as::const_buffer>&& params,
        mqtt::any life_keepers,
        packet_id_t packet_id,
        std::string topic_name,
        Args&&... args) {

        auto sp_topic = std::make_shared<std::string>(std::move(topic_name));
        params.emplace_back(as::buffer(*sp_topic));
        async_send_unsubscribe(std::move(params), std::make_pair(std::move(life_keepers), std::move(sp_topic)), packet_id, std::forward<Args>(args)...);
    }

    void async_send_unsubscribe(
        std::vector<as::const_buffer>&& params,
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
        std::vector<as::const_buffer>&& params,
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
        std::vector<std::uint8_t>&& params,
        packet_id_t packet_id,
        std::uint8_t qos,
        Args&&... args) {
        params.push_back(qos);
        async_send_unsuback(std::move(params), packet_id, std::forward<Args>(args)...);
    }

    void async_send_unsuback(
        std::vector<std::uint8_t>&& params,
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
             expected_(expected)
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
            if (expected_ != bytes_transferred) {
                self_->connected_ = false;
                while (!self_->queue_.empty()) {
                    self_->queue_.front().handler()(ec);
                    self_->queue_.pop_front();
                }
                throw write_bytes_transferred_error(expected_, bytes_transferred);
            }
            if (!self_->queue_.empty()) {
                self_->do_async_write();
            }
        }
        std::shared_ptr<this_type> self_;
        async_handler_t func_;
        std::size_t num_of_messages_;
        std::size_t expected_;
    };

    void do_async_write() {
        std::vector<as::const_buffer> buf;
        std::vector<async_handler_t> handlers;

        std::size_t total_const_buffer_sequence = 0;
        auto start = queue_.cbegin();
        auto end =
            [&] {
                if (max_queue_send_count_ == 0) return queue_.cend();
                if (max_queue_send_count_ >= queue_.size()) return queue_.cend();
                return
                    start +
                    static_cast<typename std::deque<async_packet>::const_iterator::difference_type>(max_queue_send_count_);
            } ();

        std::size_t total = 0;
        for (auto it = start; it != end; ++it) {
            auto const& elem = *it;
            auto const& mv = elem.message();
            auto size = mqtt::size<PacketIdBytes>(mv);

            if (max_queue_send_size_ != 0 && max_queue_send_size_ < total + size) {
                end = it;
                break;
            }
            total += size;
            total_const_buffer_sequence += num_of_const_buffer_sequence(mv);
        }

        buf.reserve(total_const_buffer_sequence);
        handlers.reserve(queue_.size());

        for (auto it = start; it != end; ++it) {
            auto const& elem = *it;
            auto const& mv = elem.message();
            auto const& cbs = const_buffer_sequence(mv);
            std::copy(cbs.begin(), cbs.end(), std::back_inserter(buf));
            handlers.emplace_back(elem.handler());
        }

        if (h_pre_send_) h_pre_send_();

        async_write(
            *socket_,
            buf,
            write_completion_handler(
                this->shared_from_this(),
                [handlers = std::move(handlers)]
                (boost::system::error_code const& ec) {
                    for (auto const& h : handlers) {
                        if (h) h(ec);
                    }
                },
                static_cast<std::size_t>(std::distance(start, end)),
                total
            )
        );
    }

    void do_async_write(basic_message_variant<PacketIdBytes> mv, async_handler_t func) {
        // Move this job to the socket's strand so that it can be queued without mutexes.
        socket_->post(
            [self = this->shared_from_this(), mv = std::move(mv), func = std::move(func)]
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
    std::unique_ptr<Socket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> mqtt_connected_{false};

    char buf_;
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
    bool disconnect_requested_{false};
    bool connect_requested_{false};
    std::size_t max_queue_send_count_{1};
    std::size_t max_queue_send_size_{0};
    mqtt_message_processed_handler h_mqtt_message_processed_;
    protocol_version version_{protocol_version::undetermined};
};

} // namespace mqtt

#endif // MQTT_ENDPOINT_HPP

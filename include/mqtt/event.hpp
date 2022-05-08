// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_EVENT_HPP)
#define MQTT_EVENT_HPP

#include <mqtt/config.hpp> // should be top to configure variant limit

#include <mqtt/any.hpp>
#include <mqtt/buffer.hpp>
#include <mqtt/control_packet_type.hpp>
#include <mqtt/error_code.hpp>
#include <mqtt/message.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/subscribe_entry.hpp>
#include <mqtt/v5_message.hpp>
#include <mqtt/will.hpp>

namespace MQTT_NS {

template <typename PacketId>
class event {

public:

    // MQTT common handlers

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
    virtual bool on_connect(buffer client_id,
                            optional<buffer> user_name,
                            optional<buffer> password,
                            optional<will> will,
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
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718039<BR>
     *        3.3.2  Variable header
     * @param pubopts
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718038<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using publish functions.
     * @param topic_name
     *        Topic name
     * @param contents
     *        Published contents
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_publish(optional<PacketId> packet_id,
                            publish_options pubopts,
                            buffer topic_name,
                            buffer contents) noexcept = 0;

    /**
     * @brief Puback handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.4.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_puback(PacketId packet_id) noexcept = 0;

    /**
     * @brief Pubrec handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718050<BR>
     *        3.5.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubrec(PacketId packet_id) noexcept = 0;

    /**
     * @brief Pubrel handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349791<BR>
     *        3.6.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubrel(PacketId packet_id) noexcept = 0;

    /**
     * @brief Pubcomp handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718060<BR>
     *        3.7.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_pubcomp(PacketId packet_id) noexcept = 0;

    /**
     * @brief Subscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349801<BR>
     *        3.8.2 Variable header
     * @param entries
     *        Collection of Share Name, Topic Filter, and QoS.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349802<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_subscribe(PacketId packet_id,
                              std::vector<subscribe_entry> entries) noexcept = 0;

    /**
     * @brief Suback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718070<BR>
     *        3.9.2 Variable header
     * @param qoss
     *        Collection of QoS that is corresponding to subscribed topic order.<BR>
     *        If subscription is failure, the value is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718071<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_suback(PacketId packet_id, std::vector<suback_return_code> returns) noexcept = 0;

    /**
     * @brief Unsubscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349810<BR>
     *        3.10.2 Variable header
     * @param entries
     *        Collection of Share Name and Topic Filter<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800448<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_unsubscribe(PacketId packet_id, std::vector<unsubscribe_entry> entries) noexcept = 0;

    /**
     * @brief Unsuback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.11.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_unsuback(PacketId) noexcept = 0;

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
    virtual bool on_v5_connect(buffer client_id,
                               optional<buffer> user_name,
                               optional<buffer> password,
                               optional<will> will,
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
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901108<BR>
     *        3.3.2.2 Packet Identifier
     * @param pubopts
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901101<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using publish functions.
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
    virtual bool on_v5_publish(optional<PacketId> packet_id,
                               publish_options pubopts,
                               buffer topic_name,
                               buffer contents,
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
    virtual bool on_v5_puback(PacketId packet_id,
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
    virtual bool on_v5_pubrec(PacketId packet_id,
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
    virtual bool on_v5_pubrel(PacketId packet_id,
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
    virtual bool on_v5_pubcomp(PacketId packet_id,
                               v5::pubcomp_reason_code reason_code,
                               v5::properties props) noexcept = 0;

    /**
     * @brief Subscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901163<BR>
     *        3.8.2 Variable header
     * @param entries
     *        Collection of Share Name, Topic Filter, and Subscribe Options.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901168<BR>
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_subscribe(PacketId packet_id,
                                 std::vector<subscribe_entry> entries,
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
    virtual bool on_v5_suback(PacketId packet_id,
                              std::vector<v5::suback_reason_code> reasons,
                              v5::properties props) noexcept = 0;

    /**
     * @brief Unsubscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901181<BR>
     *        3.10.2 Variable header
     * @param entries
     *        Collection of Share Name and Topic Filter<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901185<BR>
     *        3.10.3 UNSUBSCRIBE Payload
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901182<BR>
     *        3.10.2.1 UNSUBSCRIBE Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    virtual bool on_v5_unsubscribe(PacketId packet_id,
                                   std::vector<unsubscribe_entry> entries,
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
    virtual bool on_v5_unsuback(PacketId,
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

    /**
     * @brief Publish response sent handler
     *        This function is called just after puback sent on QoS1, or pubcomp sent on QoS2.
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901026<BR>
     *        2.2.1 Packet Identifier
     */
    virtual void on_pub_res_sent(PacketId packet_id) noexcept = 0;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg publish message
     */
    virtual void on_serialize_publish_message(basic_publish_message<sizeof(PacketId)> msg) noexcept = 0;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg v5::publish message
     */
    virtual void on_serialize_v5_publish_message(v5::basic_publish_message<sizeof(PacketId)> msg) noexcept = 0;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    virtual void on_serialize_pubrel_message(basic_pubrel_message<sizeof(PacketId)> msg) noexcept = 0;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    virtual void on_serialize_v5_pubrel_message(v5::basic_pubrel_message<sizeof(PacketId)> msg) noexcept = 0;

    /**
     * @brief Remove serialized message
     * @param packet_id packet identifier of the removing message
     */
    virtual void on_serialize_remove(PacketId packet_id) noexcept = 0;

    /**
     * @brief Pre-send handler
     *        This handler is called when any mqtt control packet is decided to send.
     */
    virtual void on_pre_send() noexcept = 0;

    /**
     * @brief is valid length handler
     *        This handler is called when remaining length is received.
     * @param control_packet_type control_packet_type that has variable length
     * @param remaining length
     * @return true if check is success, otherwise false
     */
    virtual bool check_is_valid_length(control_packet_type packet_type, std::size_t remaining_length) noexcept = 0;

    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     * @param func A callback function that is called when async operation will finish.
     */
    virtual void on_mqtt_message_processed(any session_life_keeper) = 0;

    virtual void disconnect_on_mqtt_error(v5::disconnect_reason_code) = 0;
    virtual void connack_on_mqtt_error(v5::connect_reason_code) = 0;

    virtual void puback_on_recv_publish(PacketId packet_id, v5::puback_reason_code) = 0;
    virtual void pubrec_on_recv_publish(PacketId packet_id, v5::pubrec_reason_code) = 0;
    virtual void pubrel_on_recv_pubrec(PacketId packet_id, v5::pubrel_reason_code) = 0;
    virtual void pubcomp_on_recv_pubrel(PacketId packet_id, v5::pubcomp_reason_code) = 0;

    virtual void send_publish_queue_one() = 0;

    virtual ~event() = default;
};

} // namespace MQTT_NS

#endif // MQTT_EVENT_HPP

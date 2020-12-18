// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CALLABLE_OVERLAY_HPP)
#define MQTT_CALLABLE_OVERLAY_HPP

#include <mqtt/variant.hpp> // should be top to configure variant limit

#include <mqtt/namespace.hpp>
#include <mqtt/attributes.hpp>
#include <mqtt/endpoint.hpp>
#include <mqtt/move.hpp>

namespace MQTT_NS {
template<typename Impl>
struct callable_overlay final : public Impl
{
    using base = Impl;
    using packet_id_t = typename base::packet_id_t;

    template<typename ... Args>
    callable_overlay(Args && ... args)
     : base(std::forward<Args>(args)...)
    { }
    ~callable_overlay() = default;
    callable_overlay(callable_overlay&&) = default;
    callable_overlay(callable_overlay const&) = default;
    callable_overlay& operator=(callable_overlay&&) = default;
    callable_overlay& operator=(callable_overlay const&) = default;

    // MQTT Common handlers

    /**
     * @brief Pingreq handler
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718086<BR>
     *        3.13 PINGREQ – PING request
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_pingreq() noexcept override final {
        return ! h_pingreq_ || h_pingreq_();
    }

    /**
     * @brief Pingresp handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901200<BR>
     *        3.13 PINGRESP – PING response
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_pingresp() noexcept override final {
        return ! h_pingresp_ || h_pingresp_();
    }

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
    MQTT_ALWAYS_INLINE bool on_connect(buffer client_id,
                                       optional<buffer> user_name,
                                       optional<buffer> password,
                                       optional<will> will,
                                       bool clean_session,
                                       std::uint16_t keep_alive) noexcept override final {
        return    ! h_connect_
               || h_connect_(force_move(client_id),
                             force_move(user_name),
                             force_move(password),
                             force_move(will),
                             clean_session,
                             keep_alive);
    }

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
    MQTT_ALWAYS_INLINE bool on_connack(bool session_present, connect_return_code return_code) noexcept override final {
        return    ! h_connack_
               || h_connack_(session_present, return_code);
    }

    /**
     * @brief Publish handler
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718039<BR>
     *        3.3.2  Variable header
     * @param fixed_header
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718038<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using publish functions.
     * @param topic_name
     *        Topic name
     * @param contents
     *        Published contents
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_publish(optional<packet_id_t> packet_id,
                                       publish_options pubopts,
                                       buffer topic_name,
                                       buffer contents) noexcept override final {
        return    ! h_publish_
               || h_publish_(packet_id,
                             pubopts,
                             force_move(topic_name),
                             force_move(contents));
    }

    /**
     * @brief Puback handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.4.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_puback(packet_id_t packet_id) noexcept override final {
        return    ! h_puback_
               || h_puback_(packet_id);
    }

    /**
     * @brief Pubrec handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718050<BR>
     *        3.5.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_pubrec(packet_id_t packet_id) noexcept override final {
        return    ! h_pubrec_
               || h_pubrec_(packet_id);
    }

    /**
     * @brief Pubrel handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349791<BR>
     *        3.6.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_pubrel(packet_id_t packet_id) noexcept override final {
        return    ! h_pubrel_
               || h_pubrel_(packet_id);
    }

    /**
     * @brief Pubcomp handler
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718060<BR>
     *        3.7.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_pubcomp(packet_id_t packet_id) noexcept override final {
        return    ! h_pubcomp_
               || h_pubcomp_(packet_id);
    }

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
    MQTT_ALWAYS_INLINE bool on_subscribe(packet_id_t packet_id,
                                         std::vector<subscribe_entry> entries) noexcept override final {
        return    ! h_subscribe_
               || h_subscribe_(packet_id, force_move(entries));
    }

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
    MQTT_ALWAYS_INLINE bool on_suback(packet_id_t packet_id,
                                      std::vector<suback_return_code> reasons) noexcept override final {
        return    ! h_suback_
               || h_suback_(packet_id, force_move(reasons));
    }

    /**
     * @brief Unsubscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349810<BR>
     *        3.10.2 Variable header
     * @param topics
     *        Collection of Share Name and Topic Filter<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800448<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_unsubscribe(packet_id_t packet_id,
                                           std::vector<unsubscribe_entry> entries) noexcept override final {
        return    ! h_unsubscribe_
               || h_unsubscribe_(packet_id, force_move(entries));
    }

    /**
     * @brief Unsuback handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718045<BR>
     *        3.11.2 Variable header
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_unsuback(packet_id_t packet_id) noexcept override final {
        return    ! h_unsuback_
               || h_unsuback_(packet_id);
    }

    /**
     * @brief Disconnect handler
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc384800463<BR>
     *        3.14 DISCONNECT – Disconnect notification
     */
    MQTT_ALWAYS_INLINE void on_disconnect() noexcept override final {
        if(h_disconnect_) h_disconnect_();
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_connect(buffer client_id,
                                          optional<buffer> user_name,
                                          optional<buffer> password,
                                          optional<will> will,
                                          bool clean_start,
                                          std::uint16_t keep_alive,
                                          v5::properties props) noexcept override final {
        return    ! h_v5_connect_
               || h_v5_connect_(force_move(client_id),
                                force_move(user_name),
                                force_move(password),
                                force_move(will),
                                clean_start,
                                keep_alive,
                                force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_connack(bool session_present,
                                          v5::connect_reason_code reason_code,
                                          v5::properties props) noexcept override final {
        return    ! h_v5_connack_
               || h_v5_connack_(session_present, reason_code, force_move(props));
    }

    /**
     * @brief Publish handler
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901108<BR>
     *        3.3.2.2 Packet Identifier
     * @param fixed_header
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
    MQTT_ALWAYS_INLINE bool on_v5_publish(optional<packet_id_t> packet_id,
                                          publish_options pubopts,
                                          buffer topic_name,
                                          buffer contents,
                                          v5::properties props) noexcept override final {
        return    ! h_v5_publish_
               || h_v5_publish_(packet_id,
                                pubopts,
                                force_move(topic_name),
                                force_move(contents),
                                force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_puback(packet_id_t packet_id,
                                         v5::puback_reason_code reason_code,
                                         v5::properties props) noexcept override final {
        return    ! h_v5_puback_
               || h_v5_puback_(packet_id, reason_code, force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_pubrec(packet_id_t packet_id,
                                         v5::pubrec_reason_code reason_code,
                                         v5::properties props) noexcept override final {
        return    ! h_v5_pubrec_
               || h_v5_pubrec_(packet_id, reason_code, force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_pubrel(packet_id_t packet_id,
                                         v5::pubrel_reason_code reason_code,
                                         v5::properties props) noexcept override final {
        return    ! h_v5_pubrel_
               || h_v5_pubrel_(packet_id, reason_code, force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_pubcomp(packet_id_t packet_id,
                                          v5::pubcomp_reason_code reason_code,
                                          v5::properties props) noexcept override final {
        return    ! h_v5_pubcomp_
               || h_v5_pubcomp_(packet_id, reason_code, force_move(props));
    }

    /**
     * @brief Subscribe handler
     * @param packet_id packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901163<BR>
     *        3.8.2 Variable header
     * @param entries
     *        Collection of Share Name, Topic Filter, and QoS.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901168<BR>
     * @param props
     *        Properties<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901164<BR>
     *        3.8.2.1 SUBSCRIBE Properties
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    MQTT_ALWAYS_INLINE bool on_v5_subscribe(packet_id_t packet_id,
                                            std::vector<subscribe_entry> entries,
                                            v5::properties props) noexcept override final {
        return    ! h_v5_subscribe_
               || h_v5_subscribe_(packet_id, force_move(entries), force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_suback(packet_id_t packet_id,
                                         std::vector<v5::suback_reason_code> reasons,
                                         v5::properties props) noexcept override final {
        return    ! h_v5_suback_
               || h_v5_suback_(packet_id, force_move(reasons), force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_unsubscribe(packet_id_t packet_id,
                                              std::vector<unsubscribe_entry> entries,
                                              v5::properties props) noexcept override final {
        return    ! h_v5_unsubscribe_
               || h_v5_unsubscribe_(packet_id, force_move(entries), force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_unsuback(packet_id_t packet_id,
                                           std::vector<v5::unsuback_reason_code> reasons,
                                           v5::properties props) noexcept override final {
        return    ! h_v5_unsuback_
               || h_v5_unsuback_(packet_id, force_move(reasons), force_move(props));
    }

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
    MQTT_ALWAYS_INLINE void on_v5_disconnect(v5::disconnect_reason_code reason_code,
                                             v5::properties props) noexcept override final {
        if (h_v5_disconnect_) h_v5_disconnect_(reason_code, force_move(props));
    }

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
    MQTT_ALWAYS_INLINE bool on_v5_auth(v5::auth_reason_code reason_code,
                                       v5::properties props) noexcept override final {
        return    ! h_v5_auth_
               || h_v5_auth_(reason_code, force_move(props));

    }

    // Original handlers

    /**
     * @brief Close handler
     *
     * This handler is called if the client called `disconnect()` and the server closed the socket cleanly.
     * If the socket is closed by other reasons, error_handler is called.
     * This calls base::on_close() prior to calling the provided callback, if any.
     */
    MQTT_ALWAYS_INLINE void on_close() noexcept override final {
        base::on_close();
        if (h_close_) h_close_();
    }

    /**
     * @brief Error handler
     *
     * This handler is called if the socket is closed without client's `disconnect()` call.
     * This calls base::on_error() prior to calling the provided callback, if any.
     *
     * @param ec error code
     */
    MQTT_ALWAYS_INLINE void on_error(error_code ec) noexcept override final {
        base::on_error(ec);
        if (h_error_) h_error_(ec);
    }

    /**
     * @brief Publish response sent handler
     *        This function is called just after puback sent on QoS1, or pubcomp sent on QoS2.
     * @param packet_id
     *        packet identifier<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901026<BR>
     *        2.2.1 Packet Identifier
     */
    MQTT_ALWAYS_INLINE void on_pub_res_sent(packet_id_t packet_id) noexcept override final {
        if (h_pub_res_sent_) h_pub_res_sent_(packet_id);
    }

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg publish message
     */
    MQTT_ALWAYS_INLINE void on_serialize_publish_message(basic_publish_message<sizeof(packet_id_t)> msg) noexcept override final {
        if (h_serialize_publish_) h_serialize_publish_(msg);
    }

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg v5::publish message
     */
    MQTT_ALWAYS_INLINE void on_serialize_v5_publish_message(v5::basic_publish_message<sizeof(packet_id_t)> msg) noexcept override final {
        if (h_serialize_v5_publish_) h_serialize_v5_publish_(msg);
    }

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    MQTT_ALWAYS_INLINE void on_serialize_pubrel_message(basic_pubrel_message<sizeof(packet_id_t)> msg) noexcept override final {
        if (h_serialize_pubrel_) h_serialize_pubrel_(msg);
    }

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    MQTT_ALWAYS_INLINE void on_serialize_v5_pubrel_message(v5::basic_pubrel_message<sizeof(packet_id_t)> msg) noexcept override final {
        if (h_serialize_v5_pubrel_) h_serialize_v5_pubrel_(msg);
    }

    /**
     * @brief Remove serialized message
     * @param packet_id packet identifier of the removing message
     */
    MQTT_ALWAYS_INLINE void on_serialize_remove(packet_id_t packet_id) noexcept override final {
        if (h_serialize_remove_) h_serialize_remove_(packet_id);
    }

    /**
     * @brief Pre-send handler
     *        This handler is called when any mqtt control packet is decided to send.
     */
    MQTT_ALWAYS_INLINE void on_pre_send() noexcept override final {
        base::on_pre_send();
        if (h_pre_send_) h_pre_send_();
    }

    /**
     * @brief is valid length handler
     *        This handler is called when remaining length is received.
     * @param control_packet_type control_packet_type that has variable length
     * @param remaining length
     * @return true if check is success, otherwise false
     */
    MQTT_ALWAYS_INLINE bool check_is_valid_length(control_packet_type packet_type, std::size_t remaining_length) noexcept override final {
        return    ! h_is_valid_length_
               || h_is_valid_length_(packet_type, remaining_length);
    }

    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     *        This calls base::on_mqtt_message_processed() only if mqtt_message_processed handler is not set.
     *        * `client` inherits `endpoint` and override `on_pre_send()`, `on_close()`, and `on_error()`.
     *           And implement important process *1 in the override definition (timer reset, timer cancel).
     *        * `client` has `set_pre_send_handler()`, `set_on_close()`, and `set_on_error()`.
     *          If a user of `client` call them, and the event happens, set handler should be called.
     *          Either the user set handlers or not, *1 should be executed automatically.
     *        * `endpoint` has the default definition of `on_mqtt_message_processed()`.
     *          `client` doesn't override `on_mqtt_message_processed()`.
     *          If a user override this, then the default definition shouldn't be called. It makes conflict behavior.
     *        * Other `on_*()` doesn't have the default definition. `callable_overlay` calls the corresponding
     *          handler only if it is set.
     * @param func A callback function that is called when async operation will finish.
     */
    MQTT_ALWAYS_INLINE void on_mqtt_message_processed(any session_life_keeper) noexcept override final {
        if(h_mqtt_message_processed_) {
            h_mqtt_message_processed_(force_move(session_life_keeper));
        }
        else {
            base::on_mqtt_message_processed(force_move(session_life_keeper));
        }
    }

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
        bool(buffer client_id,
             optional<buffer> user_name,
             optional<buffer> password,
             optional<will> will,
             bool clean_session,
             std::uint16_t keep_alive)>;

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
    using connack_handler = std::function<bool(bool session_present, connect_return_code return_code)>;

    /**
     * @brief Publish handler
     * @param fixed_header
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718038<BR>
     *        3.3.1 Fixed header<BR>
     *        You can check the fixed header using publish functions.
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc398718039<BR>
     *        3.3.2  Variable header
     * @param topic_name
     *        Topic name
     * @param contents
     *        Published contents
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using publish_handler = std::function<bool(optional<packet_id_t> packet_id,
                                               publish_options pubopts,
                                               buffer topic_name,
                                               buffer contents)>;

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
     *        Collection of Share Name, Topic Filter, and QoS.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc385349802<BR>
     * @return if the handler returns true, then continue receiving, otherwise quit.
     */
    using subscribe_handler = std::function<bool(packet_id_t packet_id,
                                                 std::vector<subscribe_entry> entries)>;

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
    using suback_handler = std::function<bool(packet_id_t packet_id,
                                              std::vector<suback_return_code> qoss)>;

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
    using unsubscribe_handler = std::function<bool(packet_id_t packet_id,
                                                   std::vector<unsubscribe_entry> entries)>;

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
        bool(buffer client_id,
             optional<buffer> user_name,
             optional<buffer> password,
             optional<will> will,
             bool clean_start,
             std::uint16_t keep_alive,
             v5::properties props)
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
             v5::connect_reason_code reason_code,
             v5::properties props)
    >;

    /**
     * @brief Publish handler
     * @param packet_id
     *        packet identifier<BR>
     *        If received publish's QoS is 0, packet_id is nullopt.<BR>
     *        See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901108<BR>
     *        3.3.2.2 Packet Identifier
     * @param fixed_header
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
    using v5_publish_handler = std::function<
        bool(optional<packet_id_t> packet_id,
             publish_options pubopts,
             buffer topic_name,
             buffer contents,
             v5::properties props)
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
             v5::puback_reason_code reason_code,
             v5::properties props)
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
             v5::pubrec_reason_code reason_code,
             v5::properties props)
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
             v5::pubrel_reason_code reason_code,
             v5::properties props)
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
             v5::pubcomp_reason_code reason_code,
             v5::properties props)
    >;

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
    using v5_subscribe_handler = std::function<
        bool(packet_id_t packet_id,
             std::vector<subscribe_entry> entries,
             v5::properties props)
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
             std::vector<v5::suback_reason_code> reasons,
             v5::properties props)
    >;

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
    using v5_unsubscribe_handler = std::function<
        bool(packet_id_t packet_id,
             std::vector<unsubscribe_entry> entries,
             v5::properties props)
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
             std::vector<v5::unsuback_reason_code> reasons,
             v5::properties props)
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
        void(v5::disconnect_reason_code reason_code,
             v5::properties props)
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
        bool(v5::auth_reason_code reason_code,
             v5::properties props)
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
    using error_handler = std::function<void(error_code ec)>;

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
    using serialize_publish_message_handler = std::function<void(basic_publish_message<sizeof(packet_id_t)> msg)>;

    /**
     * @brief Serialize publish handler
     *        You can serialize the publish message.
     *        To restore the message, use restore_serialized_message().
     * @param msg v5::publish message
     */
    using serialize_v5_publish_message_handler = std::function<void(v5::basic_publish_message<sizeof(packet_id_t)> msg)>;

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
    using serialize_pubrel_message_handler = std::function<void(basic_pubrel_message<sizeof(packet_id_t)> msg)>;

    /**
     * @brief Serialize pubrel handler
     *        You can serialize the pubrel message.
     *        If your storage has already had the publish message that has the same packet_id,
     *        then you need to replace the publish message to pubrel message.
     *        To restore the message, use restore_serialized_message().
     * @param msg pubrel message
     */
    using serialize_v5_pubrel_message_handler = std::function<void(v5::basic_pubrel_message<sizeof(packet_id_t)> msg)>;

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
        std::function<bool(control_packet_type packet_type, std::size_t remaining_length)>;

    /**
     * @brief next read handler
     *        This handler is called when the current mqtt message has been processed.
     * @param func A callback function that is called when async operation will finish.
     */
    using mqtt_message_processed_handler =
        std::function<void(any session_life_keeper)>;



    // MQTT Common handlers

    /**
     * @brief Set pingreq handler
     * @param h handler
     */
    void set_pingreq_handler(pingreq_handler h = pingreq_handler()) {
        h_pingreq_ = force_move(h);
    }

    /**
     * @brief Set pingresp handler
     * @param h handler
     */
    void set_pingresp_handler(pingresp_handler h = pingresp_handler()) {
        h_pingresp_ = force_move(h);
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
        h_connect_ = force_move(h);
    }

    /**
     * @brief Set connack handler
     * @param h handler
     */
    void set_connack_handler(connack_handler h = connack_handler()) {
        h_connack_ = force_move(h);
    }

    /**
     * @brief Set publish handler
     * @param h handler
     */
    void set_publish_handler(publish_handler h = publish_handler()) {
        h_publish_ = force_move(h);
    }

    /**
     * @brief Set puback handler
     * @param h handler
     */
    void set_puback_handler(puback_handler h = puback_handler()) {
        h_puback_ = force_move(h);
    }

    /**
     * @brief Set pubrec handler
     * @param h handler
     */
    void set_pubrec_handler(pubrec_handler h = pubrec_handler()) {
        h_pubrec_ = force_move(h);
    }

    /**
     * @brief Set pubrel handler
     * @param h handler
     */
    void set_pubrel_handler(pubrel_handler h = pubrel_handler()) {
        h_pubrel_ = force_move(h);
    }

    /**
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_pubcomp_handler(pubcomp_handler h = pubcomp_handler()) {
        h_pubcomp_ = force_move(h);
    }

    /**
     * @brief Set subscribe handler
     * @param h handler
     */
    void set_subscribe_handler(subscribe_handler h = subscribe_handler()) {
        h_subscribe_ = force_move(h);
    }

    /**
     * @brief Set suback handler
     * @param h handler
     */
    void set_suback_handler(suback_handler h = suback_handler()) {
        h_suback_ = force_move(h);
    }

    /**
     * @brief Set unsubscribe handler
     * @param h handler
     */
    void set_unsubscribe_handler(unsubscribe_handler h = unsubscribe_handler()) {
        h_unsubscribe_ = force_move(h);
    }

    /**
     * @brief Set unsuback handler
     * @param h handler
     */
    void set_unsuback_handler(unsuback_handler h = unsuback_handler()) {
        h_unsuback_ = force_move(h);
    }

    /**
     * @brief Set disconnect handler
     * @param h handler
     */
    void set_disconnect_handler(disconnect_handler h = disconnect_handler()) {
        h_disconnect_ = force_move(h);
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
        h_v5_connect_ = force_move(h);
    }

    /**
     * @brief Set connack handler
     * @param h handler
     */
    void set_v5_connack_handler(v5_connack_handler h = v5_connack_handler()) {
        h_v5_connack_ = force_move(h);
    }

    /**
     * @brief Set publish handler
     * @param h handler
     */
    void set_v5_publish_handler(v5_publish_handler h = v5_publish_handler()) {
        h_v5_publish_ = force_move(h);
    }

    /**
     * @brief Set puback handler
     * @param h handler
     */
    void set_v5_puback_handler(v5_puback_handler h = v5_puback_handler()) {
        h_v5_puback_ = force_move(h);
    }

    /**
     * @brief Set pubrec handler
     * @param h handler
     */
    void set_v5_pubrec_handler(v5_pubrec_handler h = v5_pubrec_handler()) {
        h_v5_pubrec_ = force_move(h);
    }

    /**
     * @brief Set pubrel handler
     * @param h handler
     */
    void set_v5_pubrel_handler(v5_pubrel_handler h = v5_pubrel_handler()) {
        h_v5_pubrel_ = force_move(h);
    }

    /**
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_v5_pubcomp_handler(v5_pubcomp_handler h = v5_pubcomp_handler()) {
        h_v5_pubcomp_ = force_move(h);
    }

    /**
     * @brief Set subscribe handler
     * @param h handler
     */
    void set_v5_subscribe_handler(v5_subscribe_handler h = v5_subscribe_handler()) {
        h_v5_subscribe_ = force_move(h);
    }

    /**
     * @brief Set suback handler
     * @param h handler
     */
    void set_v5_suback_handler(v5_suback_handler h = v5_suback_handler()) {
        h_v5_suback_ = force_move(h);
    }

    /**
     * @brief Set unsubscribe handler
     * @param h handler
     */
    void set_v5_unsubscribe_handler(v5_unsubscribe_handler h = v5_unsubscribe_handler()) {
        h_v5_unsubscribe_ = force_move(h);
    }

    /**
     * @brief Set unsuback handler
     * @param h handler
     */
    void set_v5_unsuback_handler(v5_unsuback_handler h = v5_unsuback_handler()) {
        h_v5_unsuback_ = force_move(h);
    }

    /**
     * @brief Set disconnect handler
     * @param h handler
     */
    void set_v5_disconnect_handler(v5_disconnect_handler h = v5_disconnect_handler()) {
        h_v5_disconnect_ = force_move(h);
    }

    /**
     * @brief Set auth handler
     * @param h handler
     */
    void set_v5_auth_handler(v5_auth_handler h = v5_auth_handler()) {
        h_v5_auth_ = force_move(h);
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
    v5_pubrec_handler const& get_v5__handler() const {
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
     * @brief Set pubcomp handler
     * @param h handler
     */
    void set_pub_res_sent_handler(pub_res_sent_handler h = pub_res_sent_handler()) {
        h_pub_res_sent_ = force_move(h);
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
        h_serialize_publish_ = force_move(h_publish);
        h_serialize_pubrel_ = force_move(h_pubrel);
        h_serialize_remove_ = force_move(h_remove);
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
        h_serialize_v5_publish_ = force_move(h_publish);
        h_serialize_v5_pubrel_ = force_move(h_pubrel);
        h_serialize_remove_ = force_move(h_remove);
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
            [h_publish = force_move(h_publish)]
            (basic_publish_message<sizeof(packet_id_t)> msg) {
                if (h_publish) {
                    auto buf = msg.continuous_buffer();
                    h_publish(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_pubrel_ =
            [h_pubrel = force_move(h_pubrel)]
            (basic_pubrel_message<sizeof(packet_id_t)> msg) {
                if (h_pubrel) {
                    auto buf = msg.continuous_buffer();
                    h_pubrel(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_remove_ = force_move(h_remove);
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
            [h_publish = force_move(h_publish)]
            (v5::basic_publish_message<sizeof(packet_id_t)> msg) {
                if (h_publish) {
                    auto buf = msg.continuous_buffer();
                    h_publish(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_v5_pubrel_ =
            [h_pubrel = force_move(h_pubrel)]
            (v5::basic_pubrel_message<sizeof(packet_id_t)> msg) {
                if (h_pubrel) {
                    auto buf = msg.continuous_buffer();
                    h_pubrel(msg.packet_id(), buf.data(), buf.size());
                }
            };
        h_serialize_remove_ = force_move(h_remove);
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
        h_pre_send_ = force_move(h);
    }

    /**
     * @brief Set check length handler
     * @param h handler
     */
    void set_is_valid_length_handler(is_valid_length_handler h = is_valid_length_handler()) {
        h_is_valid_length_ = force_move(h);
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
            h_mqtt_message_processed_ = force_move(h);
        }
        else {
            h_mqtt_message_processed_ = {};
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
     * @brief Set close handler
     * @param h handler
     */
    void set_close_handler(close_handler h = close_handler()) {
        h_close_ = force_move(h);
    }

    /**
     * @brief Set error handler
     * @param h handler
     */
    void set_error_handler(error_handler h = error_handler()) {
        h_error_ = force_move(h);
    }

    /**
     * @brief Get close handler
     * @return handler
     */
    close_handler get_close_handler() const {
        return h_close_;
    }

    /**
     * @brief Get error handler
     * @return handler
     */
    error_handler get_error_handler() const {
        return h_error_;
    }

private:
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
    mqtt_message_processed_handler h_mqtt_message_processed_;
}; // callable_overlay

} // namespace MQTT_NS

#endif // MQTT_CALLABLE_OVERLAY_HPP

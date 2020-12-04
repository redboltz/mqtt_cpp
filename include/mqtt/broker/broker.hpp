// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_HPP)
#define MQTT_BROKER_HPP

#include <mqtt/config.hpp>

#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/property.hpp>
#include <mqtt/visitor_util.hpp>
#include <mqtt/broker/subscription_map.hpp>
#include <mqtt/broker/retained_topic_map.hpp>
#include <mqtt/server.hpp>

namespace MQTT_NS {

namespace mi = boost::multi_index;
namespace as = boost::asio;


using endpoint_t = server<>::endpoint_t;
using con_sp_t = std::shared_ptr<endpoint_t>;
using con_wp_t = std::weak_ptr<endpoint_t>;
using packet_id_t = endpoint_t::packet_id_t;

#if defined(MQTT_STD_STRING_VIEW)
#define MQTT_STRING_VIEW_CONSTEXPR constexpr
#else  // defined(MQTT_STD_STRING_VIEW)
#define MQTT_STRING_VIEW_CONSTEXPR
#endif // defined(MQTT_STD_STRING_VIEW)


// TODO: Technically this function is simply wrong, since it's treating the
// topic pattern as if it were an ASCII sequence.
// To make this function correct per the standard, it would be necessary
// to conduct the search for the wildcard characters using a proper
// UTF-8 API to avoid problems of interpreting parts of multi-byte characters
// as if they were individual ASCII characters
MQTT_STRING_VIEW_CONSTEXPR
bool validate_topic_filter(string_view topic_filter) {
    /*
     * Confirm the topic pattern is valid before registering it.
     * Use rules from http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    if (topic_filter.empty() || (topic_filter.size() > std::numeric_limits<std::uint16_t>::max())) {
        return false;
    }

    for (string_view::size_type idx = topic_filter.find_first_of(string_view("\0+#", 3));
         string_view::npos != idx;
         idx = topic_filter.find_first_of(string_view("\0+#", 3), idx+1)) {
        BOOST_ASSERT(
            ('\0' == topic_filter[idx])
            || ('+'  == topic_filter[idx])
            || ('#'  == topic_filter[idx])
        );
        if ('\0' == topic_filter[idx]) {
            // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
            return false;
        }
        else if ('+' == topic_filter[idx]) {
            /*
             * Either must be the first character,
             * or be preceeded by a topic seperator.
             */
            if ((0 != idx) && ('/' != topic_filter[idx-1])) {
                return false;
            }

            /*
             * Either must be the last character,
             * or be followed by a topic seperator.
             */
            if ((topic_filter.size()-1 != idx) && ('/' != topic_filter[idx+1])) {
                return false;
            }
        }
        // multilevel wildcard
        else if ('#' == topic_filter[idx]) {
            /*
             * Must be absolute last character.
             * Must only be one multi level wild card.
             */
            if (idx != topic_filter.size()-1) {
                return false;
            }

            /*
             * If not the first character, then the
             * immediately preceeding character must
             * be a topic level separator.
             */
            if ((0 != idx) && ('/' != topic_filter[idx-1])) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return true;
}

#if defined(MQTT_STD_STRING_VIEW)
// The following rules come from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
static_assert( ! validate_topic_filter(""), "All Topic Names and Topic Filters MUST be at least one character long");
static_assert(validate_topic_filter("/"), "A Topic Name or Topic Filter consisting only of the ‘/’ character is valid");
static_assert( ! validate_topic_filter(string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
static_assert(validate_topic_filter(" "), "Topic Names and Topic Filters can include the space character");
static_assert(validate_topic_filter("/////"), "Topic level separators can appear anywhere in a Topic Filter or Topic Name. Adjacent Topic level separators indicate a zero-length topic level");
static_assert(validate_topic_filter("#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert(validate_topic_filter("/#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert(validate_topic_filter("+/#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("+#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("++"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("f#"), "The multi-level wildcard character MUST be specified either on its own or following a topic level separator");
static_assert( ! validate_topic_filter("#/"), "In either case the multi-level wildcard character MUST be the last character specified in the Topic Filter");

static_assert(validate_topic_filter("+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/alice/sue"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("bob/alice/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/alice/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/+/sue/+"), "The single-level wildcard can be used at any level in the Topic Filter, including first and last levels");
static_assert(validate_topic_filter("+/bob/+/sue/#"), "The single-level wildcard can be used at more than one level in the Topic Filter and can be used in conjunction with the multi-level wildcard");
static_assert( ! validate_topic_filter("+a"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("a+"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("/a+"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("a+/"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
static_assert( ! validate_topic_filter("/a+/"), "Where it is used, the single-level wildcard MUST occupy an entire level of the filter.");
#endif // defined(MQTT_STD_STRING_VIEW)

MQTT_STRING_VIEW_CONSTEXPR
bool validate_topic_name(string_view topic_name) {
    /*
     * Confirm the topic name is valid
     * Use rules from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    // The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name
    // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
    return
        ! topic_name.empty()
        && (topic_name.size() <= std::numeric_limits<std::uint16_t>::max())
        && (string_view::npos == topic_name.find_first_of(string_view("\0+#", 3)));
}

#if defined(MQTT_STD_STRING_VIEW)
// The following rules come from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
static_assert( ! validate_topic_name(""), "All Topic Names and Topic Filters MUST be at least one character long");
static_assert(validate_topic_name("/"), "A Topic Name or Topic Filter consisting only of the ‘/’ character is valid");
static_assert( ! validate_topic_name(string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
static_assert(validate_topic_name(" "), "Topic Names and Topic Filters can include the space character");
static_assert(validate_topic_name("/////"), "Topic level separators can appear anywhere in a Topic Filter or Topic Name. Adjacent Topic level separators indicate a zero-length topic level");
static_assert( ! validate_topic_name("#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("+"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("/#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("+/#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("f#"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
static_assert( ! validate_topic_name("#/"), "The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name");
#endif // defined(MQTT_STD_STRING_VIEW)

MQTT_STRING_VIEW_CONSTEXPR
bool compare_topic_filter(string_view topic_filter, string_view topic_name) {
    if ( ! validate_topic_filter(topic_filter)) {
        BOOST_ASSERT(validate_topic_filter(topic_filter));
        return false;
    }

    if ( ! validate_topic_name(topic_name)) {
        BOOST_ASSERT(validate_topic_name(topic_name));
        return false;
    }

    // TODO: The Server MUST NOT match Topic Filters starting with a wildcard character (# or +) with Topic Names beginning with a $ character
    for (string_view::size_type idx = topic_filter.find_first_of("+#");
         string_view::npos != idx;
         idx = topic_filter.find_first_of("+#")) {
        BOOST_ASSERT(
            ('+' == topic_filter[idx])
            || ('#' == topic_filter[idx])
        );

        if ('+' == topic_filter[idx]) {
            // Compare everything up to the first +
            if (topic_filter.substr(0, idx) == topic_name.substr(0, idx)) {
                /*
                 * We already know thanks to the topic filter being validated
                 * that the + symbol is directly touching '/'s on both sides
                 * (if not the first or last character), so we don't need to
                 * double check that.
                 *
                 * By simply removing the prefix that we've compared and letting
                 * the loop continue, we get the proper comparison of the '/'s
                 * automatically when the loop continues.
                 */
                topic_filter.remove_prefix(idx+1);
                /*
                 * It's a bit more complicated for the incoming topic though
                 * as we need to remove everything up to the next seperator.
                 */
                topic_name.remove_prefix(topic_name.find('/', idx));
            }
            else {
                return false;
            }
        }
        // multilevel wildcard
        else {
            /*
             * Compare up to where the multilevel wild card is found
             * and then anything after that matches the wildcard.
             */
            return topic_filter.substr(0, idx) == topic_name.substr(0, idx);
        }
    }

    // No + or # found in the remaining topic filter. Just do a string compare.
    return topic_filter == topic_name;
}

#if defined(MQTT_STD_STRING_VIEW)
static_assert(compare_topic_filter("bob", "bob"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("Bob", "bob"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("bob", "boB"), "Topic Names and Topic Filters are case sensitive");
static_assert( ! compare_topic_filter("/bob", "bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob/", "bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob", "/bob"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert( ! compare_topic_filter("bob", "bob/"), "A leading or trailing ‘/’ creates a distinct Topic Name or Topic Filter");
static_assert(compare_topic_filter("bob/alice", "bob/alice"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/sue", "bob/alice/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob//////sue", "bob//////sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/#", "bob//////sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob///#", "bob/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/+/sue", "bob/alice/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob/+/sue", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert(compare_topic_filter("bob/alice/mary/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
static_assert( ! compare_topic_filter("bob/alice/mary/sue/#", "bob/alice/mary/sue"), "Each non-wildcarded level in the Topic Filter has to match the corresponding level in the Topic Name character for character for the match to succeed");
#endif // defined(MQTT_STD_STRING_VIEW)

class broker {
public:
    broker(as::io_context& ioc)
        :ioc_(ioc),
         tim_disconnect_(ioc_)
    {}

    // [begin] for test setting
    /**
     * @brief set_disconnect_delay adds a delay to disconnect operations.
     *
     * This makes the broker wait the specified amount between when a disconnect
     * is received from a client, and when the connection is actually closed in
     * the broker.
     *
     * @param delay - the amount to delay by
     */
    void set_disconnect_delay(std::chrono::steady_clock::duration delay) {
        delay_disconnect_ = force_move(delay);
    }

    /**
     * @brief set pingresp send operaton
     *
     * @param b - if true, send pingresp when pingreq is received.
     *            if false, doesn't send pingrespp for test.
     */
    void set_pingresp(bool b) {
        pingresp_ = b;
    }
    // [end] for test setting

    /**
     * @brief handle_accept
     *
     * Call this function when an server (of whatever kind) has accepted a raw
     * connection from an MQTT client. By 'raw connection', this might be raw TCP sockets
     * or websockets, or completed a TLS handshake, or any other underlying transport
     * type, but what is not meant is that the mqtt client on the other end of the endpoint
     * has initiated the MQTT application protocol connection sequence with CONNECT or CONACK
     * messages being sent or received.
     *
     * This function will assign several event handlers into server (of whatever kind)
     * that is provided as a parameter. This includes connection handlers, disconnection handlers
     * and various handlers for a variety of of MQTT message types.
     *
     * @param ep - The server (of whichever kind) to accept a connection on.
     */
    void handle_accept(con_sp_t spep) {
        con_wp_t wp(spep);
        endpoint_t& ep = *spep;

        ep.socket().lowest_layer().set_option(as::ip::tcp::no_delay(true));
        ep.set_auto_pub_response(false);
        // Pass spep to keep lifetime.
        // It makes sure wp.lock() never return nullptr in the handlers below
        // including close_handler and error_handler.
        ep.start_session(spep);

        // set connection (lower than MQTT) level handlers
        ep.set_close_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                close_proc(force_move(sp), true);
            });
        ep.set_error_handler(
            [this, wp]
            (error_code ec){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#S4_13_Errors
                if (ec == boost::system::errc::protocol_error) {
                    if (sp->connected()) {
                        sp->disconnect(v5::disconnect_reason_code::protocol_error);
                    }
                    else { // connecting
                        sp->connack(false, v5::connect_reason_code::protocol_error);
                    }
                }
                close_proc(force_move(sp), true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [this, wp]
            (buffer client_id,
             optional<buffer> username,
             optional<buffer> password,
             optional<will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    force_move(sp),
                    force_move(client_id),
                    force_move(username),
                    force_move(password),
                    force_move(will),
                    clean_session,
                    keep_alive,
                    v5::properties{}
                );
            }
        );
        ep.set_v5_connect_handler(
            [this, wp]
            (buffer client_id,
             optional<buffer> username,
             optional<buffer> password,
             optional<will> will,
             bool clean_start,
             std::uint16_t keep_alive,
             v5::properties props) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    force_move(sp),
                    force_move(client_id),
                    force_move(username),
                    force_move(password),
                    force_move(will),
                    clean_start,
                    keep_alive,
                    force_move(props)
                );
            }
        );
        ep.set_disconnect_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(force_move(sp));
            }
        );
        ep.set_v5_disconnect_handler(
            [this, wp]
            (v5::disconnect_reason_code /*reason_code*/, v5::properties props) {
                if (h_disconnect_props_) h_disconnect_props_(force_move(props));
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(force_move(sp));
            }
        );
        ep.set_puback_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return puback_handler(
                    force_move(sp),
                    packet_id,
                    v5::puback_reason_code::success,
                    v5::properties{}
                );
            }
        );
        ep.set_v5_puback_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::puback_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return puback_handler(
                    force_move(sp),
                    packet_id,
                    reason_code,
                    force_move(props)
                );
            }
        );
        ep.set_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubrec_handler(
                    force_move(sp),
                    packet_id,
                    v5::pubrec_reason_code::success,
                    v5::properties{}
                );
            }
        );
        ep.set_v5_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubrec_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubrec_handler(
                    force_move(sp),
                    packet_id,
                    reason_code,
                    force_move(props)
                );
            }
        );
        ep.set_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubrel_handler(
                    force_move(sp),
                    packet_id,
                    v5::pubrel_reason_code::success,
                    v5::properties{}
                );
            }
        );
        ep.set_v5_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubrel_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubrel_handler(
                    force_move(sp),
                    packet_id,
                    reason_code,
                    force_move(props)
                );
            }
        );
        ep.set_pubcomp_handler(
            [this, wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubcomp_handler(
                    force_move(sp),
                    packet_id,
                    v5::pubcomp_reason_code::success,
                    v5::properties{}
                );
            }
        );
        ep.set_v5_pubcomp_handler(
            [this, wp]
            (packet_id_t packet_id,
             v5::pubcomp_reason_code reason_code,
             v5::properties props){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return pubcomp_handler(
                    force_move(sp),
                    packet_id,
                    reason_code,
                    force_move(props)
                );
            }
        );
        ep.set_publish_handler(
            [this, wp]
            (optional<packet_id_t> packet_id,
             publish_options pubopts,
             buffer topic_name,
             buffer contents){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    force_move(sp),
                    packet_id,
                    pubopts,
                    force_move(topic_name),
                    force_move(contents),
                    v5::properties{}
                );
            }
        );
        ep.set_v5_publish_handler(
            [this, wp]
            (optional<packet_id_t> packet_id,
             publish_options pubopts,
             buffer topic_name,
             buffer contents,
             v5::properties props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    force_move(sp),
                    packet_id,
                    pubopts,
                    force_move(topic_name),
                    force_move(contents),
                    force_move(props)
                );
            }
        );
        ep.set_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<buffer, subscribe_options>> entries) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    force_move(sp),
                    packet_id,
                    force_move(entries),
                    v5::properties{}
                );
            }
        );
        ep.set_v5_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<buffer, subscribe_options>> entries,
             v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    force_move(sp),
                    packet_id,
                    force_move(entries),
                    force_move(props)
                );
            }
        );
        ep.set_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<buffer> topics) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    force_move(sp),
                    packet_id,
                    force_move(topics),
                    v5::properties{}
                );
            }
        );
        ep.set_v5_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<buffer> topics,
             v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    force_move(sp),
                    packet_id,
                    force_move(topics),
                    force_move(props)
                );
            }
        );
        ep.set_pingreq_handler(
            [this, wp] {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                if (pingresp_) sp->pingresp();
                return true;
            }
        );
        ep.set_v5_auth_handler(
            [this]
            (v5::auth_reason_code /*reason_code*/,
             v5::properties props
            ) {
                if (h_auth_props_) h_auth_props_(force_move(props));
                return true;
            }
        );
    }

    void set_connack_props(v5::properties props) {
        connack_props_ = force_move(props);
    }

    void set_suback_props(v5::properties props) {
        suback_props_ = force_move(props);
    }

    void set_unsuback_props(v5::properties props) {
        unsuback_props_ = force_move(props);
    }

    void set_puback_props(v5::properties props) {
        puback_props_ = force_move(props);
    }

    void set_pubrec_props(v5::properties props) {
        pubrec_props_ = force_move(props);
    }

    void set_pubrel_props(v5::properties props) {
        pubrel_props_ = force_move(props);
    }

    void set_pubcomp_props(v5::properties props) {
        pubcomp_props_ = force_move(props);
    }

    void set_connect_props_handler(std::function<void(v5::properties const&)> h) {
        h_connect_props_ = force_move(h);
    }

    void set_disconnect_props_handler(std::function<void(v5::properties const&)> h) {
        h_disconnect_props_ = force_move(h);
    }

    void set_publish_props_handler(std::function<void(v5::properties const&)> h) {
        h_publish_props_ = force_move(h);
    }

    void set_puback_props_handler(std::function<void(v5::properties const&)> h) {
        h_puback_props_ = force_move(h);
    }

    void set_pubrec_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubrec_props_ = force_move(h);
    }

    void set_pubrel_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubrel_props_ = force_move(h);
    }

    void set_pubcomp_props_handler(std::function<void(v5::properties const&)> h) {
        h_pubcomp_props_ = force_move(h);
    }

    void set_subscribe_props_handler(std::function<void(v5::properties const&)> h) {
        h_subscribe_props_ = force_move(h);
    }

    void set_unsubscribe_props_handler(std::function<void(v5::properties const&)> h) {
        h_unsubscribe_props_ = force_move(h);
    }

    void set_auth_props_handler(std::function<void(v5::properties const&)> h) {
        h_auth_props_ = force_move(h);
    }

    void clear_all_sessions() {
        sessions_.clear();
    }

    void clear_all_retained_topics() {
        retains_.clear();
    }

    template <typename T>
    static optional<T> get_property(v5::properties const& props) {
        optional<T> result;

        auto visitor = make_lambda_visitor(
            [&result](T const& t) { result = t; },
            [](auto const&) { }
        );

        for (auto const& p : props) {
            MQTT_NS::visit(visitor, p);
        }

        return result;
    }

    template <typename T>
    static void set_property(v5::properties& props, T&& v) {
        auto visitor = make_lambda_visitor(
            [&v](T& t) mutable { t = std::forward<T>(v); },
            [](auto&) { }
        );

        for (auto& p : props) {
            MQTT_NS::visit(visitor, p);
        }
    }

private:
    /**
     * @brief connect_proc Process an incoming CONNECT packet
     *
     * This is called by the connect_handler function, which is registered
     * on mqtt connections where the raw transport (tcp / tls / websocket / etc)
     * is established, but the CONNECT message has not been sent / received by
     * the mqtt client on the other end of the connection.
     *
     * When the CONNECT message is received, this function is called after some
     * basic pre-connection logic, to setup the record keeping that this broker
     * class needs to handle the connection and process subscriptions and publishing.
     *
     * @param clean_start - if the clean-start flag is set on the CONNECT message.
     * @param spep - varient of shared pointers to underlying connection type.
     * @param client_id - the id that the client wants to use
     * @param will - the last-will-and-testiment of the connection, if any.
     */
    bool connect_handler(
        con_sp_t spep,
        buffer client_id,
        optional<buffer> /*username*/,
        optional<buffer> /*password*/,
        optional<will> will,
        bool clean_start,
        std::uint16_t /*keep_alive*/,
        v5::properties props
    ) {
        auto& ep = *spep;

        optional<std::chrono::steady_clock::duration> session_expiry_interval;
        optional<std::chrono::steady_clock::duration> will_expiry_interval;

        if (ep.get_protocol_version() == protocol_version::v5) {
            auto v = get_property<v5::property::session_expiry_interval>(props);
            if (v && v.value().val() != 0) {
                session_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
            }

            if (will) {
                auto v = get_property<v5::property::message_expiry_interval>(will.value().props());
                if (v) {
                    will_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
                }
            }

            if (h_connect_props_) {
                h_connect_props_(props);
            }
        }

        // If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
        // If it's a not a clean session, but no client id is provided, we would have no way to map this
        // connection's session to a new connection later. So the connection must be rejected.
        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            if (client_id.empty() && !clean_start) {
                ep.connack(false, connect_return_code::identifier_rejected);
                return false;
            }
            break;
        case protocol_version::v5:
            if (client_id.empty() && !clean_start) {
                ep.connack(false, v5::connect_reason_code::client_identifier_not_valid);
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        auto send_connack =
            [&](bool session_present) {
                // Reply to the connect message.
                switch (ep.get_protocol_version()) {
                case protocol_version::v3_1_1:
                    ep.connack(
                        session_present,
                        connect_return_code::accepted
                    );
                    break;
                case protocol_version::v5:
                    ep.connack(
                        session_present,
                        v5::connect_reason_code::success,
                        connack_props_
                    );
                    break;
                default:
                    BOOST_ASSERT(false);
                    break;
                }
            };

        auto send_inflight_messages =
            [&] (session_state& session) {
                for (auto const& ifm : session.inflight_messages) {
                    optional<store_message_variant> msg;
                    if (ifm.tim_message_expiry) {
                        MQTT_NS::visit(
                            make_lambda_visitor(
                                [&](v5::basic_publish_message<sizeof(packet_id_t)> const& m) {
                                    auto updated_msg = m;
                                    auto& props = updated_msg.props();

                                    auto d =
                                        std::chrono::duration_cast<std::chrono::seconds>(
                                            ifm.tim_message_expiry->expiry() - std::chrono::steady_clock::now()
                                        ).count();
                                    if (d < 0) d = 0;
                                    set_property<v5::property::message_expiry_interval>(
                                        props,
                                        v5::property::message_expiry_interval(
                                            static_cast<uint32_t>(d)
                                        )
                                    );
                                    msg.emplace(force_move(updated_msg));
                                },
                                [](auto const&) {
                                }
                            ),
                            ifm.msg
                        );
                    }
                    session.con->send_store_message(msg ? msg.value() : ifm.msg, ifm.life_keeper);
                }
            };

        auto send_offline_messages =
            [&] (session_state& session) {
                try {
                    auto &seq_idx = session.offline_messages.get<tag_seq>();
                    while(!seq_idx.empty()) {
                        seq_idx.modify(
                            seq_idx.begin(),
                            [&](auto &i) {
                                auto props = force_move(i.props);

                                if (i.tim_message_expiry) {
                                    auto d =
                                        std::chrono::duration_cast<std::chrono::seconds>(
                                            i.tim_message_expiry->expiry() - std::chrono::steady_clock::now()
                                        ).count();
                                    if (d < 0) d = 0;
                                    set_property<v5::property::message_expiry_interval>(
                                        props,
                                        v5::property::message_expiry_interval(
                                            static_cast<uint32_t>(d)
                                        )
                                    );
                                }

                                session.con->publish(
                                    force_move(i.topic),
                                    force_move(i.contents),
                                    force_move(i.pubopts),
                                    force_move(props)
                                );
                            }
                        );
                        seq_idx.pop_front();
                    }
                }
                catch (packet_id_exhausted_error const& e) {
                    MQTT_LOG("mqtt_broker", warning)
                        << MQTT_ADD_VALUE(address, session.con.get())
                        << e.what();
                }
            };


        /**
         * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Toc514345311
         * 3.1.2.4 Clean Start
         * If a CONNECT packet is received with Clean Start is set to 1, the Client and Server MUST
         * discard any existing Session and start a new Session [MQTT-3.1.2-4]. Consequently,
         *  the Session Present flag in CONNACK is always set to 0 if Clean Start is set to 1.
         */

        // Find any sessions that have the same client_id
        auto& idx = sessions_.get<tag_cid>();
        auto it = idx.lower_bound(client_id);
        if (it == idx.end() || it->client_id != client_id) {
            // new connection
            it = idx.emplace_hint(
                it,
                ioc_,
                subs_map_,
                spep,
                client_id,
                force_move(will),
                force_move(will_expiry_interval),
                force_move(session_expiry_interval)
            );

            send_connack(false);
        }
        else if (it->online()) {
            // online overwrite
            if (close_proc(it->con, true)) {
                // remain offline
                if (clean_start) {
                    // discard offline session
                    send_connack(false);
                    idx.modify(
                        it,
                        [&](auto& e) {
                            e.clean();
                            e.update_will(ioc_, force_move(will), will_expiry_interval);
                            // TODO: e.will_delay = force_move(will_delay);
                            e.session_expiry_interval = force_move(session_expiry_interval);
                            e.tim_session_expiry.reset();
                        },
                        [](auto&) { BOOST_ASSERT(false); }
                    );
                }
                else {
                    // inherit offline session
                    send_connack(true);
                    idx.modify(
                        it,
                        [&](auto& e) {
                            e.con = spep;
                            if (e.topic_alias_recv) {
                                ep.restore_topic_alias_recv_container(force_move(e.topic_alias_recv.value()));
                                e.topic_alias_recv = nullopt;
                            }
                            e.update_will(ioc_, force_move(will), will_expiry_interval);
                            // TODO: e.will_delay = force_move(will_delay);
                            e.session_expiry_interval = force_move(session_expiry_interval);
                            e.tim_session_expiry.reset();
                            send_inflight_messages(e);
                            send_offline_messages(e);
                        },
                        [](auto&) { BOOST_ASSERT(false); }
                    );
                    // send offline messages
                }
            }
            else {
                // new connection
                bool inserted;
                std::tie(it, inserted) = idx.emplace(
                    ioc_,
                    subs_map_,
                    spep,
                    client_id,
                    force_move(will),
                    force_move(will_expiry_interval),
                    force_move(session_expiry_interval)
                );
                BOOST_ASSERT(inserted);
                send_connack(false);
            }
        }
        else {
            // offline -> online
            if (clean_start) {
                // discard offline session
                send_connack(false);
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.clean();
                        e.con = spep;
                        e.update_will(ioc_, force_move(will), will_expiry_interval);
                        // TODO: e.will_delay = force_move(will_delay);
                        e.session_expiry_interval = force_move(session_expiry_interval);
                        e.tim_session_expiry.reset();
                    },
                    [](auto&) { BOOST_ASSERT(false); }
                );
            }
            else {
                // inherit offline session
                send_connack(true);
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.con = spep;
                        if (e.topic_alias_recv) {
                            ep.restore_topic_alias_recv_container(force_move(e.topic_alias_recv.value()));
                            e.topic_alias_recv = nullopt;
                        }
                        e.update_will(ioc_, force_move(will), will_expiry_interval);
                        // TODO: e.will_delay = force_move(will_delay);
                        e.session_expiry_interval = force_move(session_expiry_interval);
                        e.tim_session_expiry.reset();
                        send_inflight_messages(e);
                        send_offline_messages(e);
                    },
                    [](auto&) { BOOST_ASSERT(false); }
                );
            }
        }

        return true;
    }

    void disconnect_handler(
        con_sp_t spep
    ) {
        if (delay_disconnect_) {
            tim_disconnect_.expires_after(delay_disconnect_.value());
            tim_disconnect_.wait();
        }
        close_proc(force_move(spep), false);
    }

    /**
     * @brief close_proc - clean up a connection that has been closed.
     *
     * @param ep - The underlying server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     * @return true if offline session is remained, otherwise false
     */
    // TODO: Maybe change the name of this function.
    bool close_proc(con_sp_t spep, bool send_will) {
        endpoint_t& ep = *spep;

        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);

        // act_sess_it == act_sess_idx.end() could happen if broker accepts
        // the session from client but the client closes the session  before sending
        // MQTT `CONNECT` message.
        // In this case, do nothing is correct behavior.
        if (it == idx.end()) return false;

        bool session_clear =
            [&] {
                if (ep.get_protocol_version() == protocol_version::v3_1_1) {
                    return ep.clean_session();
                }
                else {
                    BOOST_ASSERT(ep.get_protocol_version() == protocol_version::v5);
                    auto const& sei_opt = it->session_expiry_interval;
                    return !sei_opt || sei_opt.value() == std::chrono::steady_clock::duration::zero();
                }
            } ();

        auto do_send_will =
            [&](session_state& session) {
                if (session.will()) {
                    if (send_will) {
                        // TODO: This should be triggered by the will delay
                        // Not sent immediately.
                        try {
                            auto props = force_move(session.will().value().props());

                            if (session.get_tim_will_expiry()) {
                                auto d =
                                    std::chrono::duration_cast<std::chrono::seconds>(
                                        session.get_tim_will_expiry()->expiry() - std::chrono::steady_clock::now()
                                    ).count();
                                if (d < 0) d = 0;
                                set_property<v5::property::message_expiry_interval>(
                                    props,
                                    v5::property::message_expiry_interval(
                                        static_cast<uint32_t>(d)
                                    )
                                );
                            }

                            do_publish(
                                ep,
                                force_move(session.will().value().topic()),
                                force_move(session.will().value().message()),
                                session.will().value().get_qos() | session.will().value().get_retain(),
                                props
                            );
                        }
                        catch (packet_id_exhausted_error const& e) {
                            MQTT_LOG("mqtt_broker", warning)
                                << MQTT_ADD_VALUE(address, session.con.get())
                                << e.what();
                        }
                    }
                    else {
                        session.reset_will();
                    }
                }
            };

        if (session_clear) {
            idx.modify(
                it,
                [&](session_state& e) {
                    do_send_will(e);
                },
                [](auto&) { BOOST_ASSERT(false); }
            );
            idx.erase(it);
            BOOST_ASSERT(sessions_.get<tag_con>().find(spep) == sessions_.get<tag_con>().end());
            return false;
        }
        else {
            idx.modify(
                it,
                [&](session_state& e) {
                    do_send_will(e);

                    e.con->for_each_store_with_life_keeper(
                        [this, &e] (store_message_variant msg, any life_keeper) {
                            MQTT_LOG("mqtt_broker", trace)
                                << MQTT_ADD_VALUE(address, e.con.get())
                                << "store inflight message";

                            std::shared_ptr<as::steady_timer> tim_message_expiry;

                            MQTT_NS::visit(
                                make_lambda_visitor(
                                    [&](v5::basic_publish_message<sizeof(packet_id_t)> const& m) {
                                        auto v = get_property<v5::property::message_expiry_interval>(m.props());
                                        if (v) {
                                            tim_message_expiry =
                                                std::make_shared<as::steady_timer>(ioc_, std::chrono::seconds(v.value().val()));
                                            tim_message_expiry->async_wait(
                                                [&e, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)]
                                                (error_code ec) {
                                                    if (auto sp = wp.lock()) {
                                                        if (!ec) {
                                                            e.inflight_messages.get<tag_tim>().erase(sp);
                                                        }
                                                    }
                                                }
                                            );
                                        }
                                    },
                                    [&](auto const&) {}
                                ),
                                msg
                            );

                            e.inflight_messages.emplace_back(
                                force_move(msg),
                                force_move(life_keeper),
                                force_move(tim_message_expiry)
                            );
                        }
                    );

                    e.con.reset();

                    if (e.session_expiry_interval && e.session_expiry_interval.value() != std::chrono::seconds(session_never_expire)) {
                        e.tim_session_expiry = std::make_shared<as::steady_timer>(ioc_, e.session_expiry_interval.value());
                        e.tim_session_expiry->async_wait(
                            [this, wp = std::weak_ptr<as::steady_timer>(e.tim_session_expiry)](error_code ec) {
                                if (auto sp = wp.lock()) {
                                    if (!ec) {
                                        sessions_.get<tag_tim>().erase(sp);
                                    }
                                }
                            }
                        );
                    }

                    // TopicAlias lifetime is the same as Session lifetime
                    // It is different from MQTT v5 spec but practical choice.
                    // See
                    // https://lists.oasis-open.org/archives/mqtt-comment/202009/msg00000.html
                    e.topic_alias_recv = ep.get_topic_alias_recv_container();
                },
                [](auto&) { BOOST_ASSERT(false); }
            );

            return true;
        }

    }

    bool publish_handler(
        con_sp_t spep,
        optional<packet_id_t> packet_id,
        publish_options pubopts,
        buffer topic_name,
        buffer contents,
        v5::properties props) {

        auto& ep = *spep;

        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);
        BOOST_ASSERT(it != idx.end());

        auto send_pubrec =
            [&] {
                switch (pubopts.get_qos()) {
                case qos::at_least_once:
                    ep.puback(packet_id.value(), v5::puback_reason_code::success, puback_props_);
                    break;
                case qos::exactly_once: {
                    idx.modify(
                        it,
                        [&](auto& e) {
                            e.qos2_publish_processed.insert(packet_id.value());
                        }
                    );
                    ep.pubrec(packet_id.value(), v5::pubrec_reason_code::success, pubrec_props_);
                } break;
                default:
                    break;
                }
            };

        if (packet_id) {
            if (pubopts.get_qos() == qos::exactly_once &&
                it->qos2_publish_processed.find(packet_id.value()) != it->qos2_publish_processed.end()) {
                MQTT_LOG("mqtt_broker", info)
                    << MQTT_ADD_VALUE(address, spep.get())
                    << "receive already processed publish pid:" << packet_id.value();
                send_pubrec();
                return true;
            }
        }

        v5::properties forward_props;

        for (auto&& p : props) {
            MQTT_NS::visit(
                make_lambda_visitor(
                    [](v5::property::topic_alias&&) {
                        // TopicAlias is not forwarded
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901113
                        // A receiver MUST NOT carry forward any Topic Alias mappings from
                        // one Network Connection to another [MQTT-3.3.2-7].
                    },
                    [&spep](v5::property::subscription_identifier&& p) {
                        MQTT_LOG("mqtt_broker", warning)
                            << MQTT_ADD_VALUE(address, spep.get())
                            << "Subscription Identifier from client not forwarded sid:" << p.val();
                    },
                    [&forward_props](auto&& p) {
                        forward_props.push_back(force_move(p));
                    }
                ),
                force_move(p)
            );
        }

        do_publish(
            ep,
            force_move(topic_name),
            force_move(contents),
            pubopts.get_qos() | pubopts.get_retain(), // remove dup flag
            force_move(forward_props)
        );

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            switch (pubopts.get_qos()) {
            case qos::at_least_once:
                ep.puback(packet_id.value());
                break;
            case qos::exactly_once:
                send_pubrec();
                break;
            default:
                break;
            }
            break;
        case protocol_version::v5:
            switch (pubopts.get_qos()) {
            case qos::at_least_once:
                ep.puback(packet_id.value(), v5::puback_reason_code::success, puback_props_);
                break;
            case qos::exactly_once:
                send_pubrec();
                break;
            default:
                break;
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        return true;
    }

    bool puback_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::puback_reason_code /*reason_code*/,
        v5::properties /*props*/) {
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);
        BOOST_ASSERT(it != idx.end());
        idx.modify(
            it,
            [&](auto& e) {
                auto& idx = e.inflight_messages.template get<tag_pid>();
                idx.erase(packet_id);
            }
        );
        return true;
    }

    bool pubrec_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubrec_reason_code /*reason_code*/,
        v5::properties /*props*/) {
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);
        BOOST_ASSERT(it != idx.end());
        idx.modify(
            it,
            [&](auto& e) {
                auto& idx = e.inflight_messages.template get<tag_pid>();
                idx.erase(packet_id);
            }
        );

        auto& ep = *spep;

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.pubrel(packet_id);
            break;
        case protocol_version::v5:
            ep.pubrel(packet_id, v5::pubrel_reason_code::success, pubrel_props_);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        return true;
    }

    bool pubrel_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubrel_reason_code /*reason_code*/,
        v5::properties /*props*/) {
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);
        BOOST_ASSERT(it != idx.end());
        idx.modify(
            it,
            [&](auto& e) {
                e.qos2_publish_processed.erase(packet_id);
            }
        );

        auto& ep = *spep;

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.pubcomp(packet_id);
            break;
        case protocol_version::v5:
            ep.pubcomp(packet_id, v5::pubcomp_reason_code::success, pubcomp_props_);
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }
        return true;
    }

    bool pubcomp_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        v5::pubcomp_reason_code /*reason_code*/,
        v5::properties /*props*/){
        auto& idx = sessions_.get<tag_con>();
        auto it = idx.find(spep);
        BOOST_ASSERT(it != idx.end());
        idx.modify(
            it,
            [&](auto& e) {
                auto& idx = e.inflight_messages.template get<tag_pid>();
                idx.erase(packet_id);
            }
        );
        return true;
    }

    bool subscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<std::tuple<buffer, subscribe_options>> entries,
        v5::properties props) {

        auto& ep = *spep;

        auto publish_proc =
            [&ep](retain const& r, qos qos_value, optional<std::size_t> sid) {
                auto props = r.props;
                if (sid) {
                    props.push_back(v5::property::subscription_identifier(*sid));
                }
                if (r.tim_message_expiry) {
                    auto d =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            r.tim_message_expiry->expiry() - std::chrono::steady_clock::now()
                        ).count();
                    set_property<v5::property::message_expiry_interval>(
                        props,
                        v5::property::message_expiry_interval(
                            static_cast<uint32_t>(d)
                        )
                    );
                }
                ep.publish(
                    r.topic,
                    r.contents,
                    std::min(r.qos_value, qos_value) | MQTT_NS::retain::yes,
                    props
                );
            };

        std::vector<std::function<void()>> retain_deliver;
        retain_deliver.reserve(entries.size());

        auto insert_or_update_sub =
            [&](buffer topic_filter,
                subscribe_options subopts,
                optional<std::size_t> sid = nullopt) {

                auto& idx = sessions_.get<tag_con>();
                auto it = idx.find(spep);
                BOOST_ASSERT(it != idx.end());

                // The element of sessions_ must have longer lifetime
                // than corresponding subscription.
                // Because the subscription store the reference of the element.
                optional<session_state_ref> ssr_opt;
                idx.modify(
                    it,
                    [&](session_state& e) {
                        ssr_opt.emplace(e);
                    },
                    [](auto&) { BOOST_ASSERT(false); }
                );

                BOOST_ASSERT(ssr_opt);
                session_state_ref ssr {ssr_opt.value()};

                auto handle_ret = subs_map_.insert_or_assign(
                    force_move(topic_filter),
                    it->client_id,
                    subscription(ssr, subopts, sid)
                );

                auto rh = subopts.get_retain_handling();

                if (handle_ret.second) { // insert

                    MQTT_LOG("mqtt_broker", trace)
                        << MQTT_ADD_VALUE(address, spep.get())
                        << "subs_online_.emplace() " << "topic_filter:" << topic_filter << " qos:" << subopts.get_qos();

                    ssr.get().handles.insert(handle_ret.first);
                    if (rh == retain_handling::send ||
                        rh == retain_handling::send_only_new_subscription) {
                        retains_.find(
                            topic_filter,
                            [&](retain const& r) {
                                retain_deliver.emplace_back(
                                    [&publish_proc, &r, qos_value = subopts.get_qos(), sid] {
                                        publish_proc(r, qos_value, sid);
                                    }
                                );
                            }
                        );
                    }
                }
                else { // update

                    MQTT_LOG("mqtt_broker", trace)
                        << MQTT_ADD_VALUE(address, spep.get())
                        << "subs_online_.update " << "topic_filter:" << topic_filter << " qos:" << subopts.get_qos();

                    if (rh == retain_handling::send) {
                        retains_.find(
                            topic_filter,
                            [&](retain const& r) {
                                retain_deliver.emplace_back(
                                    [&publish_proc, &r, qos_value = subopts.get_qos(), sid] {
                                        publish_proc(r, qos_value, sid);
                                    }
                                );
                            }
                        );
                    }
                }
            };

        // subscription identifier
        optional<std::size_t> sid;

        // An in-order list of qos settings, used to send the reply.
        // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
        // allows the server to grant a lower QOS than requested
        // So we reply with the QOS setting that was granted
        // not the one requested.
        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1: {
            std::vector<suback_return_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                buffer topic_filter = std::get<0>(e);
                subscribe_options subopts = std::get<1>(e);
                res.emplace_back(qos_to_suback_return_code(subopts.get_qos())); // converts to granted_qos_x
                insert_or_update_sub(force_move(topic_filter), subopts);
            }
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, force_move(res));
        } break;
        case protocol_version::v5: {
            // Get subscription identifier
            auto v = get_property<v5::property::subscription_identifier>(props);
            if (v && v.value().val() != 0) {
                sid.emplace(v.value().val());
            }

            std::vector<v5::suback_reason_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                buffer topic_filter = std::get<0>(e);
                subscribe_options subopts = std::get<1>(e);
                res.emplace_back(v5::qos_to_suback_reason_code(subopts.get_qos())); // converts to granted_qos_x
                insert_or_update_sub(force_move(topic_filter), subopts, sid);
            }
            if (h_subscribe_props_) h_subscribe_props_(props);
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, force_move(res), suback_props_);
        } break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        for (auto const& f : retain_deliver) {
            f();
        }
        return true;
    }

    bool unsubscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<buffer> topic_filters,
        v5::properties props) {

        auto& ep = *spep;

        auto& idx = sessions_.get<tag_con>();
        auto it  = idx.find(spep);
        BOOST_ASSERT(it != idx.end());

        // For each subscription that this connection has
        // Compare against the list of topic filters, and remove
        // the subscription if the topic filter is in the list.
        for (auto const& topic_filter : topic_filters) {
            auto handle = subs_map_.lookup(topic_filter);
            if (handle) {
                idx.modify(
                    it,
                    [&](auto& e) {
                        e.handles.erase(handle.value());
                    },
                    [](auto&) { BOOST_ASSERT(false); }
                );
                subs_map_.erase(handle.value(), it->client_id);
            }
        }

        switch (ep.get_protocol_version()) {
        case protocol_version::v3_1_1:
            ep.unsuback(packet_id);
            break;
        case protocol_version::v5:
            if (h_unsubscribe_props_) h_unsubscribe_props_(props);
            ep.unsuback(
                packet_id,
                std::vector<v5::unsuback_reason_code>(
                    topic_filters.size(),
                    v5::unsuback_reason_code::success
                ),
                unsuback_props_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        return true;
    }

    /**
     * @brief do_publish Publish a message to any subscribed clients.
     *
     * @param ep - endpoint.
     * @param topic - The topic to publish the message on.
     * @param contents - The contents of the message.
     * @param qos - The QOS setting to use for the published message.
     * @param is_retain - Whether the message should be retained so it can
     *                    be sent to newly added subscriptions in the future.\
     */
    void do_publish(
        endpoint_t& ep,
        buffer topic,
        buffer contents,
        publish_options pubopts,
        v5::properties props) {

        subs_map_.modify(
            topic,
            [&](buffer const& /*key*/, subscription& sub) {
                // If NL (no local) subscription option is set and
                // publisher is the same as subscriber, then skip it.
                if (sub.subopts.get_nl() == nl::yes &&
                    sub.ss.get().con.get() ==  &ep) return;

                // publish the message to subscribers.
                // retain is delivered as the original only if rap_value is rap::retain.
                // On MQTT v3.1.1, rap_value is always rap::dont.

                publish_options new_pubopts = std::min(pubopts.get_qos(), sub.subopts.get_qos());
                if (sub.subopts.get_rap() == rap::retain && pubopts.get_retain() == MQTT_NS::retain::yes) {
                    new_pubopts |= MQTT_NS::retain::yes;
                }

                if (sub.sid) {
                    props.push_back(v5::property::subscription_identifier(sub.sid.value()));
                    sub.ss.get().deliver(
                        ioc_,
                        topic,
                        contents,
                        new_pubopts,
                        props
                    );
                    props.pop_back();
                }
                else {
                    sub.ss.get().deliver(
                        ioc_,
                        topic,
                        contents,
                        new_pubopts,
                        props
                    );
                }

            }
        );

        optional<std::chrono::steady_clock::duration> message_expiry_interval;
        if (ep.get_protocol_version() == protocol_version::v5) {
            auto v = get_property<v5::property::message_expiry_interval>(props);
            if (v) {
                message_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
            }

        }

        /*
         * If the message is marked as being retained, then we
         * keep it in case a new subscription is added that matches
         * this topic.
         *
         * @note: The MQTT standard 3.3.1.3 RETAIN makes it clear that
         *        retained messages are global based on the topic, and
         *        are not scoped by the client id. So any client may
         *        publish a retained message on any topic, and the most
         *        recently published retained message on a particular
         *        topic is the message that is stored on the server.
         *
         * @note: The standard doesn't make it clear that publishing
         *        a message with zero length, but the retain flag not
         *        set, does not result in any existing retained message
         *        being removed. However, internet searching indicates
         *        that most brokers have opted to keep retained messages
         *        when receiving contents of zero bytes, unless the so
         *        received message has the retain flag set, in which case
         *        the retained message is removed.
         */
        if (pubopts.get_retain() == MQTT_NS::retain::yes) {
            if (contents.empty()) {
                retains_.erase(topic);
            }
            else {
                std::shared_ptr<as::steady_timer> tim_message_expiry;
                if (message_expiry_interval) {
                    tim_message_expiry = std::make_shared<as::steady_timer>(ioc_, message_expiry_interval.value());
                    tim_message_expiry->async_wait(
                        [this, topic = topic, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)]
                        (boost::system::error_code const& ec) {
                            if (auto sp = wp.lock()) {
                                if (!ec) {
                                    retains_.erase(topic);
                                }
                            }
                        }
                    );
                }

                retains_.insert_or_assign(
                    topic,
                    retain {
                        force_move(topic),
                        force_move(contents),
                        force_move(props),
                        pubopts.get_qos(),
                        tim_message_expiry
                    }
                );
            }
        }
    }

private:
    struct tag_seq {};
    struct tag_con {};
    struct tag_topic{};
    struct tag_topic_filter{};
    struct tag_con_topic_filter {};
    struct tag_cid {};
    struct tag_cid_topic_filter {};
    struct tag_tim {};
    struct tag_pid {};

    struct session_state;
    using session_state_ref = std::reference_wrapper<session_state>;

    struct subscription {
        subscription(
            session_state_ref ss,
            subscribe_options subopts,
            optional<std::size_t> sid)
            :ss { ss },
             subopts { subopts },
             sid { sid }
        {}

        session_state_ref ss;
        subscribe_options subopts;
        optional<std::size_t> sid;
    };

    friend bool operator<(subscription const& lhs, subscription const& rhs) {
        return &lhs.ss.get() < &rhs.ss.get();
    }

    struct buffer_hasher  {
        std::size_t operator()(buffer const& b) const noexcept {
            std::size_t result = 0;
            boost::hash_combine(result, b);
            return result;
        }
    };

    using sub_con_map = multiple_subscription_map<buffer, subscription, buffer_hasher>;

    struct inflight_message {
        inflight_message(
            store_message_variant msg,
            any life_keeper,
            std::shared_ptr<as::steady_timer> tim_message_expiry)
            :msg { force_move(msg) },
             life_keeper { force_move(life_keeper) },
             tim_message_expiry { force_move(tim_message_expiry) }
        {}

        packet_id_t packet_id() const {
            return
                MQTT_NS::visit(
                    make_lambda_visitor(
                        [](auto const& m) {
                            return m.packet_id();
                        }
                    ),
                    msg
                );
        }
        store_message_variant msg;
        any life_keeper;
        std::shared_ptr<as::steady_timer> tim_message_expiry;
    };

    using mi_inflight_message = mi::multi_index_container<
        inflight_message,
        mi::indexed_by<
            mi::sequenced<
                mi::tag<tag_seq>
            >,
            mi::ordered_unique<
                mi::tag<tag_pid>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(inflight_message, packet_id_t, packet_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(inflight_message, std::shared_ptr<as::steady_timer>, tim_message_expiry)
            >
        >
    >;

    // The offline_message structure holds messages that have been published on a
    // topic that a not-currently-connected client is subscribed to.
    // When a new connection is made with the client id for this saved data,
    // these messages will be published to that client, and only that client.
    struct offline_message {
        offline_message(
            buffer topic,
            buffer contents,
            v5::properties props,
            publish_options pubopts,
            std::shared_ptr<as::steady_timer> tim_message_expiry)
            : topic(force_move(topic)),
              contents(force_move(contents)),
              props(force_move(props)),
              pubopts(pubopts),
              tim_message_expiry(force_move(tim_message_expiry))
        { }

        buffer topic;
        buffer contents;
        v5::properties props;
        publish_options pubopts;

        std::shared_ptr<as::steady_timer> tim_message_expiry;
    };

    using mi_offline_message = mi::multi_index_container<
        offline_message,
        mi::indexed_by<
            mi::sequenced<
                mi::tag<tag_seq>
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(offline_message, std::shared_ptr<as::steady_timer>, tim_message_expiry)
            >
        >
    >;

    /**
     * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Session_State
     *
     * 4.1 Session State
     * In order to implement QoS 1 and QoS 2 protocol flows the Client and Server need to associate state with the Client Identifier, this is referred to as the Session State. The Server also stores the subscriptions as part of the Session State.
     * The session can continue across a sequence of Network Connections. It lasts as long as the latest Network Connection plus the Session Expiry Interval.
     * The Session State in the Server consists of:
     * · The existence of a Session, even if the rest of the Session State is empty.
     * · The Clients subscriptions, including any Subscription Identifiers.
     * · QoS 1 and QoS 2 messages which have been sent to the Client, but have not been completely acknowledged.
     * · QoS 1 and QoS 2 messages pending transmission to the Client and OPTIONALLY QoS 0 messages pending transmission to the Client.
     * · QoS 2 messages which have been received from the Client, but have not been completely acknowledged.
     * · The Will Message and the Will Delay Interval
     * · If the Session is currently not connected, the time at which the Session will end and Session State will be discarded.
     *
     * Retained messages do not form part of the Session State in the Server, they are not deleted as a result of a Session ending.
     */
    struct session_state {
        // TODO: Currently not fully implemented...
        session_state(
            as::io_context& ioc,
            sub_con_map& subs_map,
            con_sp_t con,
            buffer client_id,
            optional<will> will,
            optional<std::chrono::steady_clock::duration> will_expiry_interval,
            optional<std::chrono::steady_clock::duration> session_expiry_interval = nullopt)
            :subs_map_(subs_map),
             con(force_move(con)),
             client_id(force_move(client_id)),
             session_expiry_interval(force_move(session_expiry_interval))
        {
            update_will(ioc, will, will_expiry_interval);
        }

        session_state() = default;
        session_state(session_state&&) = default;

        ~session_state() {
            clean();
        }

        bool online() const {
            return bool(con);
        }

        void deliver(
            as::io_context& ioc,
            buffer pub_topic,
            buffer contents,
            publish_options pubopts,
            v5::properties props) {

            if (online()) {
                // TODO: Probably this should be switched to async_publish?

                //       Given the async_client / sync_client seperation
                //       and the way they have different function names,
                //       it wouldn't be possible for broker.hpp to be
                //       used with some hypothetical "async_server" in the future.
                con->publish(
                    force_move(pub_topic),
                    force_move(contents),
                    pubopts,
                    force_move(props)
                );
            }
            else {
                optional<std::chrono::steady_clock::duration> message_expiry_interval;

                auto v = get_property<v5::property::message_expiry_interval>(props);
                if (v) {
                    message_expiry_interval.emplace(std::chrono::seconds(v.value().val()));
                }

                std::shared_ptr<as::steady_timer> tim_message_expiry;
                if (message_expiry_interval) {
                    tim_message_expiry = std::make_shared<as::steady_timer>(ioc, message_expiry_interval.value());
                    tim_message_expiry->async_wait(
                        [this, wp = std::weak_ptr<as::steady_timer>(tim_message_expiry)](error_code ec) mutable {
                            if (auto sp = wp.lock()) {
                                if (!ec) {
                                    offline_messages.get<tag_tim>().erase(sp);
                                }
                            }
                        }
                    );
                }

                auto& seq_idx = offline_messages.get<tag_seq>();
                seq_idx.emplace_back(
                    force_move(pub_topic),
                    force_move(contents),
                    force_move(props),
                    pubopts,
                    force_move(tim_message_expiry)
                );
            }
        }

        void clean() {
            topic_alias_recv = nullopt;
            inflight_messages.clear();
            offline_messages.clear();
            qos2_publish_processed.clear();
            unsubscribe_all();
        }

        void unsubscribe_all() {
            for (auto const& h : handles) {
                subs_map_.value().get().erase(h, client_id);
            }
            handles.clear();
        }

        optional<std::reference_wrapper<sub_con_map>> subs_map_;
        con_sp_t con;
        buffer client_id;

        optional<std::chrono::steady_clock::duration> will_delay;
        optional<std::chrono::steady_clock::duration> session_expiry_interval;
        std::shared_ptr<as::steady_timer> tim_session_expiry;
        optional<topic_alias_recv_map_t> topic_alias_recv;

        mi_inflight_message inflight_messages;
        std::set<packet_id_t> qos2_publish_processed;

        mi_offline_message offline_messages;

        std::set<sub_con_map::handle> handles; // to efficient remove

        void update_will(
            as::io_context& ioc,
            optional<MQTT_NS::will> will,
            optional<std::chrono::steady_clock::duration> will_expiry_interval) {
            tim_will_expiry.reset();
            will_value = force_move(will);

            if (will_value && will_expiry_interval) {
                tim_will_expiry = std::make_shared<as::steady_timer>(ioc, will_expiry_interval.value());
                tim_will_expiry->async_wait(
                    [this, client_id = client_id, wp = std::weak_ptr<as::steady_timer>(tim_will_expiry)]
                    (error_code ec) {
                        if (auto sp = wp.lock()) {
                            if (!ec) {
                                reset_will();
                            }
                        }
                    }
                );
            }
        }

        void reset_will() {
            tim_will_expiry.reset();
            will_value = nullopt;
        }

        optional<MQTT_NS::will>& will() { return will_value; }
        optional<MQTT_NS::will> const& will() const { return will_value; }

        std::shared_ptr<as::steady_timer>& get_tim_will_expiry() { return tim_will_expiry; }
    private:
        std::shared_ptr<as::steady_timer> tim_will_expiry;
        optional<MQTT_NS::will> will_value;

    };

    // The mi_session_online container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_session_state = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            // non is nullable
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(session_state, con_sp_t, con)
            >,
            mi::ordered_unique<
                mi::tag<tag_cid>,
                BOOST_MULTI_INDEX_MEMBER(session_state, buffer, client_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(session_state, std::shared_ptr<as::steady_timer>, tim_session_expiry)
            >
        >
    >;

    // A collection of messages that have been retained in
    // case clients add a new subscription to the associated topics.
    struct retain {
        retain(
            buffer topic,
            buffer contents,
            v5::properties props,
            qos qos_value,
            std::shared_ptr<as::steady_timer> tim_message_expiry = std::shared_ptr<as::steady_timer>())
            :topic(force_move(topic)),
             contents(force_move(contents)),
             props(force_move(props)),
             qos_value(qos_value),
             tim_message_expiry(force_move(tim_message_expiry))
        { }

        buffer topic;
        buffer contents;
        v5::properties props;
        qos qos_value;
        std::shared_ptr<as::steady_timer> tim_message_expiry;
    };
    using retained_messages = retained_topic_map<retain>;

    as::io_context& ioc_; ///< The boost asio context to run this broker on.
    as::steady_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    optional<std::chrono::steady_clock::duration> delay_disconnect_; ///< Used to delay disconnect handling for testing

    sub_con_map subs_map_;   /// subscription information
    mi_session_state sessions_; ///< Map of active client id and connections

    retained_messages retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.

    // MQTTv5 members
    v5::properties connack_props_;
    v5::properties suback_props_;
    v5::properties unsuback_props_;
    v5::properties puback_props_;
    v5::properties pubrec_props_;
    v5::properties pubrel_props_;
    v5::properties pubcomp_props_;
    std::function<void(v5::properties const&)> h_connect_props_;
    std::function<void(v5::properties const&)> h_disconnect_props_;
    std::function<void(v5::properties const&)> h_publish_props_;
    std::function<void(v5::properties const&)> h_puback_props_;
    std::function<void(v5::properties const&)> h_pubrec_props_;
    std::function<void(v5::properties const&)> h_pubrel_props_;
    std::function<void(v5::properties const&)> h_pubcomp_props_;
    std::function<void(v5::properties const&)> h_subscribe_props_;
    std::function<void(v5::properties const&)> h_unsubscribe_props_;
    std::function<void(v5::properties const&)> h_auth_props_;
    bool pingresp_ = true;
};

} // namespace MQTT_NS

#endif // MQTT_BROKER_HPP

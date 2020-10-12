// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_BROKER_HPP)
#define MQTT_TEST_BROKER_HPP

#include <iostream>
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt_server_cpp.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/visitor_util.hpp>

#include "test_settings.hpp"


namespace mi = boost::multi_index;
namespace as = boost::asio;

using endpoint_t = MQTT_NS::server<>::endpoint_t;
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
bool validate_topic_filter(MQTT_NS::string_view topic_filter) {
    /*
     * Confirm the topic pattern is valid before registering it.
     * Use rules from http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    if(topic_filter.empty() || (topic_filter.size() > std::numeric_limits<std::uint16_t>::max())) {
        return false;
    }

    for(MQTT_NS::string_view::size_type idx = topic_filter.find_first_of(MQTT_NS::string_view("\0+#", 3));
        MQTT_NS::string_view::npos != idx;
        idx = topic_filter.find_first_of(MQTT_NS::string_view("\0+#", 3), idx+1)) {
        BOOST_ASSERT(   ('\0' == topic_filter[idx])
                     || ('+'  == topic_filter[idx])
                     || ('#'  == topic_filter[idx]));
        if('\0' == topic_filter[idx]) {
            // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
            return false;
        }
        else if('+' == topic_filter[idx]) {
            /*
             * Either must be the first character,
             * or be preceeded by a topic seperator.
             */
            if((0 != idx) && ('/' != topic_filter[idx-1])) {
                return false;
            }

            /*
             * Either must be the last character,
             * or be followed by a topic seperator.
             */
            if((topic_filter.size()-1 != idx) && ('/' != topic_filter[idx+1])) {
                return false;
            }
        }
        // multilevel wildcard
        else if('#' == topic_filter[idx]) {
            /*
             * Must be absolute last character.
             * Must only be one multi level wild card.
             */
            if(idx != topic_filter.size()-1) {
                return false;
            }

            /*
             * If not the first character, then the
             * immediately preceeding character must
             * be a topic level separator.
             */
            if((0 != idx) && ('/' != topic_filter[idx-1])) {
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
static_assert( ! validate_topic_filter(MQTT_NS::string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
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
bool validate_topic_name(MQTT_NS::string_view topic_name) {
    /*
     * Confirm the topic name is valid
     * Use rules from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
     */

    // All Topic Names and Topic Filters MUST be at least one character long
    // Topic Names and Topic Filters are UTF-8 Encoded Strings; they MUST NOT encode to more than 65,535 bytes
    // The wildcard characters can be used in Topic Filters, but MUST NOT be used within a Topic Name
    // Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)
    return    ! topic_name.empty()
           && (topic_name.size() <= std::numeric_limits<std::uint16_t>::max())
           && (MQTT_NS::string_view::npos == topic_name.find_first_of(MQTT_NS::string_view("\0+#", 3)));
}

#if defined(MQTT_STD_STRING_VIEW)
// The following rules come from https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901247
static_assert( ! validate_topic_name(""), "All Topic Names and Topic Filters MUST be at least one character long");
static_assert(validate_topic_name("/"), "A Topic Name or Topic Filter consisting only of the ‘/’ character is valid");
static_assert( ! validate_topic_name(MQTT_NS::string_view("\0", 1)), "Topic Names and Topic Filters MUST NOT include the null character (Unicode U+0000)");
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
bool compare_topic_filter(MQTT_NS::string_view topic_filter, MQTT_NS::string_view topic_name) {
    if( ! validate_topic_filter(topic_filter)) {
        BOOST_ASSERT(validate_topic_filter(topic_filter));
        return false;
    }

    if( ! validate_topic_name(topic_name)) {
        BOOST_ASSERT(validate_topic_name(topic_name));
        return false;
    }

    // TODO: The Server MUST NOT match Topic Filters starting with a wildcard character (# or +) with Topic Names beginning with a $ character
    for(MQTT_NS::string_view::size_type idx = topic_filter.find_first_of("+#");
        MQTT_NS::string_view::npos != idx;
        idx = topic_filter.find_first_of("+#")) {
        BOOST_ASSERT(   ('+' == topic_filter[idx])
                     || ('#' == topic_filter[idx]));
        if('+' == topic_filter[idx]) {
            // Compare everything up to the first +
            if(topic_filter.substr(0, idx) == topic_name.substr(0, idx)) {
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

std::pair<MQTT_NS::buffer, MQTT_NS::buffer> parse_shared_subscription(MQTT_NS::buffer topic) {
    auto const shared_prefix = MQTT_NS::string_view("$share/");
    if(topic.substr(0, shared_prefix.size()) != shared_prefix) {
        return {MQTT_NS::buffer{}, MQTT_NS::force_move(topic)};
    }

    // Remove $share/
    topic.remove_prefix(shared_prefix.size());

    // This is the '/' seperating the subscription group from the actual topic.
    auto const idx = topic.find_first_of('/');

    // We return the share and the topic as buffers that point to the same
    // storage. So we grab the substr for "share", and then remove it from topic.
    auto share = topic.substr(0, idx);
    topic.remove_prefix(idx+1);

    return {MQTT_NS::force_move(share), MQTT_NS::force_move(topic)};
}

class test_broker {
public:
    test_broker(as::io_context& ioc)
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
        delay_disconnect_ = MQTT_NS::force_move(delay);
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
     * Call this function when an MQTT_NS::server (of whatever kind) has accepted a raw
     * connection from an MQTT client. By 'raw connection', this might be raw TCP sockets
     * or websockets, or completed a TLS handshake, or any other underlying transport
     * type, but what is not meant is that the mqtt client on the other end of the endpoint
     * has initiated the MQTT application protocol connection sequence with CONNECT or CONACK
     * messages being sent or received.
     *
     * This function will assign several event handlers into MQTT_NS::server (of whatever kind)
     * that is provided as a parameter. This includes connection handlers, disconnection handlers
     * and various handlers for a variety of of MQTT message types.
     *
     * @param ep - The MQTT_NS::server (of whichever kind) to accept a connection on.
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
                close_proc(MQTT_NS::force_move(sp), true);
            });
        ep.set_error_handler(
            [this, wp]
            (MQTT_NS::error_code ec){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#S4_13_Errors
                if (ec == boost::system::errc::protocol_error) {
                    if (sp->connected()) {
                        sp->disconnect(MQTT_NS::v5::disconnect_reason_code::protocol_error);
                    }
                    else { // connecting
                        sp->connack(false, MQTT_NS::v5::connect_reason_code::protocol_error);
                    }
                }
                close_proc(MQTT_NS::force_move(sp), true);
            });

        // set MQTT level handlers
        ep.set_connect_handler(
            [this, wp]
            (MQTT_NS::buffer client_id,
             MQTT_NS::optional<MQTT_NS::buffer> username,
             MQTT_NS::optional<MQTT_NS::buffer> password,
             MQTT_NS::optional<MQTT_NS::will> will,
             bool clean_session,
             std::uint16_t keep_alive) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    MQTT_NS::force_move(sp),
                    MQTT_NS::force_move(client_id),
                    MQTT_NS::force_move(username),
                    MQTT_NS::force_move(password),
                    MQTT_NS::force_move(will),
                    clean_session,
                    keep_alive,
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_connect_handler(
            [this, wp]
            (MQTT_NS::buffer client_id,
             MQTT_NS::optional<MQTT_NS::buffer> username,
             MQTT_NS::optional<MQTT_NS::buffer> password,
             MQTT_NS::optional<MQTT_NS::will> will,
             bool clean_session,
             std::uint16_t keep_alive,
             MQTT_NS::v5::properties props) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return connect_handler(
                    MQTT_NS::force_move(sp),
                    MQTT_NS::force_move(client_id),
                    MQTT_NS::force_move(username),
                    MQTT_NS::force_move(password),
                    MQTT_NS::force_move(will),
                    clean_session,
                    keep_alive,
                    MQTT_NS::force_move(props)
                );
            }
        );
        ep.set_disconnect_handler(
            [this, wp]
            (){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(MQTT_NS::force_move(sp));
            }
        );
        ep.set_v5_disconnect_handler(
            [this, wp]
            (MQTT_NS::v5::disconnect_reason_code /*reason_code*/, MQTT_NS::v5::properties props) {
                if (h_disconnect_props_) h_disconnect_props_(MQTT_NS::force_move(props));
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return disconnect_handler(MQTT_NS::force_move(sp));
            }
        );
        ep.set_puback_handler(
            []
            (packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_puback_handler(
            []
            (packet_id_t /*packet_id*/,
             MQTT_NS::v5::puback_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                return true;
            });
        ep.set_pubrec_handler(
            [wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubrel(packet_id);
                return true;
            });
        ep.set_v5_pubrec_handler(
            [this, wp]
            (packet_id_t packet_id,
             MQTT_NS::v5::pubrec_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubrel(packet_id, MQTT_NS::v5::pubrel_reason_code::success, pubrel_props_);
                return true;
            });
        ep.set_pubrel_handler(
            [wp]
            (packet_id_t packet_id){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubcomp(packet_id);
                return true;
            });
        ep.set_v5_pubrel_handler(
            [this, wp]
            (packet_id_t packet_id,
             MQTT_NS::v5::pubrel_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->pubcomp(packet_id, MQTT_NS::v5::pubcomp_reason_code::success, pubcomp_props_);
                return true;
            });
        ep.set_pubcomp_handler(
            []
            (packet_id_t /*packet_id*/){
                return true;
            });
        ep.set_v5_pubcomp_handler(
            []
            (packet_id_t /*packet_id*/,
             MQTT_NS::v5::pubcomp_reason_code /*reason_code*/,
             MQTT_NS::v5::properties /*props*/){
                return true;
            });
        ep.set_publish_handler(
            [this, wp]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic_name,
             MQTT_NS::buffer contents){
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    pubopts,
                    MQTT_NS::force_move(topic_name),
                    MQTT_NS::force_move(contents),
                    MQTT_NS::v5::properties{}
                );
            });
        ep.set_v5_publish_handler(
            [this, wp]
            (MQTT_NS::optional<packet_id_t> packet_id,
             MQTT_NS::publish_options pubopts,
             MQTT_NS::buffer topic_name,
             MQTT_NS::buffer contents,
             MQTT_NS::v5::properties props
            ) {
                if (h_publish_props_) h_publish_props_(props);
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return publish_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    pubopts,
                    MQTT_NS::force_move(topic_name),
                    MQTT_NS::force_move(contents),
                    MQTT_NS::force_move(props)
                );
            });
        ep.set_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(entries),
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_subscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries,
             MQTT_NS::v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return subscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(entries),
                    MQTT_NS::force_move(props)
                );
            }
        );
        ep.set_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<MQTT_NS::buffer> topics) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(topics),
                    MQTT_NS::v5::properties{}
                );
            }
        );
        ep.set_v5_unsubscribe_handler(
            [this, wp]
            (packet_id_t packet_id,
             std::vector<MQTT_NS::buffer> topics,
             MQTT_NS::v5::properties props
            ) {
                con_sp_t sp = wp.lock();
                BOOST_ASSERT(sp);
                return unsubscribe_handler(
                    MQTT_NS::force_move(sp),
                    packet_id,
                    MQTT_NS::force_move(topics),
                    MQTT_NS::force_move(props)
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
            (MQTT_NS::v5::auth_reason_code /*reason_code*/,
             MQTT_NS::v5::properties props
            ) {
                if (h_auth_props_) h_auth_props_(MQTT_NS::force_move(props));
                return true;
            }
        );
    }

    void set_connack_props(MQTT_NS::v5::properties props) {
        connack_props_ = MQTT_NS::force_move(props);
    }

    void set_suback_props(MQTT_NS::v5::properties props) {
        suback_props_ = MQTT_NS::force_move(props);
    }

    void set_unsuback_props(MQTT_NS::v5::properties props) {
        unsuback_props_ = MQTT_NS::force_move(props);
    }

    void set_puback_props(MQTT_NS::v5::properties props) {
        puback_props_ = MQTT_NS::force_move(props);
    }

    void set_pubrec_props(MQTT_NS::v5::properties props) {
        pubrec_props_ = MQTT_NS::force_move(props);
    }

    void set_pubrel_props(MQTT_NS::v5::properties props) {
        pubrel_props_ = MQTT_NS::force_move(props);
    }

    void set_pubcomp_props(MQTT_NS::v5::properties props) {
        pubcomp_props_ = MQTT_NS::force_move(props);
    }

    void set_connect_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_connect_props_ = MQTT_NS::force_move(h);
    }

    void set_disconnect_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_disconnect_props_ = MQTT_NS::force_move(h);
    }

    void set_publish_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_publish_props_ = MQTT_NS::force_move(h);
    }

    void set_puback_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_puback_props_ = MQTT_NS::force_move(h);
    }

    void set_pubrec_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubrec_props_ = MQTT_NS::force_move(h);
    }

    void set_pubrel_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubrel_props_ = MQTT_NS::force_move(h);
    }

    void set_pubcomp_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_pubcomp_props_ = MQTT_NS::force_move(h);
    }

    void set_subscribe_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_subscribe_props_ = MQTT_NS::force_move(h);
    }

    void set_unsubscribe_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_unsubscribe_props_ = MQTT_NS::force_move(h);
    }

    void set_auth_props_handler(std::function<void(MQTT_NS::v5::properties const&)> h) {
        h_auth_props_ = MQTT_NS::force_move(h);
    }

    void clear_all_sessions() {
        active_sessions_.clear();
        non_active_sessions_.clear();
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
     * @param clean_session - if the clean-session flag is set on the CONNECT message.
     * @param spep - MQTT_NS::varient of shared pointers to underlying connection type.
     * @param client_id - the id that the client wants to use
     * @param will - the last-will-and-testiment of the connection, if any.
     */
    bool connect_handler(
        con_sp_t spep,
        MQTT_NS::buffer client_id,
        MQTT_NS::optional<MQTT_NS::buffer> /*username*/,
        MQTT_NS::optional<MQTT_NS::buffer> /*password*/,
        MQTT_NS::optional<MQTT_NS::will> will,
        bool clean_session,
        std::uint16_t /*keep_alive*/,
        MQTT_NS::v5::properties props
    ) {
        auto& ep = *spep;

        MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval;

        if (ep.get_protocol_version() == MQTT_NS::protocol_version::v5) {
            for (auto const& p : props) {
                MQTT_NS::visit(
                    MQTT_NS::make_lambda_visitor(
                        [&session_expiry_interval](MQTT_NS::v5::property::session_expiry_interval const& t) {
                            if (t.val() != 0) {
                                session_expiry_interval.emplace(std::chrono::seconds(t.val()));
                            }
                        },
                        [](auto&& ...) {
                        }
                    ),
                    p
                );
            }

            if (h_connect_props_) {
                h_connect_props_(props);
            }
        }

        // If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
        // If it's a not a clean session, but no client id is provided, we would have no way to map this
        // connection's session to a new connection later. So the connection must be rejected.
        // TODO: When the clientid is zero bytes, it is supposed to be assigned a unique id by the broker.
        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            if (client_id.empty() && !clean_session) {
                ep.connack(false, MQTT_NS::connect_return_code::identifier_rejected);
                return false;
            }
            break;
        case MQTT_NS::protocol_version::v5:
            if (client_id.empty() && !clean_session) {
                ep.connack(false, MQTT_NS::v5::connect_reason_code::client_identifier_not_valid);
                return false;
            }
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        // Find any sessions that have the same client_id
        auto & act_sess_idx = active_sessions_.get<tag_client_id>();
        auto act_sess_it = act_sess_idx.find(client_id);

        auto & non_act_sess_idx = non_active_sessions_.get<tag_client_id>();
        auto non_act_sess_it = non_act_sess_idx.find(client_id);

        // It's a hard error for a client_id to have a session object
        // in both the active and non active session containers.
        BOOST_ASSERT((act_sess_idx.end() == act_sess_it) || (non_act_sess_idx.end() == non_act_sess_it));

        // Reply to the connect message.
        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                MQTT_NS::connect_return_code::accepted
            );
            break;
        case MQTT_NS::protocol_version::v5:
            ep.connack(
                !clean_session && ((act_sess_idx.end() != act_sess_it) || (non_act_sess_idx.end() != non_act_sess_it)),
                MQTT_NS::v5::connect_reason_code::success,
                connack_props_
            );
            break;
        default:
            BOOST_ASSERT(false);
            break;
        }

        /**
         * http://docs.oasis-open.org/mqtt/mqtt/v5.0/cs02/mqtt-v5.0-cs02.html#_Toc514345311
         * 3.1.2.4 Clean Start
         * If a CONNECT packet is received with Clean Start is set to 1, the Client and Server MUST
         * discard any existing Session and start a new Session [MQTT-3.1.2-4]. Consequently,
         *  the Session Present flag in CONNACK is always set to 0 if Clean Start is set to 1.
         */
        if(clean_session && (non_act_sess_it != non_act_sess_idx.end())) {
            non_act_sess_idx.erase(non_act_sess_it);
            BOOST_ASSERT(non_act_sess_idx.end() == non_act_sess_idx.find(client_id));

            non_act_sess_it = non_act_sess_idx.end();
        }

        if(act_sess_it == act_sess_idx.end()) {
            // If we have a saved session, we can transfer the state from it
            // to the active_session container.
            if(non_act_sess_it == non_act_sess_idx.end()) {
                auto const& ret = active_sessions_.emplace(
                    spep,
                    client_id,
                    MQTT_NS::force_move(will),
                    MQTT_NS::force_move(session_expiry_interval)
                );
                BOOST_ASSERT(ret.second);
                act_sess_it = active_sessions_.project<tag_client_id>(ret.first);
                BOOST_ASSERT(act_sess_it->client_id == client_id);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
            }
            else {

                if (non_act_sess_it->topic_alias_recv) {

                    // TopicAlias lifetime is the same as Session lifetime
                    // It is different from MQTT v5 spec but practical choice.
                    // See
                    // https://lists.oasis-open.org/archives/mqtt-comment/202009/msg00000.html
                    //
                    // When minimum boost requirement will update to 1.74.0,
                    // the following code is updated to move semantics
                    // using multi_index extract functionality.
                    // See
                    // https://github.com/boostorg/multi_index/commit/e69466039d64dd49ecf8fc8a181d9f24d5f82386
                    ep.restore_topic_alias_recv_container(non_act_sess_it->topic_alias_recv.value());
                }

                session_state state;
                non_act_sess_idx.modify(non_act_sess_it,
                                        [&](session_state & val) { state = val; },
                                        [](session_state&) { BOOST_ASSERT(false); });
                state.con = spep;
                non_act_sess_idx.erase(non_act_sess_it);
                BOOST_ASSERT(non_act_sess_idx.end() == non_act_sess_idx.find(client_id));

                auto const& ret = active_sessions_.insert(MQTT_NS::force_move(state));
                BOOST_ASSERT(ret.second);
                act_sess_it = active_sessions_.project<tag_client_id>(ret.first);
                BOOST_ASSERT(act_sess_it->client_id == client_id);
                BOOST_ASSERT(act_sess_it == act_sess_idx.find(client_id));
                BOOST_ASSERT(active_sessions_.project<tag_con>(act_sess_it) == active_sessions_.get<tag_con>().find(spep));
            }
        }
        else {
            // Disconnect the existing connection

            // Force disconnect the client.
            // This shuts down the socket directly.
            act_sess_it->con->force_disconnect();

            // Replace it with the new connection
            // Nothing more should need to be done
            // ...
            // But potentially we need to re-send
            // messages marked as retain?
            // TODO: Verify from standard.
            {
                auto & subs_idx = subs_.get<tag_con>();
                {
                    auto const& range = boost::make_iterator_range(subs_idx.equal_range(act_sess_it->con));
                    for(auto it = range.begin(); it != range.end(); std::advance(it, 1)) {
                        subs_idx.modify_key(it,
                                            [&](con_sp_t & val) { val = spep; },
                                            [](con_sp_t&) { BOOST_ASSERT(false); });
                    }
                }
                BOOST_ASSERT(subs_idx.count(act_sess_it->con) == 0);
            }
            active_sessions_.get<tag_con>().modify_key(active_sessions_.project<tag_con>(act_sess_it),
                                                       [&](con_sp_t & val) { val = spep; },
                                                       [](con_sp_t&) { BOOST_ASSERT(false); });
        }

        if (clean_session) {
            auto & idx = saved_subs_.get<tag_client_id>();
            auto const& range = boost::make_iterator_range(idx.equal_range(client_id));
            // For each to-be-erased saved-subscription object, check to see
            // if there is a shared-subscription that is for a client other than this one.
            // if not, delete the saved_shared_subs_ object for this topic/share.
            // Note: We do not have to check the non-saved shared_subs_ map because
            // if there are any entries in that map, they are already valid
            for(auto const& item : range) {
                if(!item.share.empty()) {
                    auto const& other_range =
                        boost::make_iterator_range(
                            saved_subs_.get<tag_share_topic>().equal_range(
                                std::make_tuple(item.share, item.topic)
                            )
                        );
                    if(std::none_of(
                           other_range.begin(),
                           other_range.end(),
                           [&](auto const& sub) {
                               return sub.client_id != client_id;
                           }
                       )
                    ) {
                        auto & shared_subs_idx = saved_shared_subs_.get<tag_share_topic>();
                        auto const& it = shared_subs_idx.find(std::make_tuple(item.share, item.topic));
                        if(it != shared_subs_idx.end()) {
                            shared_subs_idx.erase(it);
                        }
                    }
                }
            }
            idx.erase(range.begin(), range.end());
            BOOST_ASSERT(saved_subs_.get<tag_client_id>().count(client_id) == 0);
        }
        else {
            // If it's not a clean session, then all of the
            // saved subscriptions are moved to the new session
            // object so they can be used immediately.
            auto & idx = saved_subs_.get<tag_client_id>();
            auto const& range = boost::make_iterator_range(idx.equal_range(client_id));
            for(auto const& item : range) {
                // Send the saved messages out on the wire.
                for (auto & d : item.messages) {
                    // But *only* for this connection
                    // Not every connection in the broker.
                    ep.publish(
                        as::buffer(item.topic),
                        as::buffer(d.contents),
                        // TODO: why is this 'retain'?
                        std::min(item.qos_value, d.qos_value) | MQTT_NS::retain::yes,
                        *(d.props),
                        std::make_tuple(item.topic, d.contents, *(d.props))
                        );
                }
                subs_.emplace(item.share, item.topic, spep, item.qos_value, item.rap_value);
                // Since we are re-activating this session, any shared-subscriptions
                // that are in the saved_shared_subs_ list that this client_id belongs to
                // should be sent to the list of subscribed clients (Which will be only this client)
                // and should then be removed from the saved_shared_subs_ list, since now it's active.
                // Note: Even if there are other deactivated sessions in the saved_subs_ list,
                // because even one session subscribed to the shared subscriotion is active
                // it should not be stored in the saved subs list.
                if(!item.share.empty()) {
                    auto & shared_subs_idx = saved_shared_subs_.get<tag_share_topic>();
                    auto const& it = shared_subs_idx.find(std::make_tuple(item.share, item.topic));
                    if(it != shared_subs_idx.end()) {
                        // Send the saved messages out on the wire.
                        for (auto & d : it->messages) {
                            // But *only* for this connection
                            // Not every connection in the broker.
                            ep.publish(
                                as::buffer(item.topic),
                                as::buffer(d.contents),
                                // TODO: why is this 'retain'?
                                std::min(item.qos_value, d.qos_value) | MQTT_NS::retain::yes,
                                *(d.props),
                                std::make_tuple(item.topic, d.contents, *(d.props))
                                );
                        }
                        shared_subs_idx.erase(it);
                    }

                    // This initializes the round-robin iterator storage for this shared subscription, if and only if, it is not already.
                    // Notably, we do not check the return of the emplace call, since if the item already existed, we don't want a new one made.
                    shared_subs_.emplace(
                        item.share,
                        item.topic,
                        // always found ?  [TBD]
                        subs_.get<tag_share_topic>().find(std::make_tuple(item.share, item.topic)
                        )
                    );
                }
            }
            idx.erase(range.begin(), range.end());
            BOOST_ASSERT(idx.count(client_id) == 0);
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
        close_proc(MQTT_NS::force_move(spep), false);
    }

    bool publish_handler(
        con_sp_t spep,
        MQTT_NS::optional<packet_id_t> packet_id,
        MQTT_NS::publish_options pubopts,
        MQTT_NS::buffer topic_name,
        MQTT_NS::buffer contents,
        MQTT_NS::v5::properties props) {

        MQTT_NS::v5::properties forward_props;

        for (auto&& p : props) {
            MQTT_NS::visit(
                MQTT_NS::make_lambda_visitor(
                    [](MQTT_NS::v5::property::topic_alias&&) {
                        // TopicAlias is not forwarded
                        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901113
                        // A receiver MUST NOT carry forward any Topic Alias mappings from
                        // one Network Connection to another [MQTT-3.3.2-7].
                    },
                    [&forward_props](auto&& p) {
                        forward_props.push_back(MQTT_NS::force_move(p));
                    }
                ),
                MQTT_NS::force_move(p)
            );
        }

        auto& ep = *spep;
        do_publish(
            MQTT_NS::force_move(topic_name),
            MQTT_NS::force_move(contents),
            pubopts.get_qos() | pubopts.get_retain(), // remove dup flag
            MQTT_NS::force_move(forward_props));

        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            switch (pubopts.get_qos()) {
            case MQTT_NS::qos::at_least_once:
                ep.puback(packet_id.value());
                break;
            case MQTT_NS::qos::exactly_once:
                ep.pubrec(packet_id.value());
                break;
            default:
                break;
            }
            break;
        case MQTT_NS::protocol_version::v5:
            switch (pubopts.get_qos()) {
            case MQTT_NS::qos::at_least_once:
                ep.puback(packet_id.value(), MQTT_NS::v5::puback_reason_code::success, puback_props_);
                break;
            case MQTT_NS::qos::exactly_once:
                ep.pubrec(packet_id.value(), MQTT_NS::v5::pubrec_reason_code::success, pubrec_props_);
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

    bool subscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries,
        MQTT_NS::v5::properties props) {

        auto& ep = *spep;

        // An in-order list of qos settings, used to send the reply.
        // The MQTT protocol 3.1.1 - 3.8.4 Response - paragraph 6
        // allows the server to grant a lower QOS than requested
        // So we reply with the QOS setting that was granted
        // not the one requested.
        switch (ep.get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
        {
            std::vector<MQTT_NS::suback_return_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                MQTT_NS::buffer topic = std::get<0>(e);
                MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                res.emplace_back(MQTT_NS::qos_to_suback_return_code(qos_value)); // converts to granted_qos_x
                // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
                // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
                subs_.emplace(MQTT_NS::buffer{}, MQTT_NS::force_move(topic), spep, qos_value);
            }
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, MQTT_NS::force_move(res));
            break;
        }
        case MQTT_NS::protocol_version::v5:
        {
            std::vector<MQTT_NS::v5::suback_reason_code> res;
            res.reserve(entries.size());
            for (auto const& e : entries) {
                MQTT_NS::buffer share;
                MQTT_NS::buffer topic;
                std::tie(share, topic) = parse_shared_subscription(std::get<0>(e));
                MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                MQTT_NS::rap rap_value = std::get<1>(e).get_rap();
                res.emplace_back(MQTT_NS::v5::qos_to_suback_reason_code(qos_value)); // converts to granted_qos_x
                // TODO: This doesn't handle situations where we receive a new subscription for the same topic.
                // MQTT 3.1.1 - 3.8.4 Response - paragraph 3.
                subs_.emplace(MQTT_NS::force_move(share), MQTT_NS::force_move(topic), spep, qos_value, rap_value);
            }
            if (h_subscribe_props_) h_subscribe_props_(props);
            // Acknowledge the subscriptions, and the registered QOS settings
            ep.suback(packet_id, MQTT_NS::force_move(res), suback_props_);
            break;
        }
        default:
            BOOST_ASSERT(false);
            break;
        }

        // Publish any retained messages that match the newly subscribed topic.
        // Note, this code intentionally does not parse the shared subscription
        // since retained messages are not published to shared subscriptions in
        // the normal way, and thus the topic comparison will fail.
        for (auto const& e : entries) {
            for(auto const& retain : retains_) {
                MQTT_NS::buffer const& topic = std::get<0>(e);
                MQTT_NS::subscribe_options options = std::get<1>(e);
                if(compare_topic_filter(topic, retain.topic)) {
                    ep.publish(
                        as::buffer(retain.topic),
                        as::buffer(retain.contents),
                        std::min(retain.qos_value, options.get_qos()) | MQTT_NS::retain::yes,
                        retain.props,
                        std::make_pair(retain.topic, retain.contents)
                    );
                }
            }
        }

        // Re-activate any saved "shared-subscriptions" that match the newly subscribed topics.
        // if any match, publish any saved messages right away.
        for (auto const& e : entries) {
            MQTT_NS::buffer share;
            MQTT_NS::buffer topic;
            std::tie(share, topic) = parse_shared_subscription(std::get<0>(e));
            MQTT_NS::subscribe_options options = std::get<1>(e);

            // If any of the newly added subscriptions matches a shared subscription
            // in the saved_shared_subs_ list, then we need to "re-activate" that
            // shared subscription by removing it from the saved_shared_subs_ list.
            // If there is one in the saved_shared_subs_ list, then this client is now
            // the only active client subscribed to the shared subscription, and should
            // be sent any saved messages that were queued for that subscription.
            if(!share.empty()) {
                auto & shared_subs_idx = saved_shared_subs_.get<tag_share_topic>();
                auto const& it = shared_subs_idx.find(std::make_tuple(share, topic));
                if(it != shared_subs_idx.end()) {
                    // Send the saved messages out on the wire.
                    for (auto & d : it->messages) {
                        // But *only* for this connection
                        // Not every connection in the broker.
                        ep.publish(
                            as::buffer(topic),
                            as::buffer(d.contents),
                            // TODO: why is this 'retain'?
                            std::min(options.get_qos(), d.qos_value) | MQTT_NS::retain::yes,
                            *(d.props),
                            std::make_tuple(topic, d.contents, *(d.props))
                        );
                    }
                    shared_subs_idx.erase(it);
                }
                // This initializes the round-robin iterator storage for this shared subscription, if and only if, it is not already.
                // Notably, we do not check the return of the emplace call, since if the item already existed, we don't want a new one made.
                shared_subs_.emplace(
                    share,
                    topic,
                    subs_.get<tag_share_topic>().find(std::make_tuple(share, topic))
                );
            }
        }
        return true;
    }

    bool unsubscribe_handler(
        con_sp_t spep,
        packet_id_t packet_id,
        std::vector<MQTT_NS::buffer> topics,
        MQTT_NS::v5::properties props) {

        {
            auto & idx = subs_.get<tag_con_share_topic>();
            for(auto const& filter : topics) {
                MQTT_NS::buffer share;
                MQTT_NS::buffer topic;
                std::tie(share, topic) = parse_shared_subscription(filter);
                auto it = idx.find(std::make_tuple(spep, MQTT_NS::force_move(share), MQTT_NS::force_move(topic)));
                if(it != idx.end()) {
                    idx.erase(it);
                }
                // TODO: If this was the last active subscription for this share and topic,
                // then the shared_subs_ entry must be destroyed.

                // TODO: If this was the last active subscription for this share and topic
                // then it is necessary to add a saved_shared_subscription if and only if
                // there is a saved_subscription for this share and topic.

                // TODO: Before erasing, we must updated the shared_subs_ iterator to avoid iterator invalidation.
            }
        }

        switch (spep->get_protocol_version()) {
        case MQTT_NS::protocol_version::v3_1_1:
            spep->unsuback(packet_id);
            break;
        case MQTT_NS::protocol_version::v5:
            if (h_unsubscribe_props_) h_unsubscribe_props_(props);
            spep->unsuback(packet_id, std::vector<MQTT_NS::v5::unsuback_reason_code>(topics.size(), MQTT_NS::v5::unsuback_reason_code::success), unsuback_props_);
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
     * @param topic - The topic to publish the message on.
     * @param contents - The contents of the message.
     * @param qos - The QOS setting to use for the published message.
     * @param is_retain - Whether the message should be retained so it can
     *                    be sent to newly added subscriptions in the future.\
     */
    void do_publish(
        MQTT_NS::buffer topic,
        MQTT_NS::buffer contents,
        MQTT_NS::publish_options pubopts,
        MQTT_NS::v5::properties props) {
        // For each active subscription registered for this topic
        for(auto const& sub : subs_.get<tag_share_topic>()) {
            if(compare_topic_filter(sub.topic, topic)) {
                // If this subscription is shared, we want to publish the message to
                // one and only one subscribed client, which is handled in the next block.
                if(sub.share.empty()) {
                    // publish the message to subscribers.
                    // TODO: Probably this should be switched to async_publish?
                    //       Given the async_client / sync_client seperation
                    //       and the way they have different function names,
                    //       it wouldn't be possible for test_broker.hpp to be
                    //       used with some hypothetical "async_server" in the future.

                    // retain is delivered as the original only if rap_value is rap::retain.
                    // On MQTT v3.1.1, rap_value is always rap::dont.
                    auto retain =
                        [&] {
                            if (sub.rap_value == MQTT_NS::rap::retain) {
                                return pubopts.get_retain();
                            }
                            return MQTT_NS::retain::no;
                        } ();
                    sub.con->publish(
                        topic,
                        contents,
                        std::min(sub.qos_value, pubopts.get_qos()) | retain,
                        props // TODO: Copying the properties vector for each subscription.
                    );
                }
            }
        }

        // publish to one client for any matching shared subscriptions.
        // We have to compare each subscribed topic in the shared_subs_
        // map, because each such subscription, regardless of overlap,
        // can qualify to receive the message independently.
        // additionally, even if multiple entries have identical topics
        // they might be part of unique subscription groups. Each such
        // subscription group receives the message independently of the
        // other groups.
        // So until a Trie based lookup algorithm is introduced,
        // this really does need to be a comparison against every single
        // shared subscription.
        {
            auto& idx = shared_subs_.get<tag_topic>();
            for(auto const& sub : idx) {
                if(compare_topic_filter(sub.topic, topic)) {
                    // retain is delivered as the original only if rap_value is rap::retain.
                    // On MQTT v3.1.1, rap_value is always rap::dont.
                    auto retain =
                        [&] {
                            if (sub.it->rap_value == MQTT_NS::rap::retain) {
                                return pubopts.get_retain();
                            }
                            return MQTT_NS::retain::no;
                        } ();
                    sub.it->con->publish(
                        topic,
                        contents,
                        std::min(sub.it->qos_value, pubopts.get_qos()) | retain,
                        props // TODO: Copying the properties vector for each subscription.
                    );
                    idx.modify(idx.iterator_to(sub),
                               [&](shared_subscription_policy& val)
                               {
                                   std::advance(val.it, 1);
                                   auto & subs_idx = subs_.get<tag_share_topic>();
                                   if(val.it == subs_idx.end()) {
                                       val.it = subs_idx.begin();
                                   }
                               },
                               [](shared_subscription_policy&) { BOOST_ASSERT(false); });
                }
            }
        }

        // For each saved subscription, add this message to
        // the list to be sent out when a connection resumes
        // a lost session.
        {
            auto & idx = saved_subs_.get<tag_share_topic>();
            // Note: Only allocated if used.
            std::shared_ptr<MQTT_NS::v5::properties> sp_props;
            for(auto const& item : idx) {
                if(compare_topic_filter(item.topic, topic)) {
                    if(!sp_props) {
                        sp_props = std::make_shared<MQTT_NS::v5::properties>(props);
                    }

                    idx.modify(idx.iterator_to(item),
                               [&](session_subscription & val)
                               {
                                   // Note: The description of session state here:
                                   // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901230
                                   // does not say that only one message is saved per topic in the state.
                                   // That behavior is apparently only applicable to the 'retain' message
                                   // storage, which is a global thing, and not a per-session thing.
                                   // So it is correct that all messages should be stored in the session state.
                                   val.messages.emplace_back(
                                       contents,
                                       sp_props,
                                       std::min(val.qos_value, pubopts.get_qos()));
                               },
                               [](session_subscription&) { BOOST_ASSERT(false); });
                }
            }
        }

        // Store in any matching saved_shared_subscriptions
        {
            auto & idx = saved_shared_subs_.get<tag_topic>();
            // Note: Only allocated if used.
            std::shared_ptr<MQTT_NS::v5::properties> sp_props;
            for(auto const& item : idx) {
                if(compare_topic_filter(item.topic, topic)) {
                    if(!sp_props) {
                        sp_props = std::make_shared<MQTT_NS::v5::properties>(props);
                    }

                    idx.modify(idx.iterator_to(item),
                               [&](shared_session_subscription & val)
                               {
                                   // Note: The description of session state here:
                                   // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901230
                                   // does not say that only one message is saved per topic in the state.
                                   // That behavior is apparently only applicable to the 'retain' message
                                   // storage, which is a global thing, and not a per-session thing.
                                   // So it is correct that all messages should be stored in the session state.
                                   val.messages.emplace_back(
                                       contents,
                                       sp_props,
                                       pubopts.get_qos());
                               },
                               [](shared_session_subscription&) { BOOST_ASSERT(false); });
                }
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
                BOOST_ASSERT(retains_.count(topic) == 0);
            }
            else {
                auto const& it = retains_.find(topic);
                if(it == retains_.end()) {
                    auto const& ret = retains_.emplace(MQTT_NS::force_move(topic),
                                                       MQTT_NS::force_move(contents),
                                                       MQTT_NS::force_move(props),
                                                       pubopts.get_qos());
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                }
                else {
                    retains_.modify(it,
                                    [&](retain& val)
                                    {
                                        val.qos_value = pubopts.get_qos();
                                        val.props = MQTT_NS::force_move(props);
                                        val.contents = MQTT_NS::force_move(contents);
                                    },
                                    [](retain&) { BOOST_ASSERT(false); });
                }
            }
        }
    }

    /**
     * @brief close_proc - clean up a connection that has been closed.
     *
     * @param ep - The underlying MQTT_NS::server (of whichever type) that is disconnecting.
     * @param send_will - Whether to publish this connections last will
     */
    // TODO: Maybe change the name of this function.
    void close_proc(con_sp_t spep, bool send_will) {
        endpoint_t& ep = *spep;

        auto & act_sess_idx = active_sessions_.get<tag_con>();
        auto act_sess_it = act_sess_idx.find(spep);

        // act_sess_it == act_sess_idx.end() could happen if broker accepts
        // the session from client but the client closes the session  before sending
        // MQTT `CONNECT` message.
        // In this case, do nothing is correct behavior.
        if (act_sess_it == act_sess_idx.end()) return;

        MQTT_NS::buffer client_id;
        MQTT_NS::optional<MQTT_NS::will> will;
        bool session_clear =
            [&] {
                if (ep.get_protocol_version() == MQTT_NS::protocol_version::v3_1_1) {
                    return ep.clean_session();
                }
                else {
                    BOOST_ASSERT(ep.get_protocol_version() == MQTT_NS::protocol_version::v5);
                    auto const& sei_opt = act_sess_it->session_expiry_interval;
                    return !sei_opt || sei_opt.value() == std::chrono::steady_clock::duration::zero();
                }
            } ();
        if (session_clear) {
            client_id = std::move(act_sess_it->client_id);
            will = std::move(act_sess_it->will);

            act_sess_idx.erase(act_sess_it);

            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(client_id) == non_active_sessions_.get<tag_client_id>().end());
        }
        else {
            session_state state = std::move(*act_sess_it);
            client_id = state.client_id;
            will = std::move(state.will);

            // TODO: Should yank out the messages from this connection object and store it in the session_state object??
            state.con.reset(); // clear the shared pointer, so it doesn't stay alive after this funciton ends.

            act_sess_idx.erase(act_sess_it);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_client_id>().find(client_id) == active_sessions_.get<tag_client_id>().end());

            BOOST_ASSERT(active_sessions_.get<tag_con>().count(spep) == 0);
            BOOST_ASSERT(active_sessions_.get<tag_con>().find(spep) == active_sessions_.get<tag_con>().end());

            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 0);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().find(client_id) == non_active_sessions_.get<tag_client_id>().end());

            auto const& sei_opt = state.session_expiry_interval;
            if (sei_opt && sei_opt.value() !=
                std::chrono::seconds(MQTT_NS::session_never_expire)) {
                state.tim_session_expiry = std::make_shared<as::steady_timer>(ioc_);
                state.tim_session_expiry->expires_after(sei_opt.value());
                state.tim_session_expiry->async_wait(
                    [&, wp = std::weak_ptr<as::steady_timer>(state.tim_session_expiry)]
                    (MQTT_NS::error_code ec) {
                        auto sp = wp.lock();
                        if (!ec) {
                            auto& idx = non_active_sessions_.get<tag_tim>();
                            idx.erase(sp);
                        }
                    }
                );
            }

            // TopicAlias lifetime is the same as Session lifetime
            // It is different from MQTT v5 spec but practical choice.
            // See
            // https://lists.oasis-open.org/archives/mqtt-comment/202009/msg00000.html
            state.topic_alias_recv = ep.get_topic_alias_recv_container();

            auto const& ret = non_active_sessions_.insert(MQTT_NS::force_move(state));
            (void)ret;
            BOOST_ASSERT(ret.second);
            BOOST_ASSERT(non_active_sessions_.get<tag_client_id>().count(client_id) == 1);
            BOOST_ASSERT(ret.first->client_id == client_id);
            BOOST_ASSERT(ret.first == non_active_sessions_.get<tag_client_id>().find(client_id));
        }

        // Before sending the will, either erase, or save, any subscriptions that
        // match this connection. It's possible that none match, and that's fine.
        {
            auto& idx = subs_.get<tag_con>();
            auto const& range = boost::make_iterator_range(idx.equal_range(spep));
            // In v3_1_1, session_expiry_interval is not set. So clean on close.
            if (!session_clear) {
                // Save all the subscriptions for this clientid for later.
                for(auto const& item : range) {
                    auto const& ret = saved_subs_.emplace(client_id,
                                                          item.share,
                                                          item.topic,
                                                          item.qos_value,
                                                          item.rap_value);
                    (void)ret;
                    BOOST_ASSERT(ret.second);
                    BOOST_ASSERT(ret.first == saved_subs_.find(client_id));
                    if(!item.share.empty()) {
                        // If no other clients are subscribed to this shared subscription topic, then we
                        // make a new saved_shared_subs_ entry to record that a disconnected session is
                        // subscribed to a shared subscription.
                        auto const& other_range =
                            boost::make_iterator_range(
                                subs_.get<tag_share_topic>().equal_range(
                                    std::make_tuple(item.share, item.topic)
                                )
                            );
                        if(std::none_of(other_range.begin(), other_range.end(), [&](auto const& sub) {
                            return sub.con != spep;
                        })) {
                            auto const& ret = saved_shared_subs_.emplace(item.share, item.topic);
                            (void)ret;
                            BOOST_ASSERT(ret.second);
                            //BOOST_ASSERT(ret.first == saved_shared_subs_.get<tag_share_topic>().find(std::make_tuple(item.share, item.topic)));
                        }
                    }
                }
            }

            // Regardless of whether we are saving any subscriptions, we need to adjust the
            // entries in the active shared_subs_ map now that this client id is disconnected.
            for(auto const& item : range) {
                if(!item.share.empty()) {
                    // If no other clients are subscribed to this shared subscription topic, then we
                    // remove the shared_subs_ entry. Otherwise we update the iterator in the shared_subs_ entry.
                    auto const& other_range =
                        boost::make_iterator_range(
                            subs_.get<tag_share_topic>().equal_range(
                                std::make_tuple(item.share, item.topic)
                            )
                        );
                    if(std::none_of(other_range.begin(), other_range.end(), [&](auto const& sub) {
                        return sub.con != spep;
                    })) {
                        auto & shared_subs_idx = shared_subs_.get<tag_share_topic>();
                        auto const& it = shared_subs_idx.find(std::make_tuple(item.share, item.topic));
                        if(it != shared_subs_idx.end()) {
                            shared_subs_idx.erase(it);
                        }
                    }
                    else {
                        auto & shared_subs_idx = shared_subs_.get<tag_share_topic>();
                        auto const& it = shared_subs_idx.find(std::make_tuple(item.share, item.topic));
                        shared_subs_idx.modify(it,
                                             [&](shared_subscription_policy& val)
                                             {
                                                 auto & subs_idx = subs_.get<tag_share_topic>();
                                                 // Only make any modifications if the iterator
                                                 // currently points to the client being disconnected.
                                                 while(val.it->con == spep) {
                                                     std::advance(val.it, 1);
                                                     if(val.it == subs_idx.end()) {
                                                     val.it = subs_idx.begin();
                                                 }
                                             }
                                         },
                                         [](shared_subscription_policy&) { BOOST_ASSERT(false); });
                    }
                }
            }

            // Remove all subscriptions for this clientid from the active subscriptions.
            idx.erase(range.begin(), range.end());
            BOOST_ASSERT(idx.count(spep) == 0);
        }

        if(send_will && will) {
            // TODO: This should be triggered by the will delay
            // Not sent immediately.
            do_publish(
                MQTT_NS::force_move(will.value().topic()),
                MQTT_NS::force_move(will.value().message()),
                will.value().get_qos() | will.value().get_retain(),
                MQTT_NS::force_move(will.value().props()));
        }
    }

private:
    struct tag_con {};
    struct tag_tim {};
    struct tag_client_id {};
    struct tag_topic {};
    struct tag_share_topic {};
    struct tag_con_share_topic {};
    struct tag_cid_share_topic {};

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
            con_sp_t con,
            MQTT_NS::buffer client_id,
            MQTT_NS::optional<MQTT_NS::will> will,
            MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval = MQTT_NS::nullopt)
            :con(MQTT_NS::force_move(con)),
             client_id(MQTT_NS::force_move(client_id)),
             will(MQTT_NS::force_move(will)),
             session_expiry_interval(MQTT_NS::force_move(session_expiry_interval))
        {}

        session_state() = default;
        session_state(session_state &&) = default;
        session_state(session_state const&) = default;
        session_state& operator=(session_state &&) = default;
        session_state& operator=(session_state const&) = default;

        con_sp_t con;
        MQTT_NS::buffer client_id;

        // TODO:
        // Messages sent to client, but not acknowledged.
        // Messages pending transmission to client
        // messages received from client, but not acknowledged
        MQTT_NS::optional<MQTT_NS::will> will;
        MQTT_NS::optional<std::chrono::steady_clock::duration> will_delay;
        MQTT_NS::optional<std::chrono::steady_clock::duration> session_expiry_interval;
        std::shared_ptr<as::steady_timer> tim_session_expiry;
        MQTT_NS::optional<MQTT_NS::topic_alias_recv_map_t> topic_alias_recv;
    };

    // The mi_active_sessions container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_active_sessions = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(session_state, con_sp_t, con)
            >,
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(session_state, MQTT_NS::buffer, client_id)
            >
        >
    >;

    // The mi_active_sessions container holds the relevant data about an active connection with the broker.
    // It can be queried either with the clientid, or with the shared pointer to the mqtt endpoint object
    using mi_non_active_sessions = mi::multi_index_container<
        session_state,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(session_state, MQTT_NS::buffer, client_id)
            >,
            mi::ordered_unique<
                mi::tag<tag_tim>,
                BOOST_MULTI_INDEX_MEMBER(session_state, std::shared_ptr<as::steady_timer>, tim_session_expiry)
            >
        >
    >;

    // Mapping between connection object and subscription topics
    struct sub_con {
        sub_con(
            MQTT_NS::buffer share,
            MQTT_NS::buffer topic,
            con_sp_t con,
            MQTT_NS::qos qos_value,
            MQTT_NS::rap rap_value = MQTT_NS::rap::dont)
            :share(MQTT_NS::force_move(share)), topic(MQTT_NS::force_move(topic)), con(MQTT_NS::force_move(con)), qos_value(qos_value), rap_value(rap_value) {}
        MQTT_NS::buffer share;
        MQTT_NS::buffer topic;
        con_sp_t con;
        MQTT_NS::qos qos_value;
        MQTT_NS::rap rap_value;
    };
    using mi_sub_con = mi::multi_index_container<
        sub_con,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_share_topic>,
                mi::composite_key<
                    sub_con,
                    BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
                >
            >,
            // Don't allow the same connection object to have the same topic multiple times.
            mi::ordered_unique<
                mi::tag<tag_con_share_topic>,
                mi::composite_key<
                    sub_con,
                    BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con),
                    BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)
                >
            >
        >
    >;

    // A collection of messages that have been retained in
    // case clients add a new subscription to the associated topics.
    struct retain {
        retain(
            MQTT_NS::buffer topic,
            MQTT_NS::buffer contents,
            MQTT_NS::v5::properties props,
            MQTT_NS::qos qos_value)
            :topic(MQTT_NS::force_move(topic)),
             contents(MQTT_NS::force_move(contents)),
             props(MQTT_NS::force_move(props)),
             qos_value(qos_value)
        { }
        MQTT_NS::buffer topic;
        MQTT_NS::buffer contents;
        MQTT_NS::v5::properties props;
        MQTT_NS::qos qos_value;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(retain, MQTT_NS::buffer, topic)
            >
        >
    >;

    // The saved_message structure holds messages that have been published on a
    // topic that a not-currently-connected client is subscribed to.
    // When a new connection is made with the client id for this saved data,
    // these messages will be published to that client, and only that client.
    struct saved_message {
        saved_message(
            MQTT_NS::buffer contents,
            std::shared_ptr<MQTT_NS::v5::properties> props,
            MQTT_NS::qos qos_value)
            : contents(MQTT_NS::force_move(contents)), props(MQTT_NS::force_move(props)), qos_value(qos_value) {}
        MQTT_NS::buffer contents;
        std::shared_ptr<MQTT_NS::v5::properties> props;
        MQTT_NS::qos qos_value;
    };

    // Each instance of session_subscription describes a subscription that the associated client id has made
    // and a collection of data associated with that subscription to be sent when the client reconnects.
    struct session_subscription {
        session_subscription(
            MQTT_NS::buffer client_id,
            MQTT_NS::buffer share,
            MQTT_NS::buffer topic,
            MQTT_NS::qos qos_value,
            MQTT_NS::rap rap_value)
            :client_id(MQTT_NS::force_move(client_id)),
             share(MQTT_NS::force_move(share)),
             topic(MQTT_NS::force_move(topic)),
             qos_value(qos_value),
             rap_value(rap_value) {}

        MQTT_NS::buffer client_id;
        MQTT_NS::buffer share;
        MQTT_NS::buffer topic;
        std::vector<saved_message> messages;
        MQTT_NS::qos qos_value;
        MQTT_NS::rap rap_value;
    };

    using mi_session_subscription = mi::multi_index_container<
        session_subscription,
        mi::indexed_by<
            // Allow multiple client id's for the same topic
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, client_id)
            >,
            // Allow multiple topics for the same client id
            mi::ordered_non_unique<
                mi::tag<tag_share_topic>,
                mi::composite_key<
                    session_subscription,
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, topic)
                >
            >,
            // Don't allow the same client id to have the same topic multiple times.
            // Note that this index does not get used by any code in the broker
            // other than to enforce the uniqueness constraints.
            // Potentially this can be enabled only in debug builds.
            mi::ordered_unique<
                mi::tag<tag_cid_share_topic>,
                mi::composite_key<
                    session_subscription,
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, client_id),
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(session_subscription, MQTT_NS::buffer, topic)
                >
            >
        >
    >;

    // Each instance of session_subscription describes a subscription that the associated client id has made
    // and a collection of data associated with that subscription to be sent when the client reconnects.
    struct shared_session_subscription {
        shared_session_subscription(
            MQTT_NS::buffer share,
            MQTT_NS::buffer topic)
            :share(MQTT_NS::force_move(share)), topic(MQTT_NS::force_move(topic)) {}
        MQTT_NS::buffer share;
        MQTT_NS::buffer topic;
        std::vector<saved_message> messages;
    };

    using mi_shared_session_subscription = mi::multi_index_container<
        shared_session_subscription,
        mi::indexed_by<
            // Allow multiple topics for the same client id
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(shared_session_subscription, MQTT_NS::buffer, topic)
            >,
            mi::ordered_unique<
                mi::tag<tag_share_topic>,
                mi::composite_key<
                    shared_session_subscription,
                    BOOST_MULTI_INDEX_MEMBER(shared_session_subscription, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(shared_session_subscription, MQTT_NS::buffer, topic)
                >
            >
        >
    >;

    // This stores the messages saved for disconnected sessions with shared subscription topics.
    struct shared_subscription_policy {
        shared_subscription_policy(
            MQTT_NS::buffer share,
            MQTT_NS::buffer topic,
            mi_sub_con::index<tag_share_topic>::type::iterator it)
            :share(MQTT_NS::force_move(share)), topic(MQTT_NS::force_move(topic)), it(MQTT_NS::force_move(it)) { }
        MQTT_NS::buffer share;
        MQTT_NS::buffer topic;
        mi_sub_con::index<tag_share_topic>::type::iterator it;
    };

    using mi_shared_subscription_policy = mi::multi_index_container<
        shared_subscription_policy,
        mi::indexed_by<
            // Allow multiple topics for the same client id
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(shared_subscription_policy, MQTT_NS::buffer, topic)
            >,
            mi::ordered_unique<
                mi::tag<tag_share_topic>,
                mi::composite_key<
                    shared_subscription_policy,
                    BOOST_MULTI_INDEX_MEMBER(shared_subscription_policy, MQTT_NS::buffer, share),
                    BOOST_MULTI_INDEX_MEMBER(shared_subscription_policy, MQTT_NS::buffer, topic)
                >
            >
        >
    >;

    as::io_context& ioc_; ///< The boost asio context to run this broker on.
    as::steady_timer tim_disconnect_; ///< Used to delay disconnect handling for testing
    MQTT_NS::optional<std::chrono::steady_clock::duration> delay_disconnect_; ///< Used to delay disconnect handling for testing

    mi_active_sessions active_sessions_; ///< Map of active client id and connections
    mi_non_active_sessions non_active_sessions_; ///< Storage for sessions not currently active. Indexed by client id.
    mi_sub_con subs_; ///< Map of topic subscriptions to client ids
    mi_session_subscription saved_subs_; ///< Topics and associated messages for clientids that are currently disconnected
    mi_shared_session_subscription saved_shared_subs_; ///< Topics and associated messages for shared subscriptions that are currently disconnected
    mi_retain retains_; ///< A list of messages retained so they can be sent to newly subscribed clients.
    mi_shared_subscription_policy shared_subs_; ///< Storage for the next client-id to send to.

    // MQTTv5 members
    MQTT_NS::v5::properties connack_props_;
    MQTT_NS::v5::properties suback_props_;
    MQTT_NS::v5::properties unsuback_props_;
    MQTT_NS::v5::properties puback_props_;
    MQTT_NS::v5::properties pubrec_props_;
    MQTT_NS::v5::properties pubrel_props_;
    MQTT_NS::v5::properties pubcomp_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_connect_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_disconnect_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_publish_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_puback_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubrec_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubrel_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_pubcomp_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_subscribe_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_unsubscribe_props_;
    std::function<void(MQTT_NS::v5::properties const&)> h_auth_props_;
    bool pingresp_ = true;
};

#endif // MQTT_TEST_BROKER_HPP

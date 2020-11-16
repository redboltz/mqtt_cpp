// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MESSAGE_HPP)
#define MQTT_MESSAGE_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>

#include <boost/asio/buffer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/two_byte_util.hpp>
#include <mqtt/fixed_header.hpp>
#include <mqtt/remaining_length.hpp>
#include <mqtt/subscribe_options.hpp>
#include <mqtt/const_buffer_util.hpp>
#include <mqtt/will.hpp>
#include <mqtt/connect_flags.hpp>
#include <mqtt/publish.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/four_byte_util.hpp>
#include <mqtt/packet_id_type.hpp>
#include <mqtt/optional.hpp>
#include <mqtt/string_view.hpp>
#include <mqtt/property.hpp>
#include <mqtt/string_check.hpp>
#include <mqtt/move.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/connect_return_code.hpp>
#include <mqtt/publish.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

inline namespace v3_1_1 {

namespace detail_v3_1_1 {

class header_only_message {
public:
    /**
     * @brief Create empty header_packet_id_message.
     */
    header_only_message(control_packet_type type, std::uint8_t flags)
        : message_ { static_cast<char>(make_fixed_header(type, flags)), 0 }
    {}

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        return { as::buffer(message_.data(), message_.size()) };
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return message_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return 1;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        return std::string(message_.data(), size());
    }
private:
    boost::container::static_vector<char, 2> message_;
};


template <std::size_t PacketIdBytes>
class basic_header_packet_id_message;



template <std::size_t PacketIdBytes>
class basic_header_packet_id_message {
public:
    /**
     * @brief Create empty header_packet_id_message.
     */
    basic_header_packet_id_message(control_packet_type type, std::uint8_t flags, typename packet_id_type<PacketIdBytes>::type packet_id)
        : message_ { static_cast<char>(make_fixed_header(type, flags)), PacketIdBytes }
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(message_, packet_id);
    }

    template <typename Iterator>
    basic_header_packet_id_message(Iterator b, Iterator e) {
        if (std::distance(b, e) != 2 + PacketIdBytes) throw remaining_length_error();
        if (b[1] != PacketIdBytes) throw remaining_length_error();

        std::copy(b, e, std::back_inserter(message_));
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        return { as::buffer(message_.data(), size()) };
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return message_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return 1;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        return std::string(message_.data(), size());
    }
protected:
    boost::container::static_vector<char, 2 + PacketIdBytes> const& message() const {
        return message_;
    }

private:
    boost::container::static_vector<char, 2 + PacketIdBytes> message_;
};

} // namespace detail_v3_1_1

template <std::size_t PacketIdBytes>
struct basic_puback_message : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes> {
    using base = detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes>;
    basic_puback_message(typename packet_id_type<PacketIdBytes>::type packet_id)
        : base(control_packet_type::puback, 0b0000, packet_id)
    {}
};

using puback_message = basic_puback_message<2>;

template <std::size_t PacketIdBytes>
struct basic_pubrec_message : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes> {
    using base = detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes>;
    basic_pubrec_message(typename packet_id_type<PacketIdBytes>::type packet_id)
        : base(control_packet_type::pubrec, 0b0000, packet_id)
    {}
};

using pubrec_message = basic_pubrec_message<2>;

template <std::size_t PacketIdBytes>
struct basic_pubrel_message : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes> {
    using base = detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes>;
    basic_pubrel_message(typename packet_id_type<PacketIdBytes>::type packet_id)
        : base(control_packet_type::pubrel, 0b0010, packet_id)
    {
    }

    basic_pubrel_message(string_view buf)
        : base(buf.begin(), buf.end())
    {
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    typename packet_id_type<PacketIdBytes>::type packet_id() const {
        return make_packet_id<PacketIdBytes>::apply(std::next(base::message().begin(), 2), base::message().end());
    }
};

using pubrel_message = basic_pubrel_message<2>;
using pubrel_32_message = basic_pubrel_message<4>;

template <std::size_t PacketIdBytes>
struct basic_pubcomp_message : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes> {
    basic_pubcomp_message(typename packet_id_type<PacketIdBytes>::type packet_id)
        : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes>(control_packet_type::pubcomp, 0b0000, packet_id)
    {}
};

using pubcomp_message = basic_pubcomp_message<2>;

template <std::size_t PacketIdBytes>
struct basic_unsuback_message : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes> {
    basic_unsuback_message(typename packet_id_type<PacketIdBytes>::type packet_id)
        : detail_v3_1_1::basic_header_packet_id_message<PacketIdBytes>(control_packet_type::unsuback, 0b0000, packet_id)
    {}
};

using unsuback_message = basic_unsuback_message<2>;

struct pingreq_message : detail_v3_1_1::header_only_message {
    pingreq_message()
        : detail_v3_1_1::header_only_message(control_packet_type::pingreq, 0b0000)
    {}
};

struct pingresp_message : detail_v3_1_1::header_only_message {
    pingresp_message()
        : detail_v3_1_1::header_only_message(control_packet_type::pingresp, 0b0000)
    {}
};

struct disconnect_message : detail_v3_1_1::header_only_message {
    disconnect_message()
        : detail_v3_1_1::header_only_message(control_packet_type::disconnect, 0b0000)
    {}
};

class connack_message {
public:
    connack_message(bool session_present, connect_return_code return_code)
        : message_ {
              static_cast<char>(make_fixed_header(control_packet_type::connack, 0b0000)),
              0b0010,
              static_cast<char>(session_present ? 1 : 0),
              static_cast<char>(return_code)
          }
    {}

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        return { as::buffer(message_.data(), size()) };
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return message_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return 1;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        return std::string(message_.data(), size());
    }

private:
    boost::container::static_vector<char, 4> message_;
};

// variable length messages

class connect_message {
public:
    connect_message(
        std::uint16_t keep_alive_sec,
        buffer client_id,
        bool clean_session,
        optional<will> w,
        optional<buffer> user_name,
        optional<buffer> password
    )
        : fixed_header_(static_cast<char>(make_fixed_header(control_packet_type::connect, 0b0000))),
          connect_flags_(0),
          // protocol name length, protocol name, protocol level, connect flag, client id length, client id, keep alive
          remaining_length_(
              2 +                     // protocol name length
              4 +                     // protocol name
              1 +                     // protocol level
              1 +                     // connect flag
              2 +                     // keep alive
              2 +                     // client id length
              client_id.size()        // client id
          ),
          protocol_name_and_level_ { 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04 },
          client_id_(force_move(client_id)),
          client_id_length_buf_{ num_to_2bytes(boost::numeric_cast<std::uint16_t>(client_id_.size())) },
          keep_alive_buf_ { num_to_2bytes(keep_alive_sec) }
    {
        utf8string_check(client_id_);
        if (clean_session) connect_flags_ |= connect_flags::clean_session;
        if (user_name) {
            utf8string_check(user_name.value());
            connect_flags_ |= connect_flags::user_name_flag;
            user_name_ = force_move(user_name.value());
            add_uint16_t_to_buf(user_name_length_buf_, boost::numeric_cast<std::uint16_t>(user_name_.size()));

            remaining_length_ += 2 + user_name_.size();
        }
        if (password) {
            connect_flags_ |= connect_flags::password_flag;
            password_ = force_move(password.value());
            add_uint16_t_to_buf(password_length_buf_, boost::numeric_cast<std::uint16_t>(password_.size()));

            remaining_length_ += 2 + password_.size();
        }
        if (w) {
            connect_flags_ |= connect_flags::will_flag;
            if (w.value().get_retain() == retain::yes) connect_flags_ |= connect_flags::will_retain;
            connect_flags::set_will_qos(connect_flags_, w.value().get_qos());

            utf8string_check(w.value().topic());
            will_topic_name_ = force_move(w.value().topic());
            add_uint16_t_to_buf(
                will_topic_name_length_buf_,
                boost::numeric_cast<std::uint16_t>(will_topic_name_.size())
            );
            if (w.value().message().size() > 0xffffL) throw will_message_length_error();
            will_message_ = force_move(w.value().message());
            add_uint16_t_to_buf(
                will_message_length_buf_,
                boost::numeric_cast<std::uint16_t>(will_message_.size()));

            remaining_length_ += 2 + will_topic_name_.size() + 2 + will_message_.size();
        }

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());

        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));
        ret.emplace_back(as::buffer(protocol_name_and_level_.data(), protocol_name_and_level_.size()));
        ret.emplace_back(as::buffer(&connect_flags_, 1));
        ret.emplace_back(as::buffer(keep_alive_buf_.data(), keep_alive_buf_.size()));

        ret.emplace_back(as::buffer(client_id_length_buf_.data(), client_id_length_buf_.size()));
        ret.emplace_back(as::buffer(client_id_));

        if (connect_flags::has_will_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(will_topic_name_length_buf_.data(), will_topic_name_length_buf_.size()));
            ret.emplace_back(as::buffer(will_topic_name_));
            ret.emplace_back(as::buffer(will_message_length_buf_.data(), will_message_length_buf_.size()));
            ret.emplace_back(as::buffer(will_message_));
        }

        if (connect_flags::has_user_name_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(user_name_length_buf_.data(), user_name_length_buf_.size()));
            ret.emplace_back(as::buffer(user_name_));
        }

        if (connect_flags::has_password_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(password_length_buf_.data(), password_length_buf_.size()));
            ret.emplace_back(as::buffer(password_));
        }

        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // protocol name and level
            1 +                   // connect flags
            1 +                   // keep alive

            2 +                   // client id length, client id

            2 +                   // will topic name length, will topic name
            2 +                   // will message length, will message
            2 +                   // user name length, user name
            2;                    // password length, password
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;

        ret.reserve(size());

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());
        ret.append(protocol_name_and_level_.data(), protocol_name_and_level_.size());
        ret.push_back(connect_flags_);
        ret.append(keep_alive_buf_.data(), keep_alive_buf_.size());

        ret.append(client_id_length_buf_.data(), client_id_length_buf_.size());
        ret.append(client_id_.data(), client_id_.size());

        if (connect_flags::has_will_flag(connect_flags_)) {
            ret.append(will_topic_name_length_buf_.data(), will_topic_name_length_buf_.size());
            ret.append(will_topic_name_.data(), will_topic_name_.size());
            ret.append(will_message_length_buf_.data(), will_message_length_buf_.size());
            ret.append(will_message_.data(), will_message_.size());
        }

        if (connect_flags::has_user_name_flag(connect_flags_)) {
            ret.append(user_name_length_buf_.data(), user_name_length_buf_.size());
            ret.append(user_name_.data(), user_name_.size());
        }

        if (connect_flags::has_password_flag(connect_flags_)) {
            ret.append(password_length_buf_.data(), password_length_buf_.size());
            ret.append(password_.data(), password_.size());
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    char connect_flags_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    boost::container::static_vector<char, 7> protocol_name_and_level_;
    buffer client_id_;
    boost::container::static_vector<char, 2> client_id_length_buf_;

    buffer will_topic_name_;
    boost::container::static_vector<char, 2> will_topic_name_length_buf_;
    buffer will_message_;
    boost::container::static_vector<char, 2> will_message_length_buf_;

    buffer user_name_;
    boost::container::static_vector<char, 2> user_name_length_buf_;
    buffer password_;
    boost::container::static_vector<char, 2> password_length_buf_;

    boost::container::static_vector<char, 2> keep_alive_buf_;
};

template <std::size_t PacketIdBytes>
class basic_publish_message {
public:
    template <
        typename ConstBufferSequence,
        typename std::enable_if<
            as::is_const_buffer_sequence<ConstBufferSequence>::value,
            std::nullptr_t
        >::type = nullptr
    >
    basic_publish_message(
        typename packet_id_type<PacketIdBytes>::type packet_id,
        as::const_buffer topic_name,
        ConstBufferSequence payloads,
        publish_options pubopts
    )
        : fixed_header_(make_fixed_header(control_packet_type::publish, 0b0000) | pubopts.operator std::uint8_t()),
          topic_name_(topic_name),
          topic_name_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_name.size())) },
          remaining_length_(
              2                      // topic name length
              + topic_name_.size()   // topic name
              + (  (pubopts.get_qos() == qos::at_least_once || pubopts.get_qos() == qos::exactly_once)
                 ? PacketIdBytes // packet_id
                 : 0)
          )
    {
        auto b = as::buffer_sequence_begin(payloads);
        auto e = as::buffer_sequence_end(payloads);
        auto num_of_payloads = static_cast<std::size_t>(std::distance(b, e));
        payloads_.reserve(num_of_payloads);
        for (; b != e; ++b) {
            auto const& payload = *b;
            remaining_length_ += payload.size();
            payloads_.push_back(payload);
        }

        utf8string_check(topic_name_);

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        if (pubopts.get_qos() == qos::at_least_once ||
            pubopts.get_qos() == qos::exactly_once) {
            packet_id_.reserve(PacketIdBytes);
            add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        }
    }

    // Used in test code, and to deserialize stored messages.
    basic_publish_message(buffer buf) {
        if (buf.empty())  throw remaining_length_error();
        fixed_header_ = static_cast<std::uint8_t>(buf.front());
        qos qos_value = get_qos();
        buf.remove_prefix(1);

        if (buf.empty()) throw remaining_length_error();
        auto len_consumed = remaining_length(buf.begin(), buf.end());
        remaining_length_ = std::get<0>(len_consumed);
        auto consumed = std::get<1>(len_consumed);

        std::copy(
            buf.begin(),
            std::next(buf.begin(), static_cast<string_view::difference_type>(consumed)),
            std::back_inserter(remaining_length_buf_));
        buf.remove_prefix(consumed);

        if (buf.size() < 2) throw remaining_length_error();
        std::copy(buf.begin(), std::next(buf.begin(), 2), std::back_inserter(topic_name_length_buf_));
        auto topic_name_length = make_uint16_t(topic_name_length_buf_.begin(), topic_name_length_buf_.end());
        buf.remove_prefix(2);

        if (buf.size() < topic_name_length) throw remaining_length_error();

        topic_name_ = as::buffer(buf.substr(0, topic_name_length));
        utf8string_check(topic_name_);
        buf.remove_prefix(topic_name_length);

        switch (qos_value) {
        case qos::at_most_once:
            break;
        case qos::at_least_once:
        case qos::exactly_once:
            if (buf.size() < PacketIdBytes) throw remaining_length_error();
            std::copy(buf.begin(), std::next(buf.begin(), PacketIdBytes), std::back_inserter(packet_id_));
            buf.remove_prefix(PacketIdBytes);
            break;
        default:
            throw protocol_error();
            break;
        };

        if (!buf.empty()) {
            payloads_.emplace_back(as::buffer(buf));
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());
        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));
        ret.emplace_back(as::buffer(topic_name_length_buf_.data(), topic_name_length_buf_.size()));
        ret.emplace_back(as::buffer(topic_name_));
        if (!packet_id_.empty()) {
            ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));
        }
        std::copy(payloads_.begin(), payloads_.end(), std::back_inserter(ret));
        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 +                   // fixed header
            1 +                   // remaining length
            2 +                   // topic name length, topic name
            (packet_id_.empty() ? 0 : 1) +  // packet_id
            payloads_.size();
    }

    /**
     * @brief Create one continuous buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;

        ret.reserve(size());

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(topic_name_length_buf_.data(), topic_name_length_buf_.size());
        ret.append(get_pointer(topic_name_), get_size(topic_name_));

        ret.append(packet_id_.data(), packet_id_.size());
        for (auto const& payload : payloads_) {
            ret.append(get_pointer(payload), get_size(payload));
        }

        return ret;
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    typename packet_id_type<PacketIdBytes>::type packet_id() const {
        return make_packet_id<PacketIdBytes>::apply(packet_id_.begin(), packet_id_.end());
    }

    /**
     * @brief Get publish_options
     * @return publish_options.
     */
    constexpr publish_options get_options() const {
        return publish_options(fixed_header_);
    }

    /**
     * @brief Get qos
     * @return qos
     */
    constexpr qos get_qos() const {
        return publish::get_qos(fixed_header_);
    }

    /**
     * @brief Check retain flag
     * @return true if retain, otherwise return false.
     */
    constexpr bool is_retain() const {
        return publish::is_retain(fixed_header_);
    }

    /**
     * @brief Check dup flag
     * @return true if dup, otherwise return false.
     */
    constexpr bool is_dup() const {
        return publish::is_dup(fixed_header_);
    }

    /**
     * @brief Get topic name
     * @return topic name
     */
    constexpr string_view topic() const {
        return string_view(get_pointer(topic_name_), get_size(topic_name_));
    }

    /**
     * @brief Get payload
     * @return payload
     */
    std::vector<string_view> payload() const {
        std::vector<string_view> ret;
        ret.reserve(payloads_.size());
        for (auto const& payload : payloads_) {
            ret.emplace_back(get_pointer(payload), get_size(payload));
        }
        return ret;
    }

    /**
     * @brief Get payload as single buffer
     * @return payload
     */
    buffer payload_as_buffer() const {
        auto size = std::accumulate(
            payloads_.begin(),
            payloads_.end(),
            std::size_t(0),
            [](std::size_t s, as::const_buffer const& payload) {
                return s += payload.size();
            }
        );

        if (size == 0) return buffer();

        auto spa = make_shared_ptr_array(size);
        auto ptr = spa.get();
        auto it = ptr;
        for (auto const& payload : payloads_) {
            auto b = get_pointer(payload);
            auto s = get_size(payload);
            auto e = b + s;
            std::copy(b, e, it);
            it += s;
        }
        return buffer(string_view(ptr, size), force_move(spa));
    }

    /**
     * @brief Set dup flag
     * @param dup flag value to set
     */
    constexpr void set_dup(bool dup) {
        publish::set_dup(fixed_header_, dup);
    }

private:
    std::uint8_t fixed_header_;
    as::const_buffer topic_name_;
    boost::container::static_vector<char, 2> topic_name_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::vector<as::const_buffer> payloads_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

using publish_message = basic_publish_message<2>;
using publish_32_message = basic_publish_message<4>;

template <std::size_t PacketIdBytes>
class basic_subscribe_message {
private:
    struct entry {
        entry(as::const_buffer topic_name, subscribe_options qos_value)
            : topic_name_(topic_name),
              topic_name_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_name_.size())) },
              qos_(qos_value.get_qos())
        {}

        as::const_buffer topic_name_;
        boost::container::static_vector<char, 2> topic_name_length_buf_;
        qos qos_;
    };

public:
    basic_subscribe_message(
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        typename packet_id_type<PacketIdBytes>::type packet_id
    )
        : fixed_header_(make_fixed_header(control_packet_type::subscribe, 0b0010)),
          remaining_length_(PacketIdBytes)
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        // Check for errors before allocating.
        for (auto&& e : params) {
            as::const_buffer topic_name = std::get<0>(e);
            utf8string_check(topic_name);
        }

        entries_.reserve(params.size());
        for (auto&& e : params) {
            as::const_buffer topic_name = std::get<0>(e);
            size_t size = topic_name.size();

            entries_.emplace_back(topic_name, std::get<1>(e));
            remaining_length_ +=
                2 +                     // topic name length
                size +                  // topic name
                1;                      // means QoS
        }
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());

        ret.emplace_back(as::buffer(&fixed_header_, 1));

        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

        ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

        for (auto const& e : entries_) {
            ret.emplace_back(as::buffer(e.topic_name_length_buf_.data(), e.topic_name_length_buf_.size()));
            ret.emplace_back(as::buffer(e.topic_name_));
            ret.emplace_back(as::buffer(&e.qos_, 1));
        }

        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            entries_.size() * 3;  // topic name length, topic name, qos
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;

        ret.reserve(size());

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(packet_id_.data(), packet_id_.size());

        for (auto const& e : entries_) {
            ret.append(e.topic_name_length_buf_.data(), e.topic_name_length_buf_.size());
            ret.append(get_pointer(e.topic_name_), get_size(e.topic_name_));
            ret.push_back(static_cast<char>(e.qos_));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

using subscribe_message = basic_subscribe_message<2>;

template <std::size_t PacketIdBytes>
class basic_suback_message {
public:
    basic_suback_message(
        std::vector<suback_return_code> params,
        typename packet_id_type<PacketIdBytes>::type packet_id
    )
        : fixed_header_(make_fixed_header(control_packet_type::suback, 0b0000)),
          remaining_length_(params.size() + PacketIdBytes)
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        // TODO: We should be able to simply static-cast params.data() into a char*.
        entries_.reserve(params.size());
        for (auto e : params) {
            entries_.push_back(static_cast<char>(e));
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());

        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));
        ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));
        ret.emplace_back(as::buffer(entries_));

        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return 4; // fixed header, remaining length, packet_id, entries
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;

        ret.reserve(size());

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(packet_id_.data(), packet_id_.size());
        ret.append(entries_);

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::string entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

using suback_message = basic_suback_message<2>;

template <std::size_t PacketIdBytes>
class basic_unsubscribe_message {
private:
    struct entry {
        entry(as::const_buffer topic_name)
            : topic_name_(force_move(topic_name)),
              topic_name_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_name_.size())) }
        {}

        as::const_buffer topic_name_;
        boost::container::static_vector<char, 2> topic_name_length_buf_;
    };

public:
    basic_unsubscribe_message(
        std::vector<as::const_buffer> params,
        typename packet_id_type<PacketIdBytes>::type packet_id
    )
        : fixed_header_(make_fixed_header(control_packet_type::unsubscribe, 0b0010)),
          remaining_length_(PacketIdBytes)
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        // Check for errors before allocating.
        for (auto&& e : params) {
            utf8string_check(e);
        }

        entries_.reserve(params.size());
        for (auto&& e : params) {
            entries_.emplace_back(e);
            remaining_length_ +=
                2 +          // topic name length
                e.size();    // topic name
        }
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());

        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

        ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

        for (auto const& e : entries_) {
            ret.emplace_back(as::buffer(e.topic_name_length_buf_.data(), e.topic_name_length_buf_.size()));
            ret.emplace_back(as::buffer(e.topic_name_));
        }

        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            entries_.size() * 2;  // topic name length, topic name
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;
        ret.reserve(size());

        ret.push_back(static_cast<char>(fixed_header_));

        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(packet_id_.data(), packet_id_.size());

        for (auto const& e : entries_) {
            ret.append(e.topic_name_length_buf_.data(), e.topic_name_length_buf_.size());
            ret.append(get_pointer(e.topic_name_), get_size(e.topic_name_));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

using unsubscribe_message = basic_unsubscribe_message<2>;

} // inline namespace v3_1_1

} // namespace MQTT_NS

#endif // MQTT_MESSAGE_HPP

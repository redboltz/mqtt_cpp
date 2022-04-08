// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_V5_MESSAGE_HPP)
#define MQTT_V5_MESSAGE_HPP

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
#include <mqtt/string_check.hpp>
#include <mqtt/property.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/property_parse.hpp>
#include <mqtt/reason_code.hpp>
#include <mqtt/packet_id_type.hpp>
#include <mqtt/move.hpp>
#include <mqtt/variant_visit.hpp>
#include <mqtt/optional.hpp>

#if !defined(MQTT_ALWAYS_SEND_REASON_CODE)
#define MQTT_ALWAYS_SEND_REASON_CODE false
#endif // !defined(MQTT_ALWAYS_SEND_REASON_CODE)

namespace MQTT_NS {

namespace as = boost::asio;

namespace v5 {

namespace detail {

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
        return std::string(message_.data(), message_.size());
    }
private:
    boost::container::static_vector<char, 2> message_;
};

} // namespace detail

class connect_message {
public:
    connect_message(
        std::uint16_t keep_alive_sec,
        buffer client_id,
        bool clean_start,
        optional<will> w,
        optional<buffer> user_name,
        optional<buffer> password,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::connect, 0b0000)),
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
          protocol_name_and_level_ { 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05 },
          client_id_(force_move(client_id)),
          client_id_length_buf_{ num_to_2bytes(boost::numeric_cast<std::uint16_t>(client_id_.size())) },
          will_property_length_(
              w ?
              std::accumulate(
                  w.value().props().begin(),
                  w.value().props().end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
              : 0U
          ),
          will_props_(
              w ?
              force_move(w.value().props())
              : properties()
          ),
          keep_alive_buf_ ({ num_to_2bytes(keep_alive_sec ) }),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // protocol name and level
              1 +                   // connect flags
              1 +                   // keep alive
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              ) +
              2                     // client id length, client id
          )
    {
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ += property_length_buf_.size() + property_length_;

        utf8string_check(client_id_);
        if (clean_start) connect_flags_ |= connect_flags::clean_start;
        if (user_name) {
            utf8string_check(user_name.value());
            connect_flags_ |= connect_flags::user_name_flag;
            user_name_ = force_move(user_name.value());
            add_uint16_t_to_buf(user_name_length_buf_, boost::numeric_cast<std::uint16_t>(user_name_.size()));

            remaining_length_ += 2 + user_name_.size();
            num_of_const_buffer_sequence_ += 2; // user name length, user name
        }
        if (password) {
            connect_flags_ |= connect_flags::password_flag;
            password_ = force_move(password.value());
            add_uint16_t_to_buf(password_length_buf_, boost::numeric_cast<std::uint16_t>(password_.size()));

            remaining_length_ += 2 + password_.size();
            num_of_const_buffer_sequence_ += 2; // password length, password
        }
        if (w) {
            connect_flags_ |= connect_flags::will_flag;
            if (w.value().get_retain() == retain::yes) connect_flags_ |= connect_flags::will_retain;
            connect_flags::set_will_qos(connect_flags_, w.value().get_qos());

            auto wpb = variable_bytes(will_property_length_);
            for (auto e : wpb) {
                will_property_length_buf_.push_back(e);
            }

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

            remaining_length_ +=
                will_property_length_buf_.size() +
                will_property_length_ +
                2 + will_topic_name_.size() + 2 + will_message_.size();
            num_of_const_buffer_sequence_ +=
                std::accumulate(
                  will_props_.begin(),
                  will_props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
                ) +
                2 +                   // will topic name length, will topic name
                2;                    // will message length, will message

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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        ret.emplace_back(as::buffer(client_id_length_buf_.data(), client_id_length_buf_.size()));
        ret.emplace_back(as::buffer(client_id_));

        if (connect_flags::has_will_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(will_property_length_buf_.data(), will_property_length_buf_.size()));
            for (auto const& p : will_props_) {
                v5::add_const_buffer_sequence(ret, p);
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        ret.append(property_length_buf_.data(), property_length_buf_.size());

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        ret.append(client_id_length_buf_.data(), client_id_length_buf_.size());
        ret.append(client_id_.data(), client_id_.size());

        if (connect_flags::has_will_flag(connect_flags_)) {
            ret.append(will_property_length_buf_.data(), will_property_length_buf_.size());
            auto it = ret.end();
            ret.resize(ret.size() + will_property_length_);
            auto end = ret.end();
            for (auto const& p : will_props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
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

    std::size_t will_property_length_;
    boost::container::static_vector<char, 4> will_property_length_buf_;
    properties will_props_;

    buffer will_topic_name_;
    boost::container::static_vector<char, 2> will_topic_name_length_buf_;
    buffer will_message_;
    boost::container::static_vector<char, 2> will_message_length_buf_;

    buffer user_name_;
    boost::container::static_vector<char, 2> user_name_length_buf_;
    buffer password_;
    boost::container::static_vector<char, 2> password_length_buf_;

    boost::container::static_vector<char, 2> keep_alive_buf_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;

    std::size_t num_of_const_buffer_sequence_;
};

class connack_message {
public:
    connack_message(
        bool session_present,
        connect_reason_code reason_code,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::connack, 0b0000)),
          remaining_length_(
              1 + // connect acknowledge flags
              1   // reason code
          ),
          connect_acknowledge_flags_(session_present ? 1 : 0),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // connect acknowledge flags
              1 +                   // reason code
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              )
          )
    {
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ += property_length_buf_.size() + property_length_;
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
        ret.emplace_back(as::buffer(&connect_acknowledge_flags_, 1));
        ret.emplace_back(as::buffer(&reason_code_, 1));

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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
        ret.push_back(static_cast<char>(connect_acknowledge_flags_));
        ret.push_back(static_cast<char>(reason_code_));

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    std::uint8_t connect_acknowledge_flags_;

    connect_reason_code reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;

    std::size_t num_of_const_buffer_sequence_;
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
        publish_options pubopts,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::publish, 0b0000) | pubopts.operator std::uint8_t()),
          topic_name_(topic_name),
          topic_name_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_name_.size())) },
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          remaining_length_(
              2                      // topic name length
              + topic_name_.size()   // topic name
              + (  (pubopts.get_qos() == qos::at_least_once || pubopts.get_qos() == qos::exactly_once)
                 ? PacketIdBytes // packet_id
                 : 0)
          ),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // topic name length
              1 +                   // topic name
              ((pubopts.get_qos() == qos::at_most_once) ? 0U : 1U) + // packet id
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              )
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
        num_of_const_buffer_sequence_ += num_of_payloads;

        utf8string_check(topic_name_);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ += property_length_buf_.size() + property_length_;

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
            std::next(buf.begin(), static_cast<buffer::difference_type>(consumed)),
            std::back_inserter(remaining_length_buf_));
        buf.remove_prefix(consumed);

        if (buf.size() < 2) throw remaining_length_error();
        std::copy(buf.begin(), std::next(buf.begin(), 2), std::back_inserter(topic_name_length_buf_));
        auto topic_name_length = make_uint16_t(buf.begin(), std::next(buf.begin(), 2));
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

        auto len_consume = variable_length(
            buf.begin(),
            buf.end()
        );
        property_length_ = std::get<0>(len_consume);
        auto consume = std::get<1>(len_consume);
        if (consume == 0) throw property_length_error();
        std::copy(
            buf.begin(),
            std::next(buf.begin(), static_cast<buffer::difference_type>(consume)),
            std::back_inserter(property_length_buf_)
        );
        buf.remove_prefix(consume);
        if (buf.size() < property_length_) throw property_length_error();

        props_ = property::parse(buf.substr(0, property_length_));
        buf.remove_prefix(property_length_);
        if (!buf.empty()) {
            payloads_.emplace_back(as::buffer(buf));
        }
        num_of_const_buffer_sequence_ =
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // topic name length
            1 +                   // topic name
            ((qos_value == qos::at_most_once) ? 0U : 1U) + // packet id
            1 +                   // property length
            std::accumulate(
                props_.begin(),
                props_.end(),
                std::size_t(0U),
                [](std::size_t total, property_variant const& pv) {
                    return total + v5::num_of_const_buffer_sequence(pv);
                }
            ) +
            payloads_.size();     // payload
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
        ret.emplace_back(topic_name_length_buf_.data(), topic_name_length_buf_.size());
        ret.emplace_back(as::buffer(topic_name_));

        if (!packet_id_.empty()) {
            ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));
        }

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        ret.append(topic_name_length_buf_.data(), topic_name_length_buf_.size());
        ret.append(get_pointer(topic_name_), get_size(topic_name_));

        ret.append(packet_id_.data(), packet_id_.size());

        ret.append(property_length_buf_.data(), property_length_buf_.size());

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

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
    string_view topic() const {
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
     * @brief Get properties
     * @return properties
     */
    properties const& props() const {
        return props_;
    }

    /**
     * @brief Add property
     * @param p property to add
     */
    void add_prop(property_variant p) {
        auto add_size = v5::size(p);
        props_.push_back(force_move(p));
        property_length_ += add_size;
        property_length_buf_.clear();
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_buf_.clear();
        remaining_length_ += add_size;
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    /**
     * @brief Update property
     *        Only fixed size property can be updated.
     * @param p property to update
     */
    template <typename Property>
    std::enable_if_t<
        std::is_base_of<property::detail::n_bytes_property<1>, Property>::value ||
        std::is_base_of<property::detail::n_bytes_property<2>, Property>::value ||
        std::is_base_of<property::detail::n_bytes_property<4>, Property>::value
    >
    update_prop(Property update_prop) {
        for (auto& p : props_) {
            MQTT_NS::visit(
                make_lambda_visitor(
                    [&update_prop](Property& t) { t = std::forward<Property>(update_prop); },
                    [](auto&) { }
                ),
                p
            );
        }
    }

    /**
     * @brief Remove property
     * @param id property::id to remove
     */
    void remove_prop(v5::property::id id) {
        std::size_t removed_size = 0;
        auto it = props_.begin();
        auto end = props_.begin();
        while (it != end) {
            if (v5::id(*it) == id) {
                removed_size += v5::size(*it);
                it = props_.erase(it);
            }
            else {
                ++it;
            }
        }

        property_length_ -= removed_size;
        property_length_buf_.clear();
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_buf_.clear();
        remaining_length_ -= removed_size;
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    /**
     * @brief Set dup flag
     * @param dup flag value to set
     */
    constexpr void set_dup(bool dup) {
        publish::set_dup(fixed_header_, dup);
    }

    /**
     * @brief Set topic name
     * @param topic_name value to set
     */
    void set_topic_name(as::const_buffer topic_name) {
        auto prev_topic_name_size = get_size(topic_name_);
        topic_name_ = force_move(topic_name);
        topic_name_length_buf_ = boost::container::static_vector<char, 2>{
            num_to_2bytes(boost::numeric_cast<std::uint16_t>(get_size(topic_name_)))
        };

        remaining_length_buf_.clear();
        remaining_length_ =  remaining_length_ - prev_topic_name_size + get_size(topic_name_);
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

private:
    std::uint8_t fixed_header_;
    as::const_buffer topic_name_;
    boost::container::static_vector<char, 2> topic_name_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::vector<as::const_buffer> payloads_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    std::size_t num_of_const_buffer_sequence_;
};

using publish_message = basic_publish_message<2>;
using publish_32_message = basic_publish_message<4>;

template <std::size_t PacketIdBytes>
struct basic_puback_message {
    basic_puback_message(
        typename packet_id_type<PacketIdBytes>::type packet_id,
        v5::puback_reason_code reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::puback, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              // TODO: This is wrong. The reason code MUST be provided
              // if there are properties. Not the other way around.
              // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124
              // 3.4.2.1 PUBACK Reason Code
              // The Reason Code and Property Length can be omitted if
              // the Reason Code is 0x00 (Success) and there are no Properties.
              // In this case the PUBACK has a Remaining Length of 2.
              [&] () -> std::size_t {
                  if ((reason_code_ != v5::puback_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                      // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                      // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                      if (props_.empty()) {
                          return 1;                 // reason code
                      }
                      else {
                          return
                              1 +                   // reason code
                              1 +                   // property length
                              std::accumulate(      // properties
                                  props_.begin(),
                                  props_.end(),
                                  std::size_t(0U),
                                  [](std::size_t total, property_variant const& pv) {
                                      return total + v5::num_of_const_buffer_sequence(pv);
                                  }
                              );
                      }
                  }
                  else {
                      return 0;
                  }
              } ()
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            // TODO: This is wrong. The reason code MUST be provided
            // if there are properties. Not the other way around.
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124
            // 3.4.2.1 PUBACK Reason Code
            // The Reason Code and Property Length can be omitted if
            // the Reason Code is 0x00 (Success) and there are no Properties.
            // In this case the PUBACK has a Remaining Length of 2.
            [&] () -> std::size_t {
                if ((reason_code_ != v5::puback_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
                    // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                    if (props_.empty()) {
                        return 1;                 // reason code
                    }
                    else {
                        return
                            1 +                   // reason code
                            property_length_buf_.size() +
                            property_length_;
                    }
                }
                else {
                    return 0;
                }
            } ();

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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124
        // 3.4.2.1 PUBACK Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBACK has a Remaining Length of 2.
        if (reason_code_ != v5::puback_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
                for (auto const& p : props_) {
                    v5::add_const_buffer_sequence(ret, p);
                }
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;
        auto sz = size();
        ret.reserve(sz);

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901124
        // 3.4.2.1 PUBACK Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBACK has a Remaining Length of 2.
        if (reason_code_ != v5::puback_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901126
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.append(property_length_buf_.data(), property_length_buf_.size());

                auto it = ret.end();
                ret.resize(sz);
                auto end = ret.end();
                for (auto const& p : props_) {
                    v5::fill(p, it, end);
                    it += static_cast<std::string::difference_type>(v5::size(p));
                }
            }
        }
        return ret;
    }

    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    v5::puback_reason_code reason_code_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using puback_message = basic_puback_message<2>;

template <std::size_t PacketIdBytes>
struct basic_pubrec_message {
    basic_pubrec_message(
        typename packet_id_type<PacketIdBytes>::type packet_id,
        pubrec_reason_code reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubrec, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              // TODO: This is wrong. The reason code MUST be provided
              // if there are properties. Not the other way around.
              // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134
              // 3.5.2.1 PUBREC Reason Code
              // The Reason Code and Property Length can be omitted if
              // the Reason Code is 0x00 (Success) and there are no Properties.
              // In this case the PUBREC has a Remaining Length of 2.
              [&] () -> std::size_t {
                  if ((reason_code_ != v5::pubrec_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                      // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                      // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                      if (props_.empty()) {
                          return 1;                 // reason code
                      }
                      else {
                          return
                              1 +                   // reason code
                              1 +                   // property length
                              std::accumulate(      // properties
                                  props_.begin(),
                                  props_.end(),
                                  std::size_t(0U),
                                  [](std::size_t total, property_variant const& pv) {
                                      return total + v5::num_of_const_buffer_sequence(pv);
                                  }
                              );
                      }
                  }
                  else {
                      return 0;
                  }
              } ()
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            // TODO: This is wrong. The reason code MUST be provided
            // if there are properties. Not the other way around.
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134
            // 3.5.2.1 PUBREC Reason Code
            // The Reason Code and Property Length can be omitted if
            // the Reason Code is 0x00 (Success) and there are no Properties.
            // In this case the PUBREC has a Remaining Length of 2.
            [&] () -> std::size_t {
                if ((reason_code_ != v5::pubrec_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
                    // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                    if (props_.empty()) {
                        return 1;                 // reason code
                    }
                    else {
                        return
                            1 +                   // reason code
                            property_length_buf_.size() +
                            property_length_;
                    }
                }
                else {
                    return 0;
                }
            } ();

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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134
        // 3.5.2.1 PUBREC Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBREC has a Remaining Length of 2.
        if (reason_code_ != v5::pubrec_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
                for (auto const& p : props_) {
                    v5::add_const_buffer_sequence(ret, p);
                }
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;
        auto sz = size();
        ret.reserve(sz);

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901134
        // 3.5.2.1 PUBREC Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBREC has a Remaining Length of 2.
        if (reason_code_ != v5::pubrec_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901136
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.append(property_length_buf_.data(), property_length_buf_.size());

                auto it = ret.end();
                ret.resize(sz);
                auto end = ret.end();
                for (auto const& p : props_) {
                    v5::fill(p, it, end);
                    it += static_cast<std::string::difference_type>(v5::size(p));
                }
            }
        }
        return ret;
    }


    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    pubrec_reason_code reason_code_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using pubrec_message = basic_pubrec_message<2>;

template <std::size_t PacketIdBytes>
struct basic_pubrel_message {
    basic_pubrel_message(
        typename packet_id_type<PacketIdBytes>::type packet_id,
        v5::pubrel_reason_code reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubrel, 0b0010)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              // TODO: This is wrong. The reason code MUST be provided
              // if there are properties. Not the other way around.
              // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
              // 3.6.2.1 PUBREL Reason Code
              // The Reason Code and Property Length can be omitted if
              // the Reason Code is 0x00 (Success) and there are no Properties.
              // In this case the PUBREL has a Remaining Length of 2.
              [&] () -> std::size_t {
                  if ((reason_code_ != v5::pubrel_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                      // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                      // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                      if (props_.empty()) {
                          return 1;                 // reason code
                      }
                      else {
                          return
                              1 +                   // reason code
                              1 +                   // property length
                              std::accumulate(      // properties
                                  props_.begin(),
                                  props_.end(),
                                  std::size_t(0U),
                                  [](std::size_t total, property_variant const& pv) {
                                      return total + v5::num_of_const_buffer_sequence(pv);
                                  }
                              );
                      }
                  }
                  else {
                      return 0;
                  }
              } ()
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            // TODO: This is wrong. The reason code MUST be provided
            // if there are properties. Not the other way around.
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
            // 3.6.2.1 PUBREL Reason Code
            // The Reason Code and Property Length can be omitted if
            // the Reason Code is 0x00 (Success) and there are no Properties.
            // In this case the PUBREL has a Remaining Length of 2.
            [&] () -> std::size_t {
                if ((reason_code_ != v5::pubrel_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                    if (props_.empty()) {
                        return 1;                 // reason code
                    }
                    else {
                        return
                            1 +                   // reason code
                            property_length_buf_.size() +
                            property_length_;
                    }
                }
                else {
                    return 0;
                }
            } ();

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    basic_pubrel_message(buffer buf) {
        if (buf.empty())  throw remaining_length_error();
        fixed_header_ = static_cast<std::uint8_t>(buf.front());
        buf.remove_prefix(1);

        if (buf.empty()) throw remaining_length_error();
        auto len_consumed = remaining_length(buf.begin(), buf.end());
        remaining_length_ = std::get<0>(len_consumed);
        auto consumed = std::get<1>(len_consumed);

        std::copy(
            buf.begin(),
            std::next(buf.begin(), static_cast<buffer::difference_type>(consumed)),
            std::back_inserter(remaining_length_buf_));
        buf.remove_prefix(consumed);

        if (buf.size() < PacketIdBytes) throw remaining_length_error();
        std::copy(buf.begin(), std::next(buf.begin(), PacketIdBytes), std::back_inserter(packet_id_));
        buf.remove_prefix(PacketIdBytes);

        if (buf.empty()) {
            num_of_const_buffer_sequence_ =
                1 +                   // fixed header
                1 +                   // remaining length
                1;                    // packet id
            reason_code_ = v5::pubrel_reason_code::success;
            return;
        }

        reason_code_ = static_cast<v5::pubrel_reason_code>(buf.front());
        buf.remove_prefix(1);

        if (buf.empty()) {
            property_length_ = 0;
        }
        else {
            auto len_consume = variable_length(
                buf.begin(),
                buf.end()
            );
            property_length_ = std::get<0>(len_consume);
            auto consume = std::get<1>(len_consume);
            if (consume == 0) throw property_length_error();
            std::copy(
                buf.begin(),
                std::next(buf.begin(), static_cast<buffer::difference_type>(consume)),
                std::back_inserter(property_length_buf_)
            );
            buf.remove_prefix(consume);
            if (buf.size() != property_length_) throw property_length_error();

            props_ = property::parse(buf);
            buf.remove_prefix(property_length_);
        }

        num_of_const_buffer_sequence_ =
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            // TODO: This is wrong. The reason code MUST be provided
            // if there are properties. Not the other way around.
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
            // 3.6.2.1 PUBREL Reason Code
            // The Reason Code and Property Length can be omitted if
            // the Reason Code is 0x00 (Success) and there are no Properties.
            // In this case the PUBREL has a Remaining Length of 2.
            [&] () -> std::size_t {
                if ((reason_code_ != v5::pubrel_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
                    // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                    if (props_.empty()) {
                        return 1;                 // reason code
                    }
                    else {
                        return
                            1 +                   // reason code
                            1 +                   // property length
                            std::accumulate(      // properties
                                props_.begin(),
                                props_.end(),
                                std::size_t(0U),
                                [](std::size_t total, property_variant const& pv) {
                                    return total + v5::num_of_const_buffer_sequence(pv);
                                }
                            );
                    }
                }
                else {
                    return 0;
                }
            } ();
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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
        // 3.6.2.1 PUBREL Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBREL has a Remaining Length of 2.
        if(reason_code_ != v5::pubrel_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));

                for (auto const& p : props_) {
                    v5::add_const_buffer_sequence(ret, p);
                }
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;
        auto sz = size();
        ret.reserve(sz);

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());
        ret.append(packet_id_.data(), packet_id_.size());

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
        // 3.6.2.1 PUBREL Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBREL has a Remaining Length of 2.
        if (reason_code_ != v5::pubrel_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901146
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.append(property_length_buf_.data(), property_length_buf_.size());

                auto it = ret.end();
                ret.resize(sz);
                auto end = ret.end();
                for (auto const& p : props_) {
                    v5::fill(p, it, end);
                    it += static_cast<std::string::difference_type>(v5::size(p));
                }
            }
        }
        return ret;
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    decltype(auto) packet_id() const {
        return make_packet_id<PacketIdBytes>::apply(packet_id_.begin(), packet_id_.end());
    }

    /**
     * @brief Get reason_code
     * @return reason_code
     */
    v5::pubrel_reason_code reason_code() const {
        return reason_code_;
    }

    /**
     * @brief Get properties
     * @return properties
     */
    properties const& props() const {
        return props_;
    }

    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    v5::pubrel_reason_code reason_code_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using pubrel_message = basic_pubrel_message<2>;
using pubrel_32_message = basic_pubrel_message<4>;

template <std::size_t PacketIdBytes>
struct basic_pubcomp_message {
    basic_pubcomp_message(
        typename packet_id_type<PacketIdBytes>::type packet_id,
        pubcomp_reason_code reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubcomp, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              // TODO: This is wrong. The reason code MUST be provided
              // if there are properties. Not the other way around.
              // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154
              // 3.7.2.1 PUBCOMP Reason Code
              // The Reason Code and Property Length can be omitted if
              // the Reason Code is 0x00 (Success) and there are no Properties.
              // In this case the PUBCOMP has a Remaining Length of 2.
              [&] () -> std::size_t {
                  if ((reason_code_ != v5::pubcomp_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                      // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                      // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                      if (props_.empty()) {
                          return 1;                 // reason code
                      }
                      else {
                          return
                              1 +                   // reason code
                              1 +                   // property length
                              std::accumulate(      // properties
                                  props_.begin(),
                                  props_.end(),
                                  std::size_t(0U),
                                  [](std::size_t total, property_variant const& pv) {
                                      return total + v5::num_of_const_buffer_sequence(pv);
                                  }
                              );
                      }
                  }
                  else {
                      return 0;
                  }
              } ()
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            // TODO: This is wrong. The reason code MUST be provided
            // if there are properties. Not the other way around.
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154
            // 3.7.2.1 PUBCOMP Reason Code
            // The Reason Code and Property Length can be omitted if
            // the Reason Code is 0x00 (Success) and there are no Properties.
            // In this case the PUBCOMP has a Remaining Length of 2.
            [&] () -> std::size_t {
                if ((reason_code_ != v5::pubcomp_reason_code::success) || MQTT_ALWAYS_SEND_REASON_CODE) {
                    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
                    // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
                    if (props_.empty()) {
                        return 1;                 // reason code
                    }
                    else {
                        return
                            1 +                   // reason code
                            property_length_buf_.size() +
                            property_length_;
                    }
                }
                else {
                    return 0;
                }
            } ();

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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154
        // 3.7.2.1 PUBCOMP Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBCOMP has a Remaining Length of 2.
        if (reason_code_ != v5::pubcomp_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));
            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));

                for (auto const& p : props_) {
                    v5::add_const_buffer_sequence(ret, p);
                }
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        std::string ret;
        auto sz = size();
        ret.reserve(sz);

        ret.push_back(static_cast<char>(fixed_header_));
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901154
        // 3.7.2.1 PUBCOMP Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no Properties.
        // In this case the PUBCOMP has a Remaining Length of 2.
        if (reason_code_ != v5::pubcomp_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901156
            // If the Remaining Length is less than 4 there is no Property Length and the value of 0 is used.
            if (!props_.empty()) {
                ret.append(property_length_buf_.data(), property_length_buf_.size());

                auto it = ret.end();
                ret.resize(sz);
                auto end = ret.end();
                for (auto const& p : props_) {
                    v5::fill(p, it, end);
                    it += static_cast<std::string::difference_type>(v5::size(p));
                }
            }
        }
        return ret;
    }


    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    pubcomp_reason_code reason_code_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using pubcomp_message = basic_pubcomp_message<2>;

template <std::size_t PacketIdBytes>
class basic_subscribe_message {
private:
    struct entry {
        entry(as::const_buffer topic_filter, subscribe_options options)
            : topic_filter_(topic_filter),
              topic_filter_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_filter_.size())) },
              options_(options)
        {}

        as::const_buffer topic_filter_;
        boost::container::static_vector<char, 2> topic_filter_length_buf_;
        subscribe_options options_;
    };

public:
    basic_subscribe_message(
        std::vector<std::tuple<as::const_buffer, subscribe_options>> params,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::subscribe, 0b0010)),
          remaining_length_(PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              ) +
              params.size() * 3   // topic filter length, topic filter, qos
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ +=
            property_length_buf_.size() +
            property_length_;

        // Check for errors before allocating.
        for (auto&& e : params) {
            as::const_buffer topic_filter = std::get<0>(e);
            utf8string_check(topic_filter);
        }

        entries_.reserve(params.size());
        for (auto&& e : params) {
            as::const_buffer topic_filter = std::get<0>(e);
            size_t size = topic_filter.size();

            entries_.emplace_back(topic_filter, std::get<1>(e));
            remaining_length_ +=
                2 +               // topic filter length
                size +            // topic filter
                1;                // means QoS
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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        for (auto const& e : entries_) {
            ret.emplace_back(as::buffer(e.topic_filter_length_buf_.data(), e.topic_filter_length_buf_.size()));
            ret.emplace_back(as::buffer(e.topic_filter_));
            ret.emplace_back(as::buffer(&e.options_, 1));
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        ret.append(property_length_buf_.data(), property_length_buf_.size());

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        for (auto const& e : entries_) {
            ret.append(e.topic_filter_length_buf_.data(), e.topic_filter_length_buf_.size());
            ret.append(get_pointer(e.topic_filter_), get_size(e.topic_filter_));
            ret.push_back(static_cast<char>(e.options_.operator std::uint8_t()));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using subscribe_message = basic_subscribe_message<2>;

template <std::size_t PacketIdBytes>
class basic_suback_message {
public:
    basic_suback_message(
        std::vector<suback_reason_code> reason_codes,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::suback, 0b0000)),
          remaining_length_(reason_codes.size() + PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              ) +
              1                     // entries (reason code ...)
          )
   {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ +=
            property_length_buf_.size() +
            property_length_;

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        entries_.reserve(reason_codes.size());
        for (auto e : reason_codes) {
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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        ret.append(entries_);

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::string entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using suback_message = basic_suback_message<2>;

template <std::size_t PacketIdBytes>
class basic_unsubscribe_message {
private:
    struct entry {
        entry(as::const_buffer topic_filter)
            : topic_filter_(topic_filter),
              topic_filter_length_buf_ { num_to_2bytes(boost::numeric_cast<std::uint16_t>(topic_filter.size())) }
        {}

        as::const_buffer topic_filter_;
        boost::container::static_vector<char, 2> topic_filter_length_buf_;
    };

public:
    basic_unsubscribe_message(
        std::vector<as::const_buffer> params,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::unsubscribe, 0b0010)),
          remaining_length_(PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              ) +
              params.size() * 2   // topic filter length, topic filter
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ +=
            property_length_buf_.size() +
            property_length_;

        // Check for errors before allocating.
        for (auto&& e : params) {
            utf8string_check(e);
        }

        entries_.reserve(params.size());
        for (auto&& e : params) {
            auto size = e.size();
            entries_.emplace_back(e);
            remaining_length_ +=
                2 +          // topic filter length
                size;        // topic filter
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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        for (auto const& e : entries_) {
            ret.emplace_back(as::buffer(e.topic_filter_length_buf_.data(), e.topic_filter_length_buf_.size()));
            ret.emplace_back(as::buffer(e.topic_filter_));
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        for (auto const& e : entries_) {
            ret.append(e.topic_filter_length_buf_.data(), e.topic_filter_length_buf_.size());
            ret.append(get_pointer(e.topic_filter_), get_size(e.topic_filter_));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using unsubscribe_message = basic_unsubscribe_message<2>;

template <std::size_t PacketIdBytes>
class basic_unsuback_message {
public:
    basic_unsuback_message(
        std::vector<v5::unsuback_reason_code> reason_codes,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::unsuback, 0b0000)),
          reason_codes_(force_move(reason_codes)),
          remaining_length_(reason_codes_.size() + PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props_.begin(),
                  props_.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              )
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ +=
            property_length_buf_.size() +
            property_length_;

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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        ret.emplace_back(as::buffer(reinterpret_cast<char const*>(reason_codes_.data()), reason_codes_.size()));

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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        auto it = ret.end();
        ret.resize(ret.size() + property_length_);
        auto end = ret.end();
        for (auto const& p : props_) {
            v5::fill(p, it, end);
            it += static_cast<std::string::difference_type>(v5::size(p));
        }

        ret.append(reinterpret_cast<char const*>(reason_codes_.data()), reason_codes_.size());

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    std::vector<v5::unsuback_reason_code> reason_codes_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

using unsuback_message = basic_unsuback_message<2>;

struct pingreq_message : detail::header_only_message {
    pingreq_message()
        : detail::header_only_message(control_packet_type::pingreq, 0b0000)
    {}
};

struct pingresp_message : detail::header_only_message {
    pingresp_message()
        : detail::header_only_message(control_packet_type::pingresp, 0b0000)
    {}
};

struct disconnect_message {
    disconnect_message(
        v5::disconnect_reason_code reason_code,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::disconnect, 0b0000)),
          remaining_length_(0),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              (
                  // TODO: This is wrong. The reason code MUST be provided
                  // if there are properties. Not the other way around.
                  // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
                  // 3.14.2.1 Disconnect Reason Code
                  // The Reason Code and Property Length can be omitted if
                  // the Reason Code is 0x00 (Normal disconnecton) and there are no
                  // Properties. In this case the DISCONNECT has a Remaining Length of 0.
                  reason_code_ != v5::disconnect_reason_code::normal_disconnection || MQTT_ALWAYS_SEND_REASON_CODE ? (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props_.begin(),
                          props_.end(),
                          std::size_t(0U),
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }
        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
        // 3.14.2.1 Disconnect Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Normal disconnecton) and there are no
        // Properties. In this case the DISCONNECT has a Remaining Length of 0.
        if (reason_code_ != v5::disconnect_reason_code::normal_disconnection || MQTT_ALWAYS_SEND_REASON_CODE) {
            remaining_length_ =
                1 +                           // reason code
                property_length_buf_.size() +
                property_length_;
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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
        // 3.14.2.1 Disconnect Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Normal disconnecton) and there are no
        // Properties. In this case the DISCONNECT has a Remaining Length of 0.
        if (reason_code_ != v5::disconnect_reason_code::normal_disconnection || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));

            ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
            for (auto const& p : props_) {
                v5::add_const_buffer_sequence(ret, p);
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901144
        // 3.14.2.1 Disconnect Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Normal disconnecton) and there are no
        // Properties. In this case the DISCONNECT has a Remaining Length of 0.
        if (reason_code_ != v5::disconnect_reason_code::normal_disconnection || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            auto it = ret.end();
            ret.resize(ret.size() + property_length_);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    v5::disconnect_reason_code reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

struct auth_message {
    auth_message(
        v5::auth_reason_code reason_code,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::auth, 0b0000)),
          remaining_length_(0),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(force_move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              (
                  // TODO: This is wrong. The reason code MUST be provided
                  // if there are properties. Not the other way around.
                  // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220
                  // 3.15.2.1 Authenticate Reason Code
                  // The Reason Code and Property Length can be omitted if
                  // the Reason Code is 0x00 (Success) and there are no
                  // Properties. In this case the AUTH has a Remaining Length of 0.
                  reason_code_ != v5::auth_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props_.begin(),
                          props_.end(),
                          std::size_t(0U),
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }
        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220
        // 3.15.2.1 Authenticate Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no
        // Properties. In this case the AUTH has a Remaining Length of 0.
        if (reason_code_ != v5::auth_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            remaining_length_ =
                1 +                           // reason code
                property_length_buf_.size() +
                property_length_;
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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220
        // 3.15.2.1 Authenticate Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no
        // Properties. In this case the AUTH has a Remaining Length of 0.
        if (reason_code_ != v5::auth_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.emplace_back(as::buffer(&reason_code_, 1));

            ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
            for (auto const& p : props_) {
                v5::add_const_buffer_sequence(ret, p);
            }
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
    constexpr std::size_t num_of_const_buffer_sequence() const {
        return num_of_const_buffer_sequence_;
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

        // TODO: This is wrong. The reason code MUST be provided
        // if there are properties. Not the other way around.
        // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901220
        // 3.15.2.1 Authenticate Reason Code
        // The Reason Code and Property Length can be omitted if
        // the Reason Code is 0x00 (Success) and there are no
        // Properties. In this case the AUTH has a Remaining Length of 0.
        if (reason_code_ != v5::auth_reason_code::success || MQTT_ALWAYS_SEND_REASON_CODE) {
            ret.push_back(static_cast<char>(reason_code_));

            auto it = ret.end();
            ret.resize(ret.size() + property_length_);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    v5::auth_reason_code reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

} // namespace v5

} // namespace MQTT_NS

#endif // MQTT_V5_MESSAGE_HPP

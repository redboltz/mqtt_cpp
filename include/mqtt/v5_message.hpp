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
#include <boost/optional.hpp>
#include <boost/container/static_vector.hpp>

#include <mqtt/two_byte_util.hpp>
#include <mqtt/fixed_header.hpp>
#include <mqtt/remaining_length.hpp>
#include <mqtt/qos.hpp>
#include <mqtt/const_buffer_util.hpp>
#include <mqtt/will.hpp>
#include <mqtt/connect_flags.hpp>
#include <mqtt/publish.hpp>
#include <mqtt/utility.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/string_check.hpp>
#include <mqtt/property.hpp>
#include <mqtt/property_variant.hpp>
#include <mqtt/property_parse.hpp>
#include <mqtt/packet_id_type.hpp>

namespace mqtt {

namespace as = boost::asio;

namespace v5 {

using properties = std::vector<property_variant>;

namespace detail {

class header_only_message {
public:
    /**
     * @brief Create empty header_packet_id_message.
     */
    header_only_message(std::uint8_t type, std::uint8_t flags)
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
    std::size_t num_of_const_buffer_sequence() const {
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
        std::string const& client_id,
        bool clean_session,
        mqtt::optional<will> const& w,
        mqtt::optional<std::string> const& user_name,
        mqtt::optional<std::string> const& password,
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
          client_id_(as::buffer(client_id)),
          client_id_length_buf_{ MQTT_16BITNUM_TO_BYTE_SEQ(client_id.size()) },
          will_property_length_(
              w ?
              std::accumulate(
                  w.value().props().begin(),
                  w.value().props().end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
              : 0
          ),
          will_props_(
              w ?
              std::move(w.value().props())
              : properties()
          ),
          keep_alive_buf_ ({ MQTT_16BITNUM_TO_BYTE_SEQ(keep_alive_sec ) }),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // protocol name and level
              1 +                   // connect flags
              1 +                   // keep alive
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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

        utf8string_check(client_id);
        if (clean_session) connect_flags_ |= connect_flags::clean_session;
        if (user_name) {
            utf8string_check(user_name.value());
            connect_flags_ |= connect_flags::user_name_flag;
            user_name_ = as::buffer(user_name.value());
            add_uint16_t_to_buf(user_name_length_buf_, static_cast<std::uint16_t>(get_size(user_name_)));

            remaining_length_ += 2 + get_size(user_name_);
            num_of_const_buffer_sequence_ += 2; // user name length, user name
        }
        if (password) {
            connect_flags_ |= connect_flags::password_flag;
            password_ = as::buffer(password.value());
            add_uint16_t_to_buf(password_length_buf_, static_cast<std::uint16_t>(get_size(password_)));

            remaining_length_ += 2 + get_size(password_);
            num_of_const_buffer_sequence_ += 2; // password length, password
        }
        if (w) {
            connect_flags_ |= connect_flags::will_flag;
            if (w.value().retain()) connect_flags_ |= connect_flags::will_retain;
            connect_flags::set_will_qos(connect_flags_, w.value().qos());

            auto wpb = variable_bytes(will_property_length_);
            for (auto e : wpb) {
                will_property_length_buf_.push_back(e);
            }

            utf8string_check(w.value().topic());
            will_topic_name_ = as::buffer(w.value().topic());
            add_uint16_t_to_buf(
                will_topic_name_length_buf_,
                static_cast<std::uint16_t>(get_size(will_topic_name_))
            );
            if (w.value().message().size() > 0xffffL) throw will_message_length_error();
            will_message_ = as::buffer(w.value().message());
            add_uint16_t_to_buf(
                will_message_length_buf_,
                static_cast<std::uint16_t>(get_size(will_message_)));

            remaining_length_ +=
                will_property_length_buf_.size() +
                will_property_length_ +
                2 + get_size(will_topic_name_) + 2 + get_size(will_message_);
            num_of_const_buffer_sequence_ +=
                std::accumulate(
                  will_props_.begin(),
                  will_props_.end(),
                  0U,
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
        ret.emplace_back(client_id_);

        if (connect_flags::has_will_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(will_property_length_buf_.data(), will_property_length_buf_.size()));
            for (auto const& p : will_props_) {
                v5::add_const_buffer_sequence(ret, p);
            }
            ret.emplace_back(as::buffer(will_topic_name_length_buf_.data(), will_topic_name_length_buf_.size()));
            ret.emplace_back(will_topic_name_);
            ret.emplace_back(as::buffer(will_message_length_buf_.data(), will_message_length_buf_.size()));
            ret.emplace_back(will_message_);
        }

        if (connect_flags::has_user_name_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(user_name_length_buf_.data(), user_name_length_buf_.size()));
            ret.emplace_back(user_name_);
        }

        if (connect_flags::has_password_flag(connect_flags_)) {
            ret.emplace_back(as::buffer(password_length_buf_.data(), password_length_buf_.size()));
            ret.emplace_back(password_);
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
        ret.append(get_pointer(client_id_), get_size(client_id_));

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
            ret.append(get_pointer(will_topic_name_), get_size(will_topic_name_));
            ret.append(will_message_length_buf_.data(), will_message_length_buf_.size());
            ret.append(get_pointer(will_message_), get_size(will_message_));
        }

        if (connect_flags::has_user_name_flag(connect_flags_)) {
            ret.append(user_name_length_buf_.data(), user_name_length_buf_.size());
            ret.append(get_pointer(user_name_), get_size(user_name_));
        }

        if (connect_flags::has_password_flag(connect_flags_)) {
            ret.append(password_length_buf_.data(), password_length_buf_.size());
            ret.append(get_pointer(password_), get_size(password_));
        }

        return ret;
    }

private:
    std::uint8_t fixed_header_;
    char connect_flags_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    boost::container::static_vector<char, 7> protocol_name_and_level_;
    as::const_buffer client_id_;
    boost::container::static_vector<char, 2> client_id_length_buf_;

    std::size_t will_property_length_;
    boost::container::static_vector<char, 4> will_property_length_buf_;
    properties will_props_;

    as::const_buffer will_topic_name_;
    boost::container::static_vector<char, 2> will_topic_name_length_buf_;
    as::const_buffer will_message_;
    boost::container::static_vector<char, 2> will_message_length_buf_;

    as::const_buffer user_name_;
    boost::container::static_vector<char, 2> user_name_length_buf_;
    as::const_buffer password_;
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
        std::uint8_t reason_code,
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
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // connect acknowledge flags
              1 +                   // reason code
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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
    std::size_t num_of_const_buffer_sequence() const {
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

    std::uint8_t reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;

    std::size_t num_of_const_buffer_sequence_;
};

template <std::size_t PacketIdBytes>
class basic_publish_message {
public:
    basic_publish_message(
        as::const_buffer const& topic_name,
        std::uint8_t qos,
        bool retain,
        bool dup,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props,
        as::const_buffer const& payload
    )
        : fixed_header_(make_fixed_header(control_packet_type::publish, 0b0000)),
          topic_name_(topic_name),
          topic_name_length_buf_ { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_name)) },
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          payload_(payload),
          remaining_length_(publish_remaining_length(topic_name, qos, payload)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // topic name length
              1 +                   // topic name
              qos == qos::at_most_once ? 0U : 1U + // packet id
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::num_of_const_buffer_sequence(pv);
                  }
              ) +
              1                     // payload
          )

    {
        utf8string_check(string_view(get_pointer(topic_name), get_size(topic_name)));
        publish::set_qos(fixed_header_, qos);
        publish::set_retain(fixed_header_, retain);
        publish::set_dup(fixed_header_, dup);

        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ += property_length_buf_.size() + property_length_;

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        if (qos == qos::at_least_once ||
            qos == qos::exactly_once) {
            packet_id_.reserve(PacketIdBytes);
            add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        }
    }

    template <typename Iterator>
    basic_publish_message(Iterator b, Iterator e) {
        if (b >= e) throw remaining_length_error();
        fixed_header_ = static_cast<std::uint8_t>(*b);
        auto qos = publish::get_qos(fixed_header_);
        ++b;

        if (b >= e) throw remaining_length_error();
        auto len_consumed = remaining_length(b, e);
        remaining_length_ = std::get<0>(len_consumed);
        auto consumed = std::get<1>(len_consumed);

        std::copy(b, b + static_cast<typename std::iterator_traits<Iterator>::difference_type>(consumed), std::back_inserter(remaining_length_buf_));
        b += static_cast<typename std::iterator_traits<Iterator>::difference_type>(consumed);

        if (b + 2 >= e) throw remaining_length_error();
        std::copy(b, b + 2, std::back_inserter(topic_name_length_buf_));
        auto topic_name_length = make_uint16_t(b, b + 2);
        b += 2;

        if (b + topic_name_length >= e) throw remaining_length_error();
        utf8string_check(string_view(&*b, topic_name_length));
        topic_name_ = as::buffer(&*b, topic_name_length);
        b += topic_name_length;

        switch (qos) {
        case qos::at_most_once:
            break;
        case qos::at_least_once:
        case qos::exactly_once:
            if (b + PacketIdBytes >= e) throw remaining_length_error();
            std::copy(b, b + PacketIdBytes, std::back_inserter(packet_id_));
            b += PacketIdBytes;
            break;
        default:
            throw protocol_error();
            break;
        };

        auto len_consume = variable_length(
            b,
            std::min(b + 4, e)
        );
        property_length_ = std::get<0>(len_consume);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }
        auto consume = std::get<1>(len_consume);
        if (consume == 0) throw property_length_error();
        b += static_cast<typename std::iterator_traits<Iterator>::difference_type>(consume);
        auto prop_end = b +
            static_cast<typename std::iterator_traits<Iterator>::difference_type>(property_length_);

        props_ = property::parse(b, e);
        if (b != prop_end) throw property_length_error();
        payload_ = as::buffer(&*b, static_cast<std::size_t>(std::distance(b, e)));
        num_of_const_buffer_sequence_ =
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // topic name length
            1 +                   // topic name
            qos == qos::at_most_once ? 0U : 1U + // packet id
            1 +                   // property length
            std::accumulate(
                props_.begin(),
                props_.end(),
                0U,
                [](std::size_t total, property_variant const& pv) {
                    return total + v5::num_of_const_buffer_sequence(pv);
                }
            ) +
            1;                    // payload
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
        ret.emplace_back(topic_name_);

        if (!packet_id_.empty()) {
            ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));
        }

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        ret.emplace_back(payload_);

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

        ret.append(get_pointer(payload_), get_size(payload_));

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
     * @brief Get qos
     * @return qos
     */
    std::uint8_t qos() const {
        return publish::get_qos(fixed_header_);
    }

    /**
     * @brief Check retain flag
     * @return true if retain, otherwise return false.
     */
    bool is_retain() const {
        return publish::is_retain(fixed_header_);
    }

    /**
     * @brief Check dup flag
     * @return true if dup, otherwise return false.
     */
    bool is_dup() const {
        return publish::is_dup(fixed_header_);
    }

    /**
     * @brief Get topic name
     * @return topic name
     */
    as::const_buffer topic() const {
        return topic_name_;
    }

    /**
     * @brief Get payload
     * @return payload
     */
    as::const_buffer payload() const {
        return payload_;
    }

    /**
     * @brief Set dup flag
     * @param dup flag value to set
     */
    void set_dup(bool dup) {
        publish::set_dup(fixed_header_, dup);
    }


private:
    static std::size_t publish_remaining_length(
        as::const_buffer const& topic_name,
        std::uint8_t qos,
        as::const_buffer const& payload) {
        return
            2                      // topic name length
            + get_size(topic_name) // topic name
            + get_size(payload)    // payload
            + [&] () -> typename packet_id_type<PacketIdBytes>::type {
                  if (qos == qos::at_least_once || qos == qos::exactly_once) {
                      return PacketIdBytes; // packet_id
                  }
                  else {
                      return 0;
                  }
              }();
    }

private:
    std::uint8_t fixed_header_;
    as::const_buffer topic_name_;
    boost::container::static_vector<char, 2> topic_name_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    as::const_buffer payload_;
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
        mqtt::optional<std::uint8_t> reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::puback, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              (
                  reason_code ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            (
                reason_code ?
                (
                    1 +                   // reason code
                    property_length_buf_.size() +
                    property_length_
                )
                : 0
            );

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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));
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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

            ret.append(property_length_buf_.data(), property_length_buf_.size());

            auto it = ret.end();
            ret.resize(sz);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
        }
        return ret;
    }


    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    mqtt::optional<std::uint8_t> reason_code_;
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
        mqtt::optional<std::uint8_t> reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubrec, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              (
                  reason_code ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            (
                reason_code ?
                (
                    1 +                   // reason code
                    property_length_buf_.size() +
                    property_length_
                )
                : 0
            );

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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));
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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

            ret.append(property_length_buf_.data(), property_length_buf_.size());

            auto it = ret.end();
            ret.resize(sz);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
        }
        return ret;
    }


    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    mqtt::optional<std::uint8_t> reason_code_;
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
        mqtt::optional<std::uint8_t> reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubrel, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              (
                  reason_code ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            (
                reason_code ?
                (
                    1 +                   // reason code
                    property_length_buf_.size() +
                    property_length_
                )
                : 0
            );

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
    }

    template <typename Iterator>
    basic_pubrel_message(Iterator b, Iterator e) {
        if (b >= e) throw remaining_length_error();
        fixed_header_ = static_cast<std::uint8_t>(*b);
        ++b;

        if (b >= e) throw remaining_length_error();
        auto len_consumed = remaining_length(b, e);
        remaining_length_ = std::get<0>(len_consumed);
        auto consumed = static_cast<
            typename std::iterator_traits<Iterator>::difference_type
        >(std::get<1>(len_consumed));

        std::copy(b, b + consumed, std::back_inserter(remaining_length_buf_));
        b += consumed;

        if (b + PacketIdBytes > e) throw remaining_length_error();
        std::copy(b, b + PacketIdBytes, std::back_inserter(packet_id_));
        b += PacketIdBytes;

        if (b >= e) {
            num_of_const_buffer_sequence_ =
                1 +                   // fixed header
                1 +                   // remaining length
                1;                    // packet id
            return;
        }

        reason_code_ = *b++;

        auto len_consume = variable_length(
            b,
            std::min(b + 4, e)
        );
        property_length_ = std::get<0>(len_consume);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }
        auto consume = std::get<1>(len_consume);
        if (consume == 0) throw property_length_error();
        b += static_cast<typename std::iterator_traits<Iterator>::difference_type>(consume);
        auto prop_end = b +
            static_cast<typename std::iterator_traits<Iterator>::difference_type>(property_length_);

        props_ = property::parse(b, e);
        if (b != prop_end) throw property_length_error();
        num_of_const_buffer_sequence_ =
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            (
                reason_code_ ?
                (
                    1 +                   // reason code
                    1 +                   // property length
                    std::accumulate(
                        props_.begin(),
                        props_.end(),
                        0U,
                        [](std::size_t total, property_variant const& pv) {
                            return total + v5::num_of_const_buffer_sequence(pv);
                        }
                    )
                )
                : 0
            );
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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));
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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

            ret.append(property_length_buf_.data(), property_length_buf_.size());

            auto it = ret.end();
            ret.resize(sz);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
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

    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    mqtt::optional<std::uint8_t> reason_code_;
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
        mqtt::optional<std::uint8_t> reason_code,
        properties props)
        : fixed_header_(make_fixed_header(control_packet_type::pubcomp, 0b0000)),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              (
                  reason_code ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
                          [](std::size_t total, property_variant const& pv) {
                              return total + v5::num_of_const_buffer_sequence(pv);
                          }
                      )
                  )
                  : 0
              )
          )
    {
        add_packet_id_to_buf<PacketIdBytes>::apply(packet_id_, packet_id);
        auto pb = variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ =
            PacketIdBytes +       // packet id
            (
                reason_code ?
                (
                    1 +                   // reason code
                    property_length_buf_.size() +
                    property_length_
                )
                : 0
            );

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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));
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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

            ret.append(property_length_buf_.data(), property_length_buf_.size());

            auto it = ret.end();
            ret.resize(sz);
            auto end = ret.end();
            for (auto const& p : props_) {
                v5::fill(p, it, end);
                it += static_cast<std::string::difference_type>(v5::size(p));
            }
        }
        return ret;
    }


    std::uint8_t fixed_header_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
    boost::container::static_vector<char, PacketIdBytes> packet_id_;
    mqtt::optional<std::uint8_t> reason_code_;
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
        entry(as::const_buffer const& topic_filter, std::uint8_t options)
            : topic_filter(topic_filter),
              topic_filter_length_buf { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_filter)) },
              options(options)
        {}

        as::const_buffer topic_filter;
        boost::container::static_vector<char, 2> topic_filter_length_buf;
        std::uint8_t options;
    };

public:
    basic_subscribe_message(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> params,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::subscribe, 0b0010)),
          remaining_length_(PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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

        for (auto const& e : params) {
            auto const& topic_filter = std::get<0>(e);
            utf8string_check(string_view(get_pointer(topic_filter), get_size(topic_filter)));

            auto qos = std::get<1>(e);
            entries_.emplace_back(topic_filter, qos);
            remaining_length_ +=
                2 +                     // topic filter length
                get_size(topic_filter) +  // topic filter
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

        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        for (auto const& p : props_) {
            v5::add_const_buffer_sequence(ret, p);
        }

        for (auto const& e : entries_) {
            utf8string_check(string_view(get_pointer(e.topic_filter), get_size(e.topic_filter)));
            ret.emplace_back(as::buffer(e.topic_filter_length_buf.data(), e.topic_filter_length_buf.size()));
            ret.emplace_back(e.topic_filter);
            ret.emplace_back(as::buffer(&e.options, 1));
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
            ret.append(e.topic_filter_length_buf.data(), e.topic_filter_length_buf.size());
            ret.append(get_pointer(e.topic_filter), get_size(e.topic_filter));
            ret.push_back(static_cast<char>(e.options));
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
        std::vector<std::uint8_t> reason_codes,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::suback, 0b0000)),
          remaining_length_(reason_codes.size() + PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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
    std::size_t num_of_const_buffer_sequence() const {
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
        entry(as::const_buffer const& topic_filter)
            : topic_filter(topic_filter),
              topic_filter_length_buf { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_filter)) }
        {}

        as::const_buffer topic_filter;
        boost::container::static_vector<char, 2> topic_filter_length_buf;
    };

public:
    basic_unsubscribe_message(
        std::vector<as::const_buffer> const& params,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::unsubscribe, 0b0010)),
          remaining_length_(PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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

        for (auto const& e : params) {
            utf8string_check(string_view(get_pointer(e), get_size(e)));
            entries_.emplace_back(e);
            remaining_length_ +=
                2 +          // topic filter length
                get_size(e); // topic filter
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
            utf8string_check(string_view(get_pointer(e.topic_filter), get_size(e.topic_filter)));
            ret.emplace_back(as::buffer(e.topic_filter_length_buf.data(), e.topic_filter_length_buf.size()));
            ret.emplace_back(e.topic_filter);
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
            ret.append(e.topic_filter_length_buf.data(), e.topic_filter_length_buf.size());
            ret.append(get_pointer(e.topic_filter), get_size(e.topic_filter));
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
        std::vector<std::uint8_t> const& reason_codes,
        typename packet_id_type<PacketIdBytes>::type packet_id,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::unsuback, 0b0000)),
          remaining_length_(reason_codes.size() + PacketIdBytes),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              1 +                   // packet id
              1 +                   // property length
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
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
    std::size_t num_of_const_buffer_sequence() const {
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
        mqtt::optional<std::uint8_t> reason_code,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::disconnect, 0b0000)),
          remaining_length_(0),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              (
                  reason_code ? (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
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
        if (reason_code) {
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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));

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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

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

    mqtt::optional<std::uint8_t> reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

struct auth_message {
    auth_message(
        mqtt::optional<std::uint8_t> reason_code,
        properties props
    )
        : fixed_header_(make_fixed_header(control_packet_type::auth, 0b0000)),
          remaining_length_(0),
          reason_code_(reason_code),
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  0U,
                  [](std::size_t total, property_variant const& pv) {
                      return total + v5::size(pv);
                  }
              )
          ),
          props_(std::move(props)),
          num_of_const_buffer_sequence_(
              1 +                   // fixed header
              1 +                   // remaining length
              (
                  reason_code ?
                  (
                      1 +                   // reason code
                      1 +                   // property length
                      std::accumulate(
                          props.begin(),
                          props.end(),
                          0U,
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
        if (reason_code) {
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

        if (reason_code_) {
            ret.emplace_back(as::buffer(&reason_code_.value(), 1));

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
    std::size_t num_of_const_buffer_sequence() const {
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

        if (reason_code_) {
            ret.push_back(static_cast<char>(reason_code_.value()));

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

    mqtt::optional<std::uint8_t> reason_code_;

    std::size_t property_length_;
    boost::container::static_vector<char, 4> property_length_buf_;
    properties props_;
    std::size_t num_of_const_buffer_sequence_;
};

} // namespace v5

} // namespace mqtt

#endif // MQTT_V5_MESSAGE_HPP

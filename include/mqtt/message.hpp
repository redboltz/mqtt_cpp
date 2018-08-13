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

namespace mqtt {

namespace as = boost::asio;

namespace detail {

inline void utf8string_check(string_view str) {
    if (!utf8string::is_valid_length(str)) throw utf8string_length_error();
    auto r = utf8string::validate_contents(str);
    if (r != utf8string::validation::well_formed) {
        throw utf8string_contents_error(r);
    }
}


class header_only_message {
public:
    /**
     * @brief Create empty header_packet_id_message.
     */
    header_only_message(std::uint8_t type, std::uint8_t flags)
        : message_ { make_fixed_header(type, flags), 0 }
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

class header_packet_id_message {
public:
    /**
     * @brief Create empty header_packet_id_message.
     */
    header_packet_id_message(std::uint8_t type, std::uint8_t flags, std::uint16_t packet_id)
        : message_ { make_fixed_header(type, flags), 2, MQTT_16BITNUM_TO_BYTE_SEQ(packet_id) }
    {}

    template <typename Iterator>
    header_packet_id_message(Iterator b, Iterator e) {
        if (std::distance(b, e) != 4) throw remaining_length_error();
        if (b[1] != 2) throw remaining_length_error();

        std::copy(b, e, std::back_inserter(message_));
    }

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
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        return std::string(message_.data(), message_.size());
    }
protected:
    boost::container::static_vector<char, 4> const& message() const {
        return message_;
    }

private:
    boost::container::static_vector<char, 4> message_;
};

} // namespace detail

struct puback_message : detail::header_packet_id_message {
    puback_message(std::uint16_t packet_id)
        : detail::header_packet_id_message(control_packet_type::puback, 0b0000, packet_id)
    {}
};

struct pubrec_message : detail::header_packet_id_message {
    pubrec_message(std::uint16_t packet_id)
        : detail::header_packet_id_message(control_packet_type::pubrec, 0b0000, packet_id)
    {}
};

struct pubrel_message : detail::header_packet_id_message {
    pubrel_message(std::uint16_t packet_id)
        : detail::header_packet_id_message(control_packet_type::pubrel, 0b0010, packet_id)
    {
    }

    template <typename Iterator>
    pubrel_message(Iterator b, Iterator e)
        : header_packet_id_message(b, e)
    {
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    std::uint16_t packet_id() const {
        return make_uint16_t(message().begin() + 2, message().end());
    }
};

struct pubcomp_message : detail::header_packet_id_message {
    pubcomp_message(std::uint16_t packet_id)
        : detail::header_packet_id_message(control_packet_type::pubcomp, 0b0000, packet_id)
    {}
};

struct unsuback_message : detail::header_packet_id_message {
    unsuback_message(std::uint16_t packet_id)
        : detail::header_packet_id_message(control_packet_type::unsuback, 0b0000, packet_id)
    {}
};

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

struct disconnect_message : detail::header_only_message {
    disconnect_message()
        : detail::header_only_message(control_packet_type::disconnect, 0b0000)
    {}
};

class connack_message {
public:
    connack_message(bool session_present, std::uint8_t return_code)
        : message_ {
              make_fixed_header(control_packet_type::connack, 0b0000),
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
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string continuous_buffer() const {
        return std::string(message_.data(), message_.size());
    }

private:
    boost::container::static_vector<char, 4> message_;
};

// variable length messages

class connect_message {
public:
    connect_message(
        std::uint16_t keep_alive_sec,
        std::string const& client_id,
        bool clean_session,
        boost::optional<will> const& w,
        boost::optional<std::string> const& user_name,
        boost::optional<std::string> const& password
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
          protocol_name_and_level_ { 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04 },
          client_id_(as::buffer(client_id)),
          client_id_length_buf_{ MQTT_16BITNUM_TO_BYTE_SEQ(client_id.size()) },
          keep_alive_buf_ { MQTT_16BITNUM_TO_BYTE_SEQ(keep_alive_sec ) }
    {
        detail::utf8string_check(client_id);
        if (clean_session) connect_flags_ |= connect_flags::clean_session;
        if (user_name) {
            detail::utf8string_check(user_name.get());
            connect_flags_ |= connect_flags::user_name_flag;
            user_name_ = as::buffer(user_name.get());
            add_uint16_t_to_buf(user_name_length_buf_, static_cast<std::uint16_t>(get_size(user_name_)));

            remaining_length_ += 2 + get_size(user_name_);
        }
        if (password) {
            connect_flags_ |= connect_flags::password_flag;
            password_ = as::buffer(password.get());
            add_uint16_t_to_buf(password_length_buf_, static_cast<std::uint16_t>(get_size(password_)));

            remaining_length_ += 2 + get_size(password_);
        }
        if (w) {
            connect_flags_ |= connect_flags::will_flag;
            if (w.get().retain()) connect_flags_ |= connect_flags::will_retain;
            connect_flags::set_will_qos(connect_flags_, w.get().qos());

            detail::utf8string_check(w.get().topic());
            will_topic_name_ = as::buffer(w.get().topic());
            add_uint16_t_to_buf(
                will_topic_name_length_buf_,
                static_cast<std::uint16_t>(get_size(will_topic_name_))
            );
            if (w.get().message().size() > 0xffffL) throw will_message_length_error();
            will_message_ = as::buffer(w.get().message());
            add_uint16_t_to_buf(
                will_message_length_buf_,
                static_cast<std::uint16_t>(get_size(will_message_)));

            remaining_length_ += 2 + get_size(will_topic_name_) + 2 + get_size(will_message_);
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
        ret.reserve(
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // protocol name and level
            1 +                   // connect flags
            1 +                   // keep alive

            2 +                   // client id length, client id

            2 +                   // will topic name length, will topic name
            2 +                   // will message length, will message
            2 +                   // user name length, user name
            2                     // password length, password
        );

        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));
        ret.emplace_back(as::buffer(protocol_name_and_level_.data(), protocol_name_and_level_.size()));
        ret.emplace_back(as::buffer(&connect_flags_, 1));
        ret.emplace_back(as::buffer(keep_alive_buf_.data(), keep_alive_buf_.size()));

        ret.emplace_back(as::buffer(client_id_length_buf_.data(), client_id_length_buf_.size()));
        ret.emplace_back(client_id_);

        if (connect_flags::has_will_flag(connect_flags_)) {
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
        return 1 + remaining_length_buf_.size() + remaining_length_;
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

        ret.push_back(fixed_header_);
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());
        ret.append(protocol_name_and_level_.data(), protocol_name_and_level_.size());
        ret.push_back(connect_flags_);
        ret.append(keep_alive_buf_.data(), keep_alive_buf_.size());

        ret.append(client_id_length_buf_.data(), client_id_length_buf_.size());
        ret.append(get_pointer(client_id_), get_size(client_id_));

        if (connect_flags::has_will_flag(connect_flags_)) {
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
    char fixed_header_;
    char connect_flags_;

    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;

    boost::container::static_vector<char, 7> protocol_name_and_level_;
    as::const_buffer client_id_;
    boost::container::static_vector<char, 2> client_id_length_buf_;

    as::const_buffer will_topic_name_;
    boost::container::static_vector<char, 2> will_topic_name_length_buf_;
    as::const_buffer will_message_;
    boost::container::static_vector<char, 2> will_message_length_buf_;

    as::const_buffer user_name_;
    boost::container::static_vector<char, 2> user_name_length_buf_;
    as::const_buffer password_;
    boost::container::static_vector<char, 2> password_length_buf_;

    boost::container::static_vector<char, 2> keep_alive_buf_;
};

class publish_message {
public:
    publish_message(
        as::const_buffer const& topic_name,
        std::uint8_t qos,
        bool retain,
        bool dup,
        std::uint16_t packet_id,
        as::const_buffer const& payload
    )
        : fixed_header_(make_fixed_header(control_packet_type::publish, 0b0000)),
          topic_name_(topic_name),
          topic_name_length_buf_ { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_name)) },
          payload_(payload),
          remaining_length_(publish_remaining_length(topic_name, qos, payload))
    {
        detail::utf8string_check(string_view(get_pointer(topic_name), get_size(topic_name)));
        publish::set_qos(fixed_header_, qos);
        publish::set_retain(fixed_header_, retain);
        publish::set_dup(fixed_header_, dup);

        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        if (qos == qos::at_least_once ||
            qos == qos::exactly_once) {
            packet_id_.reserve(2);
            add_uint16_t_to_buf(packet_id_, packet_id);
        }
    }

    template <typename Iterator>
    publish_message(Iterator b, Iterator e) {
        if (b >= e) throw remaining_length_error();
        fixed_header_ = *b;
        auto qos = publish::get_qos(fixed_header_);
        ++b;

        if (b + 4 >= e) throw remaining_length_error();
        auto len_consumed = remaining_length(b, b + 4);
        remaining_length_ = std::get<0>(len_consumed);
        auto consumed = std::get<1>(len_consumed);

        std::copy(b, b + consumed, std::back_inserter(remaining_length_buf_));
        b += consumed;

        if (b + 2 >= e) throw remaining_length_error();
        std::copy(b, b + 2, std::back_inserter(topic_name_length_buf_));
        auto topic_name_length = make_uint16_t(b, b + 2);
        b += 2;

        if (b + topic_name_length >= e) throw remaining_length_error();
        detail::utf8string_check(string_view(&*b, topic_name_length));
        topic_name_ = as::buffer(&*b, topic_name_length);
        b += topic_name_length;

        switch (qos) {
        case qos::at_most_once:
            break;
        case qos::at_least_once:
        case qos::exactly_once:
            if (b + 2 >= e) throw remaining_length_error();
            std::copy(b, b + 2, std::back_inserter(packet_id_));
            b += 2;
            break;
        default:
            throw protocol_error();
            break;
        };

        payload_ = as::buffer(&*b, std::distance(b, e));
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        if (packet_id_.empty()) {
            return
                {
                    as::buffer(&fixed_header_, 1),
                    as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()),
                    as::buffer(topic_name_length_buf_.data(), topic_name_length_buf_.size()),
                    topic_name_,
                    payload_
                };
        }
        else {
            return
                {
                    as::buffer(&fixed_header_, 1),
                    as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()),
                    as::buffer(topic_name_length_buf_.data(), topic_name_length_buf_.size()),
                    topic_name_,
                    as::buffer(packet_id_.data(), packet_id_.size()),
                    payload_
                };
        }
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + remaining_length_buf_.size() + remaining_length_;
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

        ret.push_back(fixed_header_);
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(topic_name_length_buf_.data(), topic_name_length_buf_.size());
        ret.append(get_pointer(topic_name_), get_size(topic_name_));

        ret.append(packet_id_.data(), packet_id_.size());
        ret.append(get_pointer(payload_), get_size(payload_));

        return ret;
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    std::uint16_t packet_id() const {
        return make_uint16_t(packet_id_.begin(), packet_id_.end());
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
            + [&] {
                  if (qos == qos::at_least_once || qos == qos::exactly_once) {
                      return 2; // packet_id
                  }
                  else {
                      return 0;
                  }
              }();
    }

private:
    char fixed_header_;
    as::const_buffer topic_name_;
    boost::container::static_vector<char, 2> topic_name_length_buf_;
    boost::container::static_vector<char, 2> packet_id_;
    as::const_buffer payload_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

class subscribe_message {
private:
    struct entry {
        entry(as::const_buffer const& topic_name, std::uint8_t qos)
            : topic_name(topic_name),
              topic_name_length_buf { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_name)) },
              qos(qos)
        {}

        as::const_buffer topic_name;
        boost::container::static_vector<char, 2> topic_name_length_buf;
        std::uint8_t qos;
    };

public:
    subscribe_message(
        std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
        std::uint16_t packet_id
    )
        : fixed_header_(static_cast<char>(make_fixed_header(control_packet_type::subscribe, 0b0010))),
          packet_id_ { MQTT_16BITNUM_TO_BYTE_SEQ(packet_id) },
          remaining_length_(2)
    {
        for (auto const& e : params) {
            auto const& topic_name = std::get<0>(e);
            detail::utf8string_check(string_view(get_pointer(topic_name), get_size(topic_name)));

            auto qos = std::get<1>(e);
            entries_.emplace_back(topic_name, qos);
            remaining_length_ +=
                2 +                     // topic name length
                get_size(topic_name) +  // topic name
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
        ret.reserve(
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            entries_.size() * 3   // topic name length, topic name, qos
        );

        ret.emplace_back(as::buffer(&fixed_header_, 1));

        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

        ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

        for (auto const& e : entries_) {
            detail::utf8string_check(string_view(get_pointer(e.topic_name), get_size(e.topic_name)));
            ret.emplace_back(as::buffer(e.topic_name_length_buf.data(), e.topic_name_length_buf.size()));
            ret.emplace_back(e.topic_name);
            ret.emplace_back(as::buffer(&e.qos, 1));
        }

        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + remaining_length_buf_.size() + remaining_length_;
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

        ret.push_back(fixed_header_);
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(packet_id_.data(), packet_id_.size());

        for (auto const& e : entries_) {
            ret.append(e.topic_name_length_buf.data(), e.topic_name_length_buf.size());
            ret.append(get_pointer(e.topic_name), get_size(e.topic_name));
            ret.push_back(e.qos);
        }


        return ret;
    }

private:
    char fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, 2> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 2> remaining_length_buf_;
};

class suback_message {
public:
    suback_message(
        std::vector<std::uint8_t> const& params,
        std::uint16_t packet_id
    )
        : fixed_header_(static_cast<char>(make_fixed_header(control_packet_type::suback, 0b0000))),
          packet_id_ { MQTT_16BITNUM_TO_BYTE_SEQ(packet_id) },
          remaining_length_(params.size() + 2)
    {
        auto rb = remaining_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
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
        ret.reserve(4); // fixed header, remaining length, packet_id, entries

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
        return 1 + remaining_length_buf_.size() + remaining_length_;
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

        ret.push_back(fixed_header_);
        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        ret.append(packet_id_.data(), packet_id_.size());
        ret.append(entries_);

        return ret;
    }

private:
    char fixed_header_;
    std::string entries_;
    boost::container::static_vector<char, 2> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

class unsubscribe_message {
private:
    struct entry {
        entry(as::const_buffer const& topic_name)
            : topic_name(topic_name),
              topic_name_length_buf { MQTT_16BITNUM_TO_BYTE_SEQ(get_size(topic_name)) }
        {}

        as::const_buffer topic_name;
        boost::container::static_vector<char, 2> topic_name_length_buf;
    };

public:
    unsubscribe_message(
        std::vector<as::const_buffer> const& params,
        std::uint16_t packet_id
    )
        : fixed_header_(static_cast<char>(make_fixed_header(control_packet_type::unsubscribe, 0b0010))),
          packet_id_ { MQTT_16BITNUM_TO_BYTE_SEQ(packet_id) },
          remaining_length_(2)
    {
        for (auto const& e : params) {
            detail::utf8string_check(string_view(get_pointer(e), get_size(e)));
            entries_.emplace_back(e);
            remaining_length_ +=
                2 +          // topic name length
                get_size(e); // topic name
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
        ret.reserve(
            1 +                   // fixed header
            1 +                   // remaining length
            1 +                   // packet id
            entries_.size() * 2   // topic name length, topic name
        );

        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

        ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

        for (auto const& e : entries_) {
            detail::utf8string_check(string_view(get_pointer(e.topic_name), get_size(e.topic_name)));
            ret.emplace_back(as::buffer(e.topic_name_length_buf.data(), e.topic_name_length_buf.size()));
            ret.emplace_back(e.topic_name);
        }


        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + remaining_length_buf_.size() + remaining_length_;
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

        ret.push_back(fixed_header_);

        ret.append(remaining_length_buf_.data(), remaining_length_buf_.size());

        for (auto const& e : entries_) {
            ret.append(e.topic_name_length_buf.data(), e.topic_name_length_buf.size());
            ret.append(get_pointer(e.topic_name), get_size(e.topic_name));
        }

        ret.append(packet_id_.data(), packet_id_.size());

        return ret;
    }

private:
    char fixed_header_;
    std::vector<entry> entries_;
    boost::container::static_vector<char, 2> packet_id_;
    std::size_t remaining_length_;
    boost::container::static_vector<char, 4> remaining_length_buf_;
};

} // namespace mqtt

#endif // MQTT_MESSAGE_HPP

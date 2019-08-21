// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_HPP)
#define MQTT_PROPERTY_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <mqtt/optional.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/container/static_vector.hpp>

#include <mqtt/two_byte_util.hpp>
#include <mqtt/const_buffer_util.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/string_check.hpp>
#include <mqtt/property_id.hpp>
#include <mqtt/four_byte_util.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/qos.hpp>
#include <mqtt/variable_length.hpp>
#include <mqtt/buffer.hpp>
namespace mqtt {

namespace as = boost::asio;

namespace v5 {

namespace property {

namespace detail {

enum class ostream_format {
    direct,
    int_cast,
    key_val,
    binary_string,
};

template <std::size_t N>
struct n_bytes_property {
    explicit n_bytes_property(property::id id)
        :id_(id) {}

    template <typename It>
    n_bytes_property(property::id id, It b, It e)
        :id_(id), buf_(std::move(b), std::move(e)) {}

    n_bytes_property(property::id id, boost::container::static_vector<char, N> buf)
        :id_(id), buf_(std::move(buf)) {}

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(buf_.data(), buf_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(buf_.begin(), buf_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + buf_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, N> buf_;
};

struct binary_property {
    binary_property(property::id id, buffer buf)
        :id_(id),
         buf_(std::move(buf)),
         length_{ num_to_2bytes(boost::numeric_cast<std::uint16_t>(buf_.size())) } {
             if (buf_.size() > 0xffff) throw property_length_error();
         }

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(length_.data(), length_.size()));
        v.emplace_back(as::buffer(buf_.data(), buf_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        using dt = typename It::difference_type;

        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(length_.begin(), length_.end(), b);
        std::advance(b, static_cast<dt>(length_.size()));
        std::copy(buf_.begin(), buf_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + length_.size() + buf_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    buffer const& val() const {
        return buf_;
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    buffer buf_;
    boost::container::static_vector<char, 2> length_;
};

struct string_property : binary_property {
    string_property(property::id id, buffer buf, bool already_checked)
        :binary_property(id, std::move(buf)) {
        if (!already_checked) {
            auto r = utf8string::validate_contents(this->val());
            if (r != utf8string::validation::well_formed) throw utf8string_contents_error(r);
        }
    }
};

struct variable_property {
    variable_property(property::id id, std::size_t value)
        :id_(id)  {
        variable_push(value_, value);
    }

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(value_.data(), value_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(value_.begin(), value_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + value_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    std::size_t val() const {
        return std::get<0>(variable_length(value_));
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, 4> value_;
};

} // namespace detail

class payload_format_indicator : public detail::n_bytes_property<1> {
public:
    using recv = payload_format_indicator;
    using store = payload_format_indicator;
    enum payload_format {
        binary,
        string
    };

    payload_format_indicator(payload_format fmt = binary)
        : detail::n_bytes_property<1>(id::payload_format_indicator, { fmt == binary ? char(0) : char(1) } ) {}

    template <typename It>
    payload_format_indicator(It b, It e)
        : detail::n_bytes_property<1>(id::payload_format_indicator, b, e) {}

    payload_format val() const {
        return
            [this] {
                if (buf_.front() == 0) return binary;
                else return string;
            }();
    }

    static constexpr detail::ostream_format const of_ = detail::ostream_format::binary_string;
};


class message_expiry_interval : public detail::n_bytes_property<4> {
public:
    using recv = message_expiry_interval;
    using store = message_expiry_interval;
    message_expiry_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::message_expiry_interval, num_to_4bytes(val) ) {}

    template <typename It>
    message_expiry_interval(It b, It e)
        : detail::n_bytes_property<4>(id::message_expiry_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};

class content_type : public detail::string_property {
public:
    explicit content_type(buffer val, bool already_checked = false)
        : detail::string_property(id::content_type, std::move(val), already_checked) {}
};

class response_topic : public detail::string_property {
public:
    explicit response_topic(buffer val, bool already_checked = false)
        : detail::string_property(id::response_topic, std::move(val), already_checked) {}
};

class correlation_data : public detail::string_property {
public:
    explicit correlation_data(buffer val, bool already_checked = false)
        : detail::string_property(id::correlation_data, std::move(val), already_checked) {}
};

class subscription_identifier : public detail::variable_property {
public:
    using recv = subscription_identifier;
    using store = subscription_identifier;
    subscription_identifier(std::size_t subscription_id)
        : detail::variable_property(id::subscription_identifier, subscription_id) {}
};

class session_expiry_interval : public detail::n_bytes_property<4> {
public:
    using recv = session_expiry_interval;
    using store = session_expiry_interval;
    session_expiry_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::session_expiry_interval, { num_to_4bytes(val) } ) {}

    template <typename It>
    session_expiry_interval(It b, It e)
        : detail::n_bytes_property<4>(id::session_expiry_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};

class assigned_client_identifier : public detail::string_property {
public:
    explicit assigned_client_identifier(buffer val, bool already_checked = false)
        : detail::string_property(id::assigned_client_identifier, std::move(val), already_checked) {}
};

class server_keep_alive : public detail::n_bytes_property<2> {
public:
    using recv = server_keep_alive;
    using store = server_keep_alive;
    server_keep_alive(std::uint16_t val)
        : detail::n_bytes_property<2>(id::server_keep_alive, { num_to_2bytes(val) } ) {}

    template <typename It>
    server_keep_alive(It b, It e)
        : detail::n_bytes_property<2>(id::server_keep_alive, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};

class authentication_method : public detail::string_property {
public:
    explicit authentication_method(buffer val, bool already_checked = false)
        : detail::string_property(id::authentication_method, std::move(val), already_checked) {}
};

class authentication_data : public detail::binary_property {
public:
    explicit authentication_data(buffer val)
        : detail::binary_property(id::authentication_data, std::move(val)) {}
};

class request_problem_information : public detail::n_bytes_property<1> {
public:
    using recv = request_problem_information;
    using store = request_problem_information;
    request_problem_information(bool value)
        : detail::n_bytes_property<1>(id::request_problem_information, { value ? char(1) : char(0) } ) {}

    template <typename It>
    request_problem_information(It b, It e)
        : detail::n_bytes_property<1>(id::request_problem_information, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};

class will_delay_interval : public detail::n_bytes_property<4> {
public:
    using recv = will_delay_interval;
    using store = will_delay_interval;
    will_delay_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::will_delay_interval, { num_to_4bytes(val) } ) {}

    template <typename It>
    will_delay_interval(It b, It e)
        : detail::n_bytes_property<4>(id::will_delay_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};

class request_response_information : public detail::n_bytes_property<1> {
public:
    using recv = request_response_information;
    using store = request_response_information;
    request_response_information(bool value)
        : detail::n_bytes_property<1>(id::request_response_information, { value ? char(1) : char(0) } ) {}

    template <typename It>
    request_response_information(It b, It e)
        : detail::n_bytes_property<1>(id::request_response_information, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};

class response_information : public detail::string_property {
public:
    explicit response_information(buffer val, bool already_checked = false)
        : detail::string_property(id::response_information, std::move(val), already_checked) {}
};

class server_reference : public detail::string_property {
public:
    explicit server_reference(buffer val, bool already_checked = false)
        : detail::string_property(id::server_reference, std::move(val), already_checked) {}
};

class reason_string : public detail::string_property {
public:
    explicit reason_string(buffer val, bool already_checked = false)
        : detail::string_property(id::reason_string, std::move(val), already_checked) {}
};

class receive_maximum : public detail::n_bytes_property<2> {
public:
    using recv = receive_maximum;
    using store = receive_maximum;
    receive_maximum(std::uint16_t val)
        : detail::n_bytes_property<2>(id::receive_maximum, { num_to_2bytes(val) } ) {}

    template <typename It>
    receive_maximum(It b, It e)
        : detail::n_bytes_property<2>(id::receive_maximum, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};


class topic_alias_maximum : public detail::n_bytes_property<2> {
public:
    using recv = topic_alias_maximum;
    using store = topic_alias_maximum;
    topic_alias_maximum(std::uint16_t val)
        : detail::n_bytes_property<2>(id::topic_alias_maximum, { num_to_2bytes(val) } ) {}

    template <typename It>
    topic_alias_maximum(It b, It e)
        : detail::n_bytes_property<2>(id::topic_alias_maximum, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};


class topic_alias : public detail::n_bytes_property<2> {
public:
    using recv = topic_alias;
    using store = topic_alias;
    topic_alias(std::uint16_t val)
        : detail::n_bytes_property<2>(id::topic_alias, { num_to_2bytes(val) } ) {}

    template <typename It>
    topic_alias(It b, It e)
        : detail::n_bytes_property<2>(id::topic_alias, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};

class maximum_qos : public detail::n_bytes_property<1> {
public:
    using recv = maximum_qos;
    using store = maximum_qos;
    maximum_qos(std::uint8_t qos)
        : detail::n_bytes_property<1>(id::maximum_qos, { static_cast<char>(qos) } ) {
        if (qos != qos::at_most_once &&
            qos != qos::at_least_once &&
            qos != qos::exactly_once) throw property_parse_error();
    }

    template <typename It>
    maximum_qos(It b, It e)
        : detail::n_bytes_property<1>(id::maximum_qos, b, e) {}

    std::uint8_t val() const {
        return static_cast<std::uint8_t>(buf_.front());
    }

    static constexpr const detail::ostream_format of_ = detail::ostream_format::int_cast;
};

class retain_available : public detail::n_bytes_property<1> {
public:
    using recv = retain_available;
    using store = retain_available;
    retain_available(bool value)
        : detail::n_bytes_property<1>(id::retain_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    retain_available(It b, It e)
        : detail::n_bytes_property<1>(id::retain_available, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};


class user_property {
public:
    user_property(buffer key, buffer val, bool key_already_checked = false, bool val_already_checked = false)
        : key_(std::move(key), key_already_checked), val_(std::move(val), val_already_checked) {}

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(key_.len.data(), key_.len.size()));
        v.emplace_back(as::buffer(key_.buf.data(), key_.buf.size()));
        v.emplace_back(as::buffer(val_.len.data(), val_.len.size()));
        v.emplace_back(as::buffer(val_.buf.data(), val_.buf.size()));
    }

    template <typename It>
    void fill(It b, It e) const {
        using dt = typename It::difference_type;
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());

        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        {
            std::copy(key_.len.begin(), key_.len.end(), b);
            std::advance(b, static_cast<dt>(key_.len.size()));
            auto ptr = key_.buf.data();
            auto size = key_.buf.size();
            std::copy(ptr, std::next(ptr, static_cast<dt>(size)), b);
            std::advance(b, static_cast<dt>(size));
        }
        {
            std::copy(val_.len.begin(), val_.len.end(), b);
            std::advance(b, static_cast<dt>(val_.len.size()));
            auto ptr = val_.buf.data();
            auto size = val_.buf.size();
            std::copy(ptr, std::next(ptr, static_cast<dt>(size)), b);
            std::advance(b, static_cast<dt>(size));
        }
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 + // id_
            key_.size() +
            val_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 + // header
            2 + // key (len, buf)
            2;  // val (len, buf)
    }

    buffer const& key() const {
        return key_.buf;
    }

    buffer const& val() const {
        return val_.buf;
    }

    static constexpr detail::ostream_format const of_ = detail::ostream_format::key_val;

private:
    struct len_str {
        explicit len_str(buffer b, bool already_checked = false)
            : buf(std::move(b)),
              len{ num_to_2bytes(boost::numeric_cast<std::uint16_t>(buf.size())) }
        {
            if (!already_checked) {
                auto r = utf8string::validate_contents(buf);
                if (r != utf8string::validation::well_formed) throw utf8string_contents_error(r);
            }
        }

        std::size_t size() const {
            return len.size() + buf.size();
        }
        buffer buf;
        boost::container::static_vector<char, 2> len;
    };

private:
    property::id id_ = id::user_property;
    len_str key_;
    len_str val_;
};

class maximum_packet_size : public detail::n_bytes_property<4> {
public:
    using recv = maximum_packet_size;
    using store = maximum_packet_size;
    maximum_packet_size(std::uint32_t val)
        : detail::n_bytes_property<4>(id::maximum_packet_size, { num_to_4bytes(val) } ) {}

    template <typename It>
    maximum_packet_size(It b, It e)
        : detail::n_bytes_property<4>(id::maximum_packet_size, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};


class wildcard_subscription_available : public detail::n_bytes_property<1> {
public:
    using recv = wildcard_subscription_available;
    using store = wildcard_subscription_available;
    wildcard_subscription_available(bool value)
        : detail::n_bytes_property<1>(id::wildcard_subscription_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    wildcard_subscription_available(It b, It e)
        : detail::n_bytes_property<1>(id::wildcard_subscription_available, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};


class subscription_identifier_available : public detail::n_bytes_property<1> {
public:
    using recv = subscription_identifier_available;
    using store = subscription_identifier_available;
    subscription_identifier_available(bool value)
        : detail::n_bytes_property<1>(id::subscription_identifier_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    subscription_identifier_available(It b, It e)
        : detail::n_bytes_property<1>(id::subscription_identifier_available, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};


class shared_subscription_available : public detail::n_bytes_property<1> {
public:
    using recv = shared_subscription_available;
    using store = shared_subscription_available;
    shared_subscription_available(bool value)
        : detail::n_bytes_property<1>(id::shared_subscription_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    shared_subscription_available(It b, It e)
        : detail::n_bytes_property<1>(id::shared_subscription_available, b, e) {}

    bool val() const {
        return buf_.front() == 1;
    }
};

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::direct,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << p.val();
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::int_cast,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << static_cast<int>(p.val());
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::key_val,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << p.key() << ':' << p.val();
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::binary_string,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << (p.val() == payload_format_indicator::binary ? "binary" : "string");
    return o;
}


} // namespace property
} // namespace v5
} // namespace mqtt

#endif // MQTT_PROPERTY_HPP
